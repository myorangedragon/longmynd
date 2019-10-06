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

static const uint32_t crc32_mpeg2_table[256];

static uint32_t crc32_mpeg2(uint8_t *data_ptr, size_t length)
{
    uint32_t crc;

    crc = 0xFFFFFFFF;
    while (length--)
    {
        crc = (crc << 8) ^ crc32_mpeg2_table[((crc >> 24) ^ *data_ptr++) & 0xFF];
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
    uint32_t ts_adaption_field_flag;
    uint32_t ts_adaption_field_length;
    uint32_t ts_payload_content_offset;
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
    uint32_t ts_pmt_offset;
    uint32_t ts_pmt_index;

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
                /* Align input to the TS sync byte */
                ts_buffer_length_remaining = ts_buffer_length - (&ts_packet_ptr[0] - &ts_buffer[0]);

                if(ts_buffer_length_remaining <= TS_PACKET_SIZE)
                {
                    /* Nothing more in buffer, force exit */
                    ts_packet_ptr = NULL;
                    continue;
                }

                ts_packet_ptr = memchr(ts_packet_ptr, TS_HEADER_SYNC, ts_buffer_length_remaining - TS_PACKET_SIZE);
                if(ts_packet_ptr == NULL)
                {
                    continue;
                }
            }

            ts_pid = (uint32_t)((ts_packet_ptr[1] & 0x1F) << 8) | (uint32_t)ts_packet_ptr[2];
        
            ts_packet_total_count++;
            
            ts_payload_content_offset = 4;

            ts_adaption_field_flag = (uint32_t)(ts_packet_ptr[3] & 0x20) >> 5;
            if(ts_adaption_field_flag > 0)
            {
                ts_adaption_field_length = ts_packet_ptr[4];

                if(ts_adaption_field_length == 0
                    || ts_adaption_field_length > 183)
                {
                    /* Length invalid, packet is likely invalid */
                    ts_packet_ptr++;
                    continue;
                }

                ts_payload_content_offset += ts_adaption_field_length;
            }
            
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
                ts_payload_ptr = (uint8_t *)&ts_packet_ptr[ts_payload_content_offset + 1 + ts_packet_ptr[ts_payload_content_offset]];

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
                    /* CRC Fail */
                    ts_packet_ptr++;
                    continue;
                }

                ts_pat_programs_count = (ts_payload_section_length - 9) / 4;

                /* For now, only read the first programme */
                /* TODO: Read all programs here to enable PID parsing of PMT */
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
                ts_payload_ptr = (uint8_t *)&ts_packet_ptr[ts_payload_content_offset + 1 + ts_packet_ptr[ts_payload_content_offset]];

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
                    /* CRC Fail */
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
                ts_payload_ptr = (uint8_t *)&ts_packet_ptr[ts_payload_content_offset + 1 + ts_packet_ptr[ts_payload_content_offset]];

                /* We're not filtering by PID here yet, so we rely on filtering by table ID */
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
                    /* CRC Fail */
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

                ts_pmt_offset = 0;
                ts_pmt_index = 0;
                while((12+1+ts_pmt_program_info_length+ts_pmt_offset) < ts_payload_section_length)
                {
                    ts_pmt_es_ptr = &ts_payload_ptr[12 + ts_pmt_program_info_length + ts_pmt_offset];

                    /* For each elementary PID */
                    ts_pmt_es_type = (uint32_t)ts_pmt_es_ptr[0];

                    ts_pmt_es_pid = ((uint32_t)(ts_pmt_es_ptr[1] & 0x1F) << 8) | (uint32_t)ts_pmt_es_ptr[2];

                    ts_pmt_es_info_length = ((uint32_t)(ts_pmt_es_ptr[3] & 0x0F) << 8) | (uint32_t)ts_pmt_es_ptr[4];
                    //if(ts_pmt_es_info_length > 0)
                    //{
                        //printf(" - - PMT ES Info: %.*s\n", ts_pmt_es_info_length, &ts_pmt_es_ptr[5]);
                    //}

                    pthread_mutex_lock(&status->mutex);

                    status->ts_elementary_streams[ts_pmt_index][0] = ts_pmt_es_pid;
                    status->ts_elementary_streams[ts_pmt_index][1] = ts_pmt_es_type;

                    pthread_mutex_unlock(&status->mutex);

                    ts_pmt_offset += (5 + ts_pmt_es_info_length);
                    ts_pmt_index++;
                }

                ts_packet_ptr++;
                continue;
            }

            ts_packet_ptr++;
        }
        pthread_mutex_lock(&status->mutex);

        if(ts_packet_total_count > 0)
        {
            status->ts_null_percentage = (100 * ts_packet_null_count) / ts_packet_total_count;
        }

        /* Trigger pthread signal */
        pthread_cond_signal(&status->signal);

        pthread_mutex_unlock(&status->mutex);
    }

    free(ts_buffer);

    return NULL;
}

static const uint32_t crc32_mpeg2_table[256] = {
    0x00000000, 0x04c11db7, 0x09823b6e, 0x0d4326d9, 0x130476dc, 0x17c56b6b, 0x1a864db2, 0x1e475005,
    0x2608edb8, 0x22c9f00f, 0x2f8ad6d6, 0x2b4bcb61, 0x350c9b64, 0x31cd86d3, 0x3c8ea00a, 0x384fbdbd,
    0x4c11db70, 0x48d0c6c7, 0x4593e01e, 0x4152fda9, 0x5f15adac, 0x5bd4b01b, 0x569796c2, 0x52568b75,
    0x6a1936c8, 0x6ed82b7f, 0x639b0da6, 0x675a1011, 0x791d4014, 0x7ddc5da3, 0x709f7b7a, 0x745e66cd,
    0x9823b6e0, 0x9ce2ab57, 0x91a18d8e, 0x95609039, 0x8b27c03c, 0x8fe6dd8b, 0x82a5fb52, 0x8664e6e5,
    0xbe2b5b58, 0xbaea46ef, 0xb7a96036, 0xb3687d81, 0xad2f2d84, 0xa9ee3033, 0xa4ad16ea, 0xa06c0b5d,
    0xd4326d90, 0xd0f37027, 0xddb056fe, 0xd9714b49, 0xc7361b4c, 0xc3f706fb, 0xceb42022, 0xca753d95,
    0xf23a8028, 0xf6fb9d9f, 0xfbb8bb46, 0xff79a6f1, 0xe13ef6f4, 0xe5ffeb43, 0xe8bccd9a, 0xec7dd02d,
    0x34867077, 0x30476dc0, 0x3d044b19, 0x39c556ae, 0x278206ab, 0x23431b1c, 0x2e003dc5, 0x2ac12072,
    0x128e9dcf, 0x164f8078, 0x1b0ca6a1, 0x1fcdbb16, 0x018aeb13, 0x054bf6a4, 0x0808d07d, 0x0cc9cdca,
    0x7897ab07, 0x7c56b6b0, 0x71159069, 0x75d48dde, 0x6b93dddb, 0x6f52c06c, 0x6211e6b5, 0x66d0fb02,
    0x5e9f46bf, 0x5a5e5b08, 0x571d7dd1, 0x53dc6066, 0x4d9b3063, 0x495a2dd4, 0x44190b0d, 0x40d816ba,
    0xaca5c697, 0xa864db20, 0xa527fdf9, 0xa1e6e04e, 0xbfa1b04b, 0xbb60adfc, 0xb6238b25, 0xb2e29692,
    0x8aad2b2f, 0x8e6c3698, 0x832f1041, 0x87ee0df6, 0x99a95df3, 0x9d684044, 0x902b669d, 0x94ea7b2a,
    0xe0b41de7, 0xe4750050, 0xe9362689, 0xedf73b3e, 0xf3b06b3b, 0xf771768c, 0xfa325055, 0xfef34de2,
    0xc6bcf05f, 0xc27dede8, 0xcf3ecb31, 0xcbffd686, 0xd5b88683, 0xd1799b34, 0xdc3abded, 0xd8fba05a,
    0x690ce0ee, 0x6dcdfd59, 0x608edb80, 0x644fc637, 0x7a089632, 0x7ec98b85, 0x738aad5c, 0x774bb0eb,
    0x4f040d56, 0x4bc510e1, 0x46863638, 0x42472b8f, 0x5c007b8a, 0x58c1663d, 0x558240e4, 0x51435d53,
    0x251d3b9e, 0x21dc2629, 0x2c9f00f0, 0x285e1d47, 0x36194d42, 0x32d850f5, 0x3f9b762c, 0x3b5a6b9b,
    0x0315d626, 0x07d4cb91, 0x0a97ed48, 0x0e56f0ff, 0x1011a0fa, 0x14d0bd4d, 0x19939b94, 0x1d528623,
    0xf12f560e, 0xf5ee4bb9, 0xf8ad6d60, 0xfc6c70d7, 0xe22b20d2, 0xe6ea3d65, 0xeba91bbc, 0xef68060b,
    0xd727bbb6, 0xd3e6a601, 0xdea580d8, 0xda649d6f, 0xc423cd6a, 0xc0e2d0dd, 0xcda1f604, 0xc960ebb3,
    0xbd3e8d7e, 0xb9ff90c9, 0xb4bcb610, 0xb07daba7, 0xae3afba2, 0xaafbe615, 0xa7b8c0cc, 0xa379dd7b,
    0x9b3660c6, 0x9ff77d71, 0x92b45ba8, 0x9675461f, 0x8832161a, 0x8cf30bad, 0x81b02d74, 0x857130c3,
    0x5d8a9099, 0x594b8d2e, 0x5408abf7, 0x50c9b640, 0x4e8ee645, 0x4a4ffbf2, 0x470cdd2b, 0x43cdc09c,
    0x7b827d21, 0x7f436096, 0x7200464f, 0x76c15bf8, 0x68860bfd, 0x6c47164a, 0x61043093, 0x65c52d24,
    0x119b4be9, 0x155a565e, 0x18197087, 0x1cd86d30, 0x029f3d35, 0x065e2082, 0x0b1d065b, 0x0fdc1bec,
    0x3793a651, 0x3352bbe6, 0x3e119d3f, 0x3ad08088, 0x2497d08d, 0x2056cd3a, 0x2d15ebe3, 0x29d4f654,
    0xc5a92679, 0xc1683bce, 0xcc2b1d17, 0xc8ea00a0, 0xd6ad50a5, 0xd26c4d12, 0xdf2f6bcb, 0xdbee767c,
    0xe3a1cbc1, 0xe760d676, 0xea23f0af, 0xeee2ed18, 0xf0a5bd1d, 0xf464a0aa, 0xf9278673, 0xfde69bc4,
    0x89b8fd09, 0x8d79e0be, 0x803ac667, 0x84fbdbd0, 0x9abc8bd5, 0x9e7d9662, 0x933eb0bb, 0x97ffad0c,
    0xafb010b1, 0xab710d06, 0xa6322bdf, 0xa2f33668, 0xbcb4666d, 0xb8757bda, 0xb5365d03, 0xb1f740b4
};