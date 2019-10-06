/* -------------------------------------------------------------------------------------------------- */
/* The LongMynd receiver: ts.c                                                                        */
/* Copyright 2019 Heather Lomond                                                                      */
/* -------------------------------------------------------------------------------------------------- */
/*
    This file is part of longmynd.

    Longmynd is free software: you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation, either version 3 of the License, or
    (at your option) any later version.

    Longmynd is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with longmynd.  If not, see <https://www.gnu.org/licenses/>.
*/

#include <string.h>

#include "main.h"
#include "errors.h"
#include "udp.h"
#include "fifo.h"
#include "ftdi.h"
#include "ftdi_usb.h"
#include "ts.h"

#define TS_FRAME_SIZE 20*512 // 512 is base USB FTDI frame

#define MAX_PID  8192

#define TS_PACKET_SIZE 188
#define TS_HEADER_SYNC 0x47

#define TS_PID_PAT 0x0000
#define TS_PID_SDT 0x0011
#define TS_PID_NULL 0x1FFF

#define TS_TABLE_PAT 0x00
#define TS_TABLE_PMT 0x02
#define TS_TABLE_SDT 0x42

uint8_t *ts_buffer_ptr = NULL;
bool ts_buffer_waiting;

typedef struct {
    uint8_t *buffer;
    uint32_t length;
    bool waiting;
    pthread_mutex_t mutex;
    pthread_cond_t signal;
} longmynd_ts_parse_buffer_t;

static longmynd_ts_parse_buffer_t longmynd_ts_parse_buffer = {
    .buffer = NULL,
    .length = 0,
    .waiting = false,
    .mutex = PTHREAD_MUTEX_INITIALIZER,
    .signal = PTHREAD_COND_INITIALIZER
};


/* -------------------------------------------------------------------------------------------------- */
void *loop_ts(void *arg) {
/* -------------------------------------------------------------------------------------------------- */
/* Runs a loop to query the Minitiouner TS endpoint, and output it to the requested interface         */
/* -------------------------------------------------------------------------------------------------- */
    thread_vars_t *thread_vars=(thread_vars_t *)arg;
    uint8_t *err = &thread_vars->thread_err;
    longmynd_config_t *config = thread_vars->config;

    uint8_t *buffer;
    uint16_t len=0;
    uint8_t (*ts_write)(uint8_t*,uint32_t);

    *err=ERROR_NONE;

    buffer = malloc(TS_FRAME_SIZE);
    if(buffer == NULL)
    {
        *err=ERROR_TS_BUFFER_MALLOC;
    }

    if(thread_vars->config->ts_use_ip) {
        *err=udp_ts_init(thread_vars->config->ts_ip_addr, thread_vars->config->ts_ip_port);
        ts_write = udp_ts_write;
    } else {
        *err=fifo_ts_init(thread_vars->config->ts_fifo_path);
        ts_write = fifo_ts_write;
    }

    while(*err == ERROR_NONE && *thread_vars->main_err_ptr == ERROR_NONE){
        /* If reset flag is active (eg. just started or changed station), then clear out the ts buffer */
        if(config->ts_reset) {
            do {
                if (*err==ERROR_NONE) *err=ftdi_usb_ts_read(buffer, &len, TS_FRAME_SIZE);
            } while (*err==ERROR_NONE && len>2);
           config->ts_reset = false; 
        }

        *err=ftdi_usb_ts_read(buffer, &len, TS_FRAME_SIZE);

        /* if there is ts data then we send it out to the required output. But, we have to lose the first 2 bytes */
        /* that are the usual FTDI 2 byte response and not part of the TS */
        if ((*err==ERROR_NONE) && (len>2)) {
            ts_write(&buffer[2],len-2);

            if(longmynd_ts_parse_buffer.waiting && longmynd_ts_parse_buffer.buffer != NULL)
            {                
                pthread_mutex_lock(&longmynd_ts_parse_buffer.mutex);

                memcpy(longmynd_ts_parse_buffer.buffer, &buffer[2],len-2);
                longmynd_ts_parse_buffer.length = len-2;
                pthread_cond_signal(&longmynd_ts_parse_buffer.signal);
                longmynd_ts_parse_buffer.waiting = false;

                pthread_mutex_unlock(&longmynd_ts_parse_buffer.mutex);
            }
        }
    }

    free(buffer);

    return NULL;
}

static uint32_t crc32_mpeg2(uint8_t *buffer, size_t length)
{
    size_t i, j;
    uint32_t crc, msb;

    crc = 0xFFFFFFFF;
    for(i = 0; i < length; i++)
    {
        crc ^= (((uint32_t)buffer[i])<<24);
        for (j = 0; j < 8; j++)
        {
            msb = crc>>31;
            crc <<= 1;
            crc ^= (0 - msb) & 0x04C11DB7;
        }
    }
    return crc;
}

/* -------------------------------------------------------------------------------------------------- */
void *loop_ts_parse(void *arg) {
/* -------------------------------------------------------------------------------------------------- */
/* Runs a loop to parse the MPEG-TS                                                                   */
/* -------------------------------------------------------------------------------------------------- */
    thread_vars_t *thread_vars=(thread_vars_t *)arg;
    uint8_t *err = &thread_vars->thread_err;
    *err=ERROR_NONE;
    //longmynd_config_t *config = thread_vars->config;
    longmynd_status_t *status = thread_vars->status;

    /* TS Processing Vars */
    uint8_t *ts_buffer;
    uint32_t ts_buffer_length;
    uint8_t *ts_packet_ptr;
    uint32_t ts_buffer_length_remaining;

    /* TS Stats Vars */
    uint32_t ts_packet_total_count;
    uint32_t ts_packet_null_count;

    /* Generic TS */
    uint32_t ts_pid;
    uint32_t ts_payload_content_length;
    uint8_t *ts_payload_ptr;
    uint32_t ts_payload_section_length;

    uint32_t ts_payload_crc;
    uint32_t ts_payload_crc_c;

    /* PAT */
    //uint32_t ts_pat_programs_count;
    //uint32_t ts_pat_program_id;
    //uint32_t ts_pat_program_pid;

    /* PMT */
    //uint32_t ts_pmt_pcr_pid;
    uint32_t ts_pmt_program_info_length;
    uint8_t *ts_pmt_es_ptr;
    uint32_t ts_pmt_es_type;
    uint32_t ts_pmt_es_pid;
    uint32_t ts_pmt_es_info_length;

    /* SDT */
    uint8_t *ts_packet_sdt_table_ptr;
    //uint32_t service_id;
    uint8_t *ts_packet_sdt_descriptor_ptr;
    //uint32_t descriptor_tag;
    //uint32_t descriptor_length;
    uint32_t service_provider_name_length;
    uint32_t service_name_length;

    ts_buffer = malloc(TS_FRAME_SIZE);
    if(ts_buffer == NULL)
    {
        *err=ERROR_TS_BUFFER_MALLOC;
    }

    longmynd_ts_parse_buffer.buffer = ts_buffer;

    while(*err == ERROR_NONE && *thread_vars->main_err_ptr == ERROR_NONE)
    {
        //ts_pat_program_pid = 0x00; // Updated by PAT parse

        /* Reset Stats */
        ts_packet_total_count = 0;
        ts_packet_null_count = 0;

        pthread_mutex_lock(&longmynd_ts_parse_buffer.mutex);
        longmynd_ts_parse_buffer.waiting = true;

        while(longmynd_ts_parse_buffer.waiting)
        {
            pthread_cond_wait(&longmynd_ts_parse_buffer.signal, &longmynd_ts_parse_buffer.mutex);
        }

        pthread_mutex_unlock(&longmynd_ts_parse_buffer.mutex);

        ts_packet_ptr = &ts_buffer[0];
        ts_buffer_length = longmynd_ts_parse_buffer.length;
        ts_buffer_length_remaining = ts_buffer_length;

        while(ts_packet_ptr != NULL)
        {
            if(ts_packet_ptr[0] != TS_HEADER_SYNC)
            {
                /* Re-align input to the TS sync byte */
                ts_packet_ptr = memchr(ts_packet_ptr, TS_HEADER_SYNC, ts_buffer_length_remaining - TS_PACKET_SIZE);
                if(ts_packet_ptr == NULL)
                {
                    continue;
                }

                ts_buffer_length_remaining = ts_buffer_length - (ts_packet_ptr - ts_buffer);
            }
        
            ts_packet_total_count++;

            ts_pid = (uint32_t)((ts_packet_ptr[1] & 0x1F) << 8) | (uint32_t)ts_packet_ptr[2];
            
            /* NULL/padding packets */
            if(ts_pid == TS_PID_NULL)
            {
                ts_packet_null_count++;
                ts_packet_ptr++;
                continue;
            }

#if 0
            if(ts_pid == TS_PID_PAT)
            {
                ts_payload_ptr = (uint8_t *)&ts_packet_ptr[4 + 1 + ts_packet_ptr[4]];

                if(ts_payload_ptr[0] != TS_TABLE_PAT)
                {
                    ts_packet_ptr++;
                    continue;
                }

                ts_payload_section_length = ((uint32_t)(ts_payload_ptr[1] & 0x0F) << 8) | (uint32_t)ts_payload_ptr[2];

                if(ts_payload_section_length < 1)
                {
                    ts_packet_ptr++;
                    continue;
                }

                ts_payload_crc = ((uint32_t)ts_payload_ptr[ts_payload_section_length-1] << 24) | ((uint32_t)ts_payload_ptr[ts_payload_section_length] << 16)
                                | ((uint32_t)ts_payload_ptr[ts_payload_section_length+1] << 8) | (uint32_t)ts_payload_ptr[ts_payload_section_length+2];

                ts_payload_crc_c = crc32_mpeg2(ts_payload_ptr, (ts_payload_section_length-1));

                if(ts_payload_crc != ts_payload_crc_c)
                {
                    //printf(" - CRC FAIL (%"PRIx32"/%"PRIx32")\n", ts_payload_crc_c, ts_payload_crc);
                    ts_packet_ptr++;
                    continue;
                }

                ts_pat_programs_count = (ts_payload_section_length - 9) / 4;

                /* For now, only read the first programme */
                if(ts_pat_programs_count > 0)
                {
                    //ts_pat_program_id = ((uint32_t)ts_payload_ptr[8] << 8) | (uint32_t)ts_payload_ptr[9];
                    ts_pat_program_pid = ((uint32_t)(ts_payload_ptr[10] & 0x1F) << 8) | (uint32_t)ts_payload_ptr[11];
                    //printf(" - PAT Program PID: %"PRIu32"\n", ts_pat_program_pid);
                }

                ts_packet_ptr++;
                continue;
            }
#endif
            if(ts_pid == TS_PID_SDT)
            {
                ts_payload_content_length = 0;
                ts_payload_ptr = (uint8_t *)&ts_packet_ptr[4 + 1 + ts_packet_ptr[4]];

                if(ts_payload_ptr[0] != TS_TABLE_SDT)
                {
                    ts_packet_ptr++;
                    continue;
                }

                ts_payload_section_length = ((uint32_t)(ts_payload_ptr[1] & 0x0F) << 8) | (uint32_t)ts_payload_ptr[2];
                //printf(" - SDT Section Length: %"PRIu32"\n", ts_payload_section_length);

                if(ts_payload_section_length < 1)
                {
                    ts_packet_ptr++;
                    continue;
                }

                ts_payload_crc = ((uint32_t)ts_payload_ptr[ts_payload_section_length-1] << 24) | ((uint32_t)ts_payload_ptr[ts_payload_section_length] << 16)
                                | ((uint32_t)ts_payload_ptr[ts_payload_section_length+1] << 8) | (uint32_t)ts_payload_ptr[ts_payload_section_length+2];

                ts_payload_crc_c = crc32_mpeg2(ts_payload_ptr, (ts_payload_section_length-1));

                if(ts_payload_crc != ts_payload_crc_c)
                {
                    //printf(" - CRC FAIL (%"PRIx32"/%"PRIx32")\n", ts_payload_crc_c, ts_payload_crc);
                    ts_packet_ptr++;
                    continue;
                }

                /* Per service */
                ts_packet_sdt_table_ptr = &ts_payload_ptr[11];
                ts_payload_content_length += 11;

                //service_id = ((uint32_t)ts_packet_sdt_table_ptr[0] << 8) | (uint32_t)ts_packet_sdt_table_ptr[1];
                //printf(" - - Service ID: %"PRIu32"\n", service_id);

                /* Per descriptor */
                ts_packet_sdt_descriptor_ptr = &ts_packet_sdt_table_ptr[5];
                ts_payload_content_length += 5;

                //descriptor_tag = (uint32_t)ts_packet_sdt_descriptor_ptr[0];
                //printf(" - - - Descriptor Tag: %"PRIu32"\n", descriptor_tag);

                //descriptor_length = (uint32_t)ts_packet_sdt_descriptor_ptr[1];
                //printf(" - - - Descriptor Length: %"PRIu32"\n", descriptor_length);

                //uint32_t service_type = (uint32_t)ts_packet_sdt_descriptor_ptr[2];
                //printf(" - - - Service Type %"PRIu32"\n", service_type);

                ts_payload_content_length += 3;

                service_provider_name_length = (uint32_t)ts_packet_sdt_descriptor_ptr[3];
                //printf(" - - - Service Provider Name Length %"PRIu32"\n", service_provider_name_length);
                //printf(" - - - Service Provider Name: %.*s\n", service_provider_name_length, &ts_packet_sdt_descriptor_ptr[4]);

                service_name_length = (uint32_t)ts_packet_sdt_descriptor_ptr[3+1+service_provider_name_length];
                //printf(" - - - Service Name Length %"PRIu32"\n", service_name_length);
                //printf(" - - - Service Name: %.*s\n", service_name_length, &ts_packet_sdt_descriptor_ptr[4+1+service_provider_name_length]);

                pthread_mutex_lock(&status->mutex);
                
                memcpy(status->service_name, &ts_packet_sdt_descriptor_ptr[4+1+service_provider_name_length], service_name_length);
                status->service_name[service_name_length] = '\0';

                memcpy(status->service_provider_name, &ts_packet_sdt_descriptor_ptr[4], service_provider_name_length);
                status->service_provider_name[service_provider_name_length] = '\0';

                /* Trigger pthread signal */
                pthread_cond_signal(&status->signal);
                pthread_mutex_unlock(&status->mutex);

                ts_payload_content_length += 1;
                ts_payload_content_length += service_provider_name_length;
                ts_payload_content_length += 1;
                ts_payload_content_length += service_name_length;

                ts_packet_ptr++;
                continue;
            }
            else // if(ts_pat_program_pid !=0x00 && ts_pid == ts_pat_program_pid) /* PMT, once found in PAT */
            {
                ts_payload_ptr = (uint8_t *)&ts_packet_ptr[4 + 1 + ts_packet_ptr[4]];

                if(ts_payload_ptr[0] != TS_TABLE_PMT)
                {
                    ts_packet_ptr++;
                    continue;
                }

                ts_payload_section_length = ((uint32_t)(ts_payload_ptr[1] & 0x0F) << 8) | (uint32_t)ts_payload_ptr[2];

                if(ts_payload_section_length < 1)
                {
                    ts_packet_ptr++;
                    continue;
                }

                ts_payload_crc = ((uint32_t)ts_payload_ptr[ts_payload_section_length-1] << 24) | ((uint32_t)ts_payload_ptr[ts_payload_section_length] << 16)
                                | ((uint32_t)ts_payload_ptr[ts_payload_section_length+1] << 8) | (uint32_t)ts_payload_ptr[ts_payload_section_length+2];

                ts_payload_crc_c = crc32_mpeg2(ts_payload_ptr, (ts_payload_section_length-1));

                if(ts_payload_crc != ts_payload_crc_c)
                {
                    //printf(" - CRC FAIL (%"PRIx32"/%"PRIx32")\n", ts_payload_crc_c, ts_payload_crc);
                    ts_packet_ptr++;
                    continue;
                }

                //ts_pmt_pcr_pid = ((uint32_t)(ts_payload_ptr[8] & 0x1F) << 8) | (uint32_t)ts_payload_ptr[9];
                //printf(" - PMT: PCR PID: %"PRIu32"\n", ts_pmt_pcr_pid);

                ts_pmt_program_info_length = ((uint32_t)(ts_payload_ptr[10] & 0x0F) << 8) | (uint32_t)ts_payload_ptr[11];
                //if(ts_pmt_program_info_length > 0)
                //{
                //    printf(" - PMT Program Info: %.*s\n", ts_pmt_program_info_length, &ts_payload_ptr[12]);
                //}

                uint32_t offset = 0, index = 0;
                while((12+1+ts_pmt_program_info_length+offset) < ts_payload_section_length)
                {
                    ts_pmt_es_ptr = &ts_payload_ptr[12 + ts_pmt_program_info_length + offset];

                    /* For each elementary PID */
                    ts_pmt_es_type = (uint32_t)ts_pmt_es_ptr[0];

                    ts_pmt_es_pid = ((uint32_t)(ts_pmt_es_ptr[1] & 0x1F) << 8) | (uint32_t)ts_pmt_es_ptr[2];

                    ts_pmt_es_info_length = ((uint32_t)(ts_pmt_es_ptr[3] & 0x0F) << 8) | (uint32_t)ts_pmt_es_ptr[4];
                    if(ts_pmt_es_info_length > 0)
                    {
                        //printf(" - - PMT ES Info: %.*s\n", ts_pmt_es_info_length, &ts_pmt_es_ptr[5]);
                    }

                    pthread_mutex_lock(&status->mutex);

                    status->ts_elementary_streams[index][0] = ts_pmt_es_pid;
                    status->ts_elementary_streams[index][1] = ts_pmt_es_type;

                    pthread_mutex_unlock(&status->mutex);

                    offset += (5 + ts_pmt_es_info_length);
                    index++;
                }

                pthread_mutex_lock(&status->mutex);

                for(; index < NUM_ELEMENT_STREAMS; index++)
                {
                    status->ts_elementary_streams[index][0] = 0x00;
                    status->ts_elementary_streams[index][1] = 0x00;
                }

                /* Trigger pthread signal */
                pthread_cond_signal(&status->signal);
                pthread_mutex_unlock(&status->mutex);

                ts_packet_ptr++;
                continue;
            }

            ts_packet_ptr++;
        }

        pthread_mutex_lock(&status->mutex);
        
        status->ts_null_percentage = (100 * ts_packet_null_count) / ts_packet_total_count;

        /* Trigger pthread signal */
        pthread_cond_signal(&status->signal);
        pthread_mutex_unlock(&status->mutex);
    }

    free(ts_buffer);

    return NULL;
}