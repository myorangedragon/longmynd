/* -------------------------------------------------------------------------------------------------- */
/* The LongMynd receiver: main.c                                                                      */
/*    - an implementation of the Serit NIM controlling software for the MiniTiouner Hardware          */
/*    - the top level (main) and command line procesing                                               */
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

/* -------------------------------------------------------------------------------------------------- */
/* ----------------- INCLUDES ----------------------------------------------------------------------- */
/* -------------------------------------------------------------------------------------------------- */

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <stdint.h>
#include <stdbool.h>
#include <string.h>
#include <pthread.h>
#include "ftdi.h"
#include "stv0910.h"
#include "stv0910_regs.h"
#include "stv0910_utils.h"
#include "stv6120.h"
#include "stvvglna.h"
#include "nim.h"
#include "errors.h"
#include "fifo.h"
#include "ftdi_usb.h"
#include "udp.h"

/* -------------------------------------------------------------------------------------------------- */
/* ----------------- DEFINES ------------------------------------------------------------------------ */
/* -------------------------------------------------------------------------------------------------- */

/* states of the main loop state machine */
#define STATE_INIT               0
#define STATE_DEMOD_HUNTING      1
#define STATE_DEMOD_FOUND_HEADER 2
#define STATE_DEMOD_S            3
#define STATE_DEMOD_S2           4

/* define the various status reports */
#define STATUS_STATE               1
#define STATUS_LNA_GAIN            2
#define STATUS_PUNCTURE_RATE       3
#define STATUS_POWER_I             4 
#define STATUS_POWER_Q             5
#define STATUS_CARRIER_FREQUENCY   6
#define STATUS_CONSTELLATION_I     7
#define STATUS_CONSTELLATION_Q     8
#define STATUS_SYMBOL_RATE         9
#define STATUS_VITERBI_ERROR_RATE 10
#define STATUS_BER                11
#define STATUS_MER                12


/* The number of constellation peeks we do for each background loop */
#define NUM_CONSTELLATIONS 16

/* Milliseconds between each status report loop */
#define STATUS_LOOP_MS  100

/* -------------------------------------------------------------------------------------------------- */
/* ----------------- GLOBALS ------------------------------------------------------------------------ */
/* -------------------------------------------------------------------------------------------------- */

uint8_t buffer[FTDI_USB_TS_FRAME_SIZE]; /* holds teh TS stream data */
bool lna_ok; /* set to true when LNAs are detected */
uint32_t freq;  /* the requested frequency required for status reporting */

typedef struct {
    bool port_swap;
    uint32_t freq_requested;
    uint32_t sr_requested;

    uint8_t device_usb_bus;
    uint8_t device_usb_addr;

    bool ts_use_ip;
    char ts_fifo_path[128];
    char ts_ip_addr[16];
    int ts_ip_port;

    bool status_use_ip;
    char status_fifo_path[128];
    char status_ip_addr[16];
    int status_ip_port;
} longmynd_config_t;

longmynd_config_t longmynd_config;

/* -------------------------------------------------------------------------------------------------- */
/* ----------------- ROUTINES ----------------------------------------------------------------------- */
/* -------------------------------------------------------------------------------------------------- */

uint64_t timestamp_ms(void) {
    struct timespec tp;

    if(clock_gettime(CLOCK_REALTIME, &tp) != 0)
    {
        return 0;
    }

    return (uint64_t) tp.tv_sec * 1000 + tp.tv_nsec / 1000000;
}

/* -------------------------------------------------------------------------------------------------- */
uint8_t process_command_line(int argc, char *argv[], longmynd_config_t *config) {
/* -------------------------------------------------------------------------------------------------- */
/* processes the command line arguments, sets up the parameters in main from them and error checks    */
/* All the required parameters are passed in                                                          */
/* return: error code                                                                                 */
/* -------------------------------------------------------------------------------------------------- */
    uint8_t err=ERROR_NONE;
    uint8_t param;
    bool main_usb_set=false;
    bool ts_ip_set=false;
    bool ts_fifo_set=false;
    bool status_ip_set=false;
    bool status_fifo_set=false;

    /* Defaults */
    config->port_swap = false;
    config->device_usb_addr = 0;
    config->device_usb_bus = 0;
    config->ts_use_ip = false;
    strcpy(config->ts_fifo_path, "longmynd_main_ts");
    config->status_use_ip = false;
    strcpy(config->status_fifo_path, "longmynd_main_status");

    param=1;
    while (param<argc-2) {
        if (argv[param][0]=='-') {
          switch (argv[param++][1]) {
            case 'u':
                config->device_usb_bus =(uint8_t)strtol(argv[param++],NULL,10);
                config->device_usb_addr=(uint8_t)strtol(argv[param  ],NULL,10);
                main_usb_set=true;
                break;
            case 'i':
                strncpy(config->ts_ip_addr,argv[param++], 16);
                config->ts_ip_port=(uint16_t)strtol(argv[param],NULL,10);
                config->ts_use_ip=true;
                ts_ip_set = true;
                break;
            case 't':
                strncpy(config->status_fifo_path, argv[param], 128);
                ts_fifo_set=true;
                break;
            case 'I':
                strncpy(config->status_ip_addr,argv[param++], 16);
                config->status_ip_port=(uint16_t)strtol(argv[param],NULL,10);
                config->status_use_ip=true;
                status_ip_set = true;
                break;
            case 's':
                strncpy(config->status_fifo_path, argv[param], 128);
                status_fifo_set=true;
                break;
            case 'w':
                config->port_swap=true;
                param--; /* there is no data for this so go back */
                break;
          }
        }
        param++;
    }

    if ((argc-param)<2) {
        err=ERROR_ARGS_INPUT;
        printf("ERROR: Main Frequency and Main Symbol Rate not found.\n");
    }

    if (err==ERROR_NONE) {
        config->freq_requested =(uint32_t)strtol(argv[param++],NULL,10);
        if(config->freq_requested==0) {
            err=ERROR_ARGS_INPUT;
            printf("ERROR: Main Frequency not in a valid format.\n");
        }

        config->sr_requested =(uint32_t)strtol(argv[param  ],NULL,10);
        if(config->sr_requested==0) {
            err=ERROR_ARGS_INPUT;
            printf("ERROR: Main Symbol Rate not in a valid format.\n");
        }
    }

    if (err==ERROR_NONE) {
        if (config->freq_requested>2450000) {
            err=ERROR_ARGS_INPUT;
            printf("ERROR: Freq must be <= 2450 MHz\n");
        } else if (config->freq_requested<144) {
            err=ERROR_ARGS_INPUT;
            printf("ERROR: Freq_must be >= 144 MHz\n");
        } else if (config->sr_requested>27500) {
            err=ERROR_ARGS_INPUT;
            printf("ERROR: SR must be <= 27 Msymbols/s\n");
        } else if (config->sr_requested<33) {
            err=ERROR_ARGS_INPUT;
            printf("ERROR: SR must be >= 33 Ksymbols/s\n");
        } else if (ts_ip_set && ts_fifo_set) {
            err=ERROR_ARGS_INPUT;
            printf("ERROR: Cannot set TS FIFO and TS IP address\n");
        } else if (status_ip_set && status_fifo_set) {
            err=ERROR_ARGS_INPUT;
            printf("ERROR: Cannot set Status FIFO and Status IP address\n");
        } else if (config->ts_use_ip && config->status_use_ip) {
            /* Check ip/port conflict */
            if((config->ts_ip_port == config->status_ip_port) && (0==strcmp(config->ts_ip_addr, config->status_ip_addr))) {
                err=ERROR_ARGS_INPUT;
                printf("ERROR: Cannot set Status IP & Port identical to TS IP & Port\n");
            }
        } else { /* err==ERROR_NONE */
             printf("      Status: Main Frequency=%i KHz\n",config->freq_requested);
             printf("              Main Symbol Rate=%i KSymbols/s\n",config->sr_requested);
             if (!main_usb_set)       printf("              Using First Minitiouner detected on USB\n");
             else                     printf("              USB bus/device=%i,%i\n",config->device_usb_bus,config->device_usb_addr);
             if (!config->ts_use_ip)  printf("              Main TS output to FIFO=%s\n",config->ts_fifo_path);
             else                     printf("              Main TS output to IP=%s:%i\n",config->ts_ip_addr,config->ts_ip_port);
             if (!config->status_use_ip)  printf("              Main Status output to FIFO=%s\n",config->status_fifo_path);
             else                     printf("              Main Status output to IP=%s:%i\n",config->status_ip_addr,config->status_ip_port);
             if (config->port_swap)   printf("              NIM inputs are swapped (Main now refers to BOTTOM F-Type\n");
             else                     printf("              Main refers to TOP F-Type\n");
        }
    }

    if (err!=ERROR_NONE) {
        printf("Please refer to the longmynd manual page via:\n");
        printf("    man -l longmynd.1\n");
    }
    return err;
}

/* -------------------------------------------------------------------------------------------------- */
uint8_t do_report(uint8_t state, uint8_t (*status_write)(uint8_t,uint32_t)) {
/* -------------------------------------------------------------------------------------------------- */
/* interrogates the demodulator to find the interesting info to report                                */
/*  state: the current state machine                                                                  */
/* return: error code                                                                                 */
/* -------------------------------------------------------------------------------------------------- */
    uint8_t err=ERROR_NONE;
    uint8_t lna_vgo;
    uint8_t lna_gain;
    uint8_t puncture_rate;
    uint8_t power_i, power_q;
    uint32_t vit_errs;
    uint8_t count;
    uint8_t i,q;
    int32_t car_freq;
    uint32_t actual_freq;
    uint32_t sr;
    uint32_t ber;
    uint32_t mer;

    /* LNAs if present */
    if (lna_ok) {
        if (err==ERROR_NONE) stvvglna_read_agc(NIM_INPUT_TOP, &lna_gain, &lna_vgo);
        if (err==ERROR_NONE) err=status_write(STATUS_LNA_GAIN,(lna_gain<<5) | lna_vgo);
    }

    /* I,Q powers */
    if (err==ERROR_NONE) err=stv0910_read_power(STV0910_DEMOD_TOP, &power_i, &power_q);
    if (err==ERROR_NONE) err=status_write(STATUS_POWER_I, power_i);
    if (err==ERROR_NONE) err=status_write(STATUS_POWER_Q, power_q);

    /* constellations */
    for (count=0; count<NUM_CONSTELLATIONS; count++) {
        if (err==ERROR_NONE) stv0910_read_constellation(STV0910_DEMOD_TOP, &i, &q);
        if (err==ERROR_NONE) err=status_write(STATUS_CONSTELLATION_I, i);
        if (err==ERROR_NONE) err=status_write(STATUS_CONSTELLATION_Q, q);
    }

    /* puncture rate */
    if (err==ERROR_NONE) err=stv0910_read_puncture_rate(STV0910_DEMOD_TOP, &puncture_rate);
    if (err==ERROR_NONE) err=status_write(STATUS_PUNCTURE_RATE, puncture_rate);

    /* carrier frequency offset we are trying */
    if (err==ERROR_NONE) err=stv0910_read_car_freq(STV0910_DEMOD_TOP, &car_freq);

    /* note we now have the offset, so we need to add in the freq we tried to set it to */
    actual_freq=(uint32_t)(freq+(car_freq/1000));
    if (err==ERROR_NONE) err=status_write(STATUS_CARRIER_FREQUENCY, actual_freq);

    /* symbol rate we are trying */
    if (err==ERROR_NONE) err=stv0910_read_sr(STV0910_DEMOD_TOP, &sr);
    if (err==ERROR_NONE) err=status_write(STATUS_SYMBOL_RATE, sr);

    /* viterbi error rate */
    if (err==ERROR_NONE) err=stv0910_read_err_rate(STV0910_DEMOD_TOP, &vit_errs);
    if (err==ERROR_NONE) err=status_write(STATUS_VITERBI_ERROR_RATE, vit_errs); 

    /* BER */
    if (err==ERROR_NONE) err=stv0910_read_ber(STV0910_DEMOD_TOP, &ber);
    if (err==ERROR_NONE) err=status_write(STATUS_BER, ber);

    /* MER */
    if(state==STATE_DEMOD_S2) {
        if (err==ERROR_NONE) err=stv0910_read_dvbs2_mer(STV0910_DEMOD_TOP, &mer);
        if (err==ERROR_NONE) err=status_write(STATUS_MER, mer);
    }

    return err;
}

pthread_t thread_ts;

typedef struct {
    uint8_t *main_state_ptr;
    uint8_t *main_err_ptr;
    bool use_ip;
} loop_ts_vars_t;

void *loop_ts(void *arg){
    loop_ts_vars_t *loop_ts_vars=(loop_ts_vars_t *)arg;
    uint8_t err=ERROR_NONE;
    uint16_t len=0;

    while(err == ERROR_NONE && *loop_ts_vars->main_err_ptr == ERROR_NONE){

        /* once we are running, every time round we check to see if there is any ts data to send out */
        if (*loop_ts_vars->main_state_ptr==STATE_INIT){
            /* if we are in the init phase then we need to empty the TS buffer from any old data */
            do {
                if (err==ERROR_NONE) err=ftdi_usb_ts_read(buffer, &len);
            } while (len>2);
        }else{
            if (err==ERROR_NONE) err=ftdi_usb_ts_read(buffer, &len);
            //fprintf(stderr, "ts_read: %d\n", len);
            /* if there is ts data then we send it out to the required output. But, we have to lose the first 2 bytes */
            /* that are the usual FTDI 2 byte response and not part of the TS */
            if ((err==ERROR_NONE) && (len>2)) {
                if (loop_ts_vars->use_ip) err= udp_ts_write(&buffer[2],len-2);
                else        err=fifo_ts_write(&buffer[2],len-2);
            }
        }

    }
    return NULL;
}

/* -------------------------------------------------------------------------------------------------- */
int main(int argc, char *argv[]) {
/* -------------------------------------------------------------------------------------------------- */
/*    command line processing                                                                         */
/*    module initialisation                                                                           */
/*    main loop of checking tuner/demoduator status and executing the state machine accordingly       */
/* -------------------------------------------------------------------------------------------------- */
    uint8_t err;
    uint8_t state=STATE_INIT;
    uint8_t demod_state;
    uint64_t last_status_loop;
    uint8_t (*status_write)(uint8_t,uint32_t);


    printf("Flow: main\n");

    err=process_command_line(argc, argv, &longmynd_config);

    /* first setup the fifos, udp socket, ftdi and usb */
    if(longmynd_config.status_use_ip) {
        if (err==ERROR_NONE) err=udp_status_init(longmynd_config.status_ip_addr, longmynd_config.status_ip_port);
    } else {
        if (err==ERROR_NONE) err=fifo_status_init(longmynd_config.status_fifo_path);
    }
    if(longmynd_config.ts_use_ip) {
        if (err==ERROR_NONE) err=udp_ts_init(longmynd_config.ts_ip_addr, longmynd_config.ts_ip_port);
    } else {
        if (err==ERROR_NONE) err=fifo_ts_init(longmynd_config.ts_fifo_path);
    }

    if (err==ERROR_NONE) err=ftdi_init(longmynd_config.device_usb_bus, longmynd_config.device_usb_addr);

    loop_ts_vars_t loop_ts_vars = {
        .main_state_ptr = &state,
        .main_err_ptr = &err,
        .use_ip = longmynd_config.ts_use_ip
    };

    if(0 != pthread_create(&thread_ts, NULL, loop_ts, (void *)&loop_ts_vars))
    {
        fprintf(stderr, "Error creating loop_ts pthread\n");
    }

    if(longmynd_config.status_use_ip) {
        status_write = udp_status_write;
    } else {
        status_write = fifo_status_write;
    }

    /* here is the main loop and state machine */
    while (err==ERROR_NONE) {
        last_status_loop = timestamp_ms();
        switch(state) {
            case STATE_INIT:
                if (err==ERROR_NONE) err=status_write(STATUS_STATE,state);
                /* init all the modules */
                if (err==ERROR_NONE) err=nim_init();
                /* we are only using the one demodulator so set the other to 0 to turn it off */
                if (err==ERROR_NONE) err=stv0910_init(longmynd_config.sr_requested,0);
                /* we only use one of the tuners in STV6120 so freq for tuner 2=0 to turn it off */
                if (err==ERROR_NONE) err=stv6120_init(longmynd_config.freq_requested,0,longmynd_config.port_swap);
                /* we turn on the LNA we want and turn the other off (if they exist) */
                if (err==ERROR_NONE) err=stvvglna_init(NIM_INPUT_TOP,    (longmynd_config.port_swap) ? STVVGLNA_OFF : STVVGLNA_ON,  &lna_ok);
                if (err==ERROR_NONE) err=stvvglna_init(NIM_INPUT_BOTTOM, (longmynd_config.port_swap) ? STVVGLNA_ON  : STVVGLNA_OFF, &lna_ok);

                if (err!=ERROR_NONE) printf("ERROR: failed to init a device - is the NIM powered on?\n");

                /* now start the whole thing scanning for the signal */
                if (err==ERROR_NONE) {
                    err=stv0910_start_scan(STV0910_DEMOD_TOP);
                    state=STATE_DEMOD_HUNTING;
                    if (err==ERROR_NONE) err=status_write(STATUS_STATE,state);
                }
               break;

            case STATE_DEMOD_HUNTING:
                if (err==ERROR_NONE) err=do_report(state, status_write);
                /* process state changes */
                if (err==ERROR_NONE) err=stv0910_read_scan_state(STV0910_DEMOD_TOP, &demod_state);
                if (demod_state==DEMOD_FOUND_HEADER) {
                     state=STATE_DEMOD_FOUND_HEADER;
                     if (err==ERROR_NONE) err=status_write(STATUS_STATE,state);
                }
                else if (demod_state==DEMOD_S2) {
                    state=STATE_DEMOD_S2;
                    if (err==ERROR_NONE) err=status_write(STATUS_STATE,state);
                }
                else if (demod_state==DEMOD_S) {
                    state=STATE_DEMOD_S;
                    if (err==ERROR_NONE) err=status_write(STATUS_STATE,state);
                }
                else if ((demod_state!=DEMOD_HUNTING) && (err==ERROR_NONE)) {
                    printf("ERROR: demodulator returned a bad scan state\n");
                    err=ERROR_BAD_DEMOD_HUNT_STATE; /* not allowed to have any other states */
                } /* no need for another else, all states covered */
                break;

            case STATE_DEMOD_FOUND_HEADER:
                if (err==ERROR_NONE) err=do_report(state, status_write);
                /* process state changes */
                err=stv0910_read_scan_state(STV0910_DEMOD_TOP, &demod_state);
                if (demod_state==DEMOD_HUNTING) {
                    state=STATE_DEMOD_HUNTING;
                    if (err==ERROR_NONE) err=status_write(STATUS_STATE,state);
                }
                else if (demod_state==DEMOD_S2)  {
                    state=STATE_DEMOD_S2;
                    if (err==ERROR_NONE) err=status_write(STATUS_STATE,state);
                }
                else if (demod_state==DEMOD_S)  {
                    state=STATE_DEMOD_S;
                    if (err==ERROR_NONE) err=status_write(STATUS_STATE,state);
                }
                else if ((demod_state!=DEMOD_FOUND_HEADER) && (err==ERROR_NONE)) {
                    printf("ERROR: demodulator returned a bad scan state\n");
                    err=ERROR_BAD_DEMOD_HUNT_STATE; /* not allowed to have any other states */
                } /* no need for another else, all states covered */
                break;

            case STATE_DEMOD_S2:
                if (err==ERROR_NONE) err=do_report(state, status_write);
                /* process state changes */
                err=stv0910_read_scan_state(STV0910_DEMOD_TOP, &demod_state);
                if (demod_state==DEMOD_HUNTING) {
                    state=STATE_DEMOD_HUNTING;
                    if (err==ERROR_NONE) err=status_write(STATUS_STATE,state);
                }
                else if (demod_state==DEMOD_FOUND_HEADER)  {
                    state=STATE_DEMOD_FOUND_HEADER;
                    if (err==ERROR_NONE) err=status_write(STATUS_STATE,state);
                }
                else if (demod_state==DEMOD_S) {
                    state=STATE_DEMOD_S;
                    if (err==ERROR_NONE) err=status_write(STATUS_STATE,state);
                }
                else if ((demod_state!=DEMOD_S2) && (err==ERROR_NONE)) {
                    printf("ERROR: demodulator returned a bad scan state\n");
                    err=ERROR_BAD_DEMOD_HUNT_STATE; /* not allowed to have any other states */
                } /* no need for another else, all states covered */
                break;

            case STATE_DEMOD_S:
                if (err==ERROR_NONE) err=do_report(state, status_write);
                /* process state changes */
                err=stv0910_read_scan_state(STV0910_DEMOD_TOP, &demod_state);
                if (demod_state==DEMOD_HUNTING) {
                    state=STATE_DEMOD_HUNTING;
                    if (err==ERROR_NONE) err=status_write(STATUS_STATE,state);
                }
                else if (demod_state==DEMOD_FOUND_HEADER)  {
                    state=STATE_DEMOD_FOUND_HEADER;
                    if (err==ERROR_NONE) err=status_write(STATUS_STATE,state);
                }
                else if (demod_state==DEMOD_S2) {
                    state=STATE_DEMOD_S2;
                    if (err==ERROR_NONE) err=status_write(STATUS_STATE,state);
                }
                else if ((demod_state!=DEMOD_S) && (err==ERROR_NONE)) {
                    printf("ERROR: demodulator returned a bad scan state\n");
                    err=ERROR_BAD_DEMOD_HUNT_STATE; /* not allowed to have any other states */
                } /* no need for another else, all states covered */
                break;

            default:
                err=ERROR_STATE; /* we should never get here so panic if we do */
                break;
        }

        /* Status Report Loop Timer */
        do {
            /* Sleep for at least 10ms */
            usleep(10*1000);
        } while (timestamp_ms() < (last_status_loop + STATUS_LOOP_MS));
    }

    /* Exited, wait for child threads to exit */
    pthread_join(thread_ts, NULL);

    return err;
}
