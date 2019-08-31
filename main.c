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

/* number of background loop periods between each status reporting. Note: this loop period */
/* includes the timeout for the USB which is a lot slower when there is TS data */
#define TIMER_REPORT_STATUS 10

/* The number of constellation peeks we do for each background loop */
#define NUM_CONSTELLATIONS 16

/* we have to do reporting in small chunks to keep up with the Transport stream */
#define REPORTS_CHUNKS 5

/* -------------------------------------------------------------------------------------------------- */
/* ----------------- GLOBALS ------------------------------------------------------------------------ */
/* -------------------------------------------------------------------------------------------------- */

uint8_t buffer[FTDI_USB_TS_FRAME_SIZE]; /* holds teh TS stream data */
bool lna_ok; /* set to true when LNAs are detected */
uint32_t freq;  /* the requested frequency required for status reporting */

/* -------------------------------------------------------------------------------------------------- */
/* ----------------- ROUTINES ----------------------------------------------------------------------- */
/* -------------------------------------------------------------------------------------------------- */

/* -------------------------------------------------------------------------------------------------- */
uint8_t process_command_line(int argc, char *argv[],
                             uint32_t *main_freq, uint32_t *main_sr,
                             char **main_ts_fifo, char **main_status_fifo,
                             uint8_t *main_usb_bus, uint8_t *main_usb_addr,
                             bool *main_ip_set, char **main_ip_addr, int *main_ip_port,
                             bool *swap) {
/* -------------------------------------------------------------------------------------------------- */
/* processes the command line arguments, sets up the parameters in main from them and error checks    */
/* All the required parameters are passed in                                                          */
/* return: error code                                                                                 */
/* -------------------------------------------------------------------------------------------------- */
    uint8_t err=ERROR_NONE;
    uint8_t param;
    bool main_usb_set=false;
    bool main_fifo_set=false;

    *main_ts_fifo="longmynd_main_ts";
    *main_status_fifo="longmynd_main_status";
    *main_usb_bus=0;
    *main_usb_addr=0;
    *main_ip_set=false;
    *main_ip_addr="";
    *main_ip_port=0;
    *swap=false;

    param=1;
    while (param<argc-2) {
        if (argv[param][0]=='-') {
          switch (argv[param++][1]) {
            case 'u':
                *main_usb_bus =(uint8_t)strtol(argv[param++],NULL,10);
                *main_usb_addr=(uint8_t)strtol(argv[param  ],NULL,10);
                main_usb_set=true;
                break;
            case 'i':
                *main_ip_addr=argv[param++];
                *main_ip_port=(uint16_t)strtol(argv[param],NULL,10);
                *main_ip_set=true;
                break;
            case 't':
                *main_ts_fifo=argv[param];
                main_fifo_set=true;
                break;
            case 's':
                *main_status_fifo=argv[param];
                break;
            case 'w':
                *swap=true;
                param--; /* there is no data for this so go back */
                break;
          }
        }
        param++;
    }

    *main_freq =(uint32_t)strtol(argv[param++],NULL,10);
    *main_sr   =(uint32_t)strtol(argv[param  ],NULL,10);

    if (*main_freq>2450000) {
        err=ERROR_ARGS_INPUT;
        printf("ERROR: Freq must be <= 2450 MHz\n");
    } else if (*main_freq<144) {
        err=ERROR_ARGS_INPUT;
        printf("ERROR: Freq_must be >= 144 MHz\n");
    } else if (*main_sr>27500) {
        err=ERROR_ARGS_INPUT;
        printf("ERROR: SR must be <= 27 Msymbols/s\n");
    } else if (*main_sr<33) {
        err=ERROR_ARGS_INPUT;
        printf("ERROR: SR must be >= 33 Ksymbols/s\n");
    } else if (main_ip_set && main_fifo_set) {
        err=ERROR_ARGS_INPUT;
        printf("ERROR: Cannot set main FIFO and Main IP address\n");
    } else { /* err==ERROR_NONE */
         printf("      Status: Main Frequency=%i KHz\n",*main_freq);
         printf("              Main Symbol Rate=%i KSymbols/S\n",*main_sr);
         if (!main_usb_set) printf("              Using First Minitiouner detected on USB\n");
         else               printf("              USB bus/device=%i,%i\n",*main_usb_bus,*main_usb_addr);
         if (!main_ip_set)  printf("              Main TS output to FIFO=%s\n",*main_ts_fifo);
         else               printf("              Main TS output to IP=%s:%i\n",*main_ip_addr,*main_ip_port);
         printf("              Main Status FIFO=%s\n",*main_status_fifo);
         if (*swap)         printf("              NIM inputs are swapped (Main now refers to BOTTOM F-Type\n");
         else               printf("              Main refers to TOP F-Type\n");
    }

    if (err!=ERROR_NONE) {
        printf("Please refer to the longmynd manual page via:\n");
        printf("    man -l longmynd.1\n");
    }
    return err;
}

/* -------------------------------------------------------------------------------------------------- */
uint8_t do_report() {
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
    static int loop_count=0; /* used to keep track of what we are reporting in which time slot */

    /* Note that we can only do a very limited amount of register reads in between each TS frame  */
    /* (problems start with 27 reg reads) so we have to do it a bit at a time                     */
    loop_count++;
    if (loop_count==REPORTS_CHUNKS) loop_count=0;

    if (loop_count==0) {
        /* LNAs if present */
        if (lna_ok) {
            if (err==ERROR_NONE) stvvglna_read_agc(NIM_INPUT_TOP, &lna_gain, &lna_vgo);
            if (err==ERROR_NONE) err=fifo_status_write(STATUS_LNA_GAIN,(lna_gain<<5) | lna_vgo);
        }
        /* I,Q powers */
        if (err==ERROR_NONE) err=stv0910_read_power(STV0910_DEMOD_TOP, &power_i, &power_q);
        if (err==ERROR_NONE) err=fifo_status_write(STATUS_POWER_I, power_i);
        if (err==ERROR_NONE) err=fifo_status_write(STATUS_POWER_Q, power_q);
    } else if (loop_count==1) {
        /* constellations */
        for (count=0; count<NUM_CONSTELLATIONS; count++) {
            if (err==ERROR_NONE) stv0910_read_constellation(STV0910_DEMOD_TOP, &i, &q);
            if (err==ERROR_NONE) err=fifo_status_write(STATUS_CONSTELLATION_I, i);
            if (err==ERROR_NONE) err=fifo_status_write(STATUS_CONSTELLATION_Q, q);
        }
    } else if (loop_count==2) {
        /* puncture rate */
        if (err==ERROR_NONE) err=stv0910_read_puncture_rate(STV0910_DEMOD_TOP, &puncture_rate);
        if (err==ERROR_NONE) err=fifo_status_write(STATUS_PUNCTURE_RATE, puncture_rate);
        /* carrier frequency offset we are trying */
        if (err==ERROR_NONE) err=stv0910_read_car_freq(STV0910_DEMOD_TOP, &car_freq);
        /* note we now have the offset, so we need to add in the freq we tried to set it to */
        actual_freq=(uint32_t)(freq+(car_freq/1000));
        if (err==ERROR_NONE) err=fifo_status_write(STATUS_CARRIER_FREQUENCY, actual_freq);
    } else if (loop_count==3) {
        /* symbol rate we are trying */
        if (err==ERROR_NONE) err=stv0910_read_sr(STV0910_DEMOD_TOP, &sr);
        if (err==ERROR_NONE) err=fifo_status_write(STATUS_SYMBOL_RATE, sr);
        /* viterbi error rate */
        if (err==ERROR_NONE) err=stv0910_read_err_rate(STV0910_DEMOD_TOP, &vit_errs);
        if (err==ERROR_NONE) err=fifo_status_write(STATUS_VITERBI_ERROR_RATE, vit_errs); 
    } else if (loop_count==4) {
        /* BER */
        if (err==ERROR_NONE) err=stv0910_read_ber(STV0910_DEMOD_TOP, &ber);
        if (err==ERROR_NONE) err=fifo_status_write(STATUS_BER, ber); 
    }

    return err;
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
    uint8_t usb_bus, usb_addr;
    uint32_t sr;
    uint8_t demod_state;
    uint16_t len=0;
    uint16_t timer=1; /* ensure we do state change at very start */
    char *ts_fifo;
    char *status_fifo;
    char *ip_addr;
    int ip_port;
    bool swap;
    bool use_ip;

    printf("Flow: main\n");

    err=process_command_line(argc, argv,
                             &freq, &sr, &ts_fifo, &status_fifo, &usb_bus, &usb_addr,
                             &use_ip, &ip_addr, &ip_port,
                             &swap);
    /* first setup the fifos, udp socket, ftdi and usb */
    if (err==ERROR_NONE) err=fifo_init(ts_fifo, status_fifo, use_ip);
    if ((use_ip) && (err==ERROR_NONE)) err=udp_init(ip_addr, ip_port);
    if (err==ERROR_NONE) err=ftdi_init(usb_bus, usb_addr);

    /* here is the main loop and state machine */
    while (err==ERROR_NONE) {

        /* once we are running, every time round we check to see if there is any ts data to send out */
        if (state!=STATE_INIT) {
            if (err==ERROR_NONE) err=ftdi_usb_ts_read(buffer, &len);
            /* if there is ts data then we send it out to the required output. But, we have to lose the first 2 bytes */
            /* that are the usual FTDI 2 byte response and not part of the TS */
            if ((err==ERROR_NONE) && (len>2)) {
                if (use_ip) err= udp_ts_write(&buffer[2],len-2);
                else        err=fifo_ts_write(&buffer[2],len-2);
            }
        }

        /* decide if we are ready to do or check state change */
        timer--;
        if ((err==ERROR_NONE) && (timer==0)) {
            timer=TIMER_REPORT_STATUS; /* reset the timer ready to time the next reporting of state */

            switch(state) {
                case STATE_INIT:
                    /* if we are in the init phase then we need to empty the TS buffer from any old data */
                    printf("Flow: Emptying old TS buffer\n");
                    do {
                        if (err==ERROR_NONE) err=ftdi_usb_ts_read(buffer, &len);
                    } while (len>2);
                    if (err==ERROR_NONE) err=fifo_status_write(STATUS_STATE,state);
                    /* init all the modules */
                    if (err==ERROR_NONE) err=nim_init();
                    /* we are only using the one demodulator so set the other to 0 to turn it off */
                    if (err==ERROR_NONE) err=stv0910_init(sr,0);
                    /* we only use one of the tuners in STV6120 so freq for tuner 2=0 to turn it off */
                    if (err==ERROR_NONE) err=stv6120_init(freq,0,swap);
                    /* we turn on the LNA we want and turn the other off (if they exist) */
                    if (err==ERROR_NONE) err=stvvglna_init(NIM_INPUT_TOP,    (swap) ? STVVGLNA_OFF : STVVGLNA_ON,  &lna_ok);
                    if (err==ERROR_NONE) err=stvvglna_init(NIM_INPUT_BOTTOM, (swap) ? STVVGLNA_ON  : STVVGLNA_OFF, &lna_ok);
                    if (err!=ERROR_NONE) printf("ERROR: failed to init a device\n");

                    /* now start the whole thing scanning for the signal */
                    if (err==ERROR_NONE) {
                        err=stv0910_start_scan(STV0910_DEMOD_TOP);
                        state=STATE_DEMOD_HUNTING;
                        if (err==ERROR_NONE) err=fifo_status_write(STATUS_STATE,state);
                    }
                   break;

                case STATE_DEMOD_HUNTING:
                    if (err==ERROR_NONE) err=do_report();
                    /* process state changes */
                    if (err==ERROR_NONE) err=stv0910_read_scan_state(STV0910_DEMOD_TOP, &demod_state);
                    if (demod_state==DEMOD_FOUND_HEADER) {
                         state=STATE_DEMOD_FOUND_HEADER;
                         if (err==ERROR_NONE) err=fifo_status_write(STATUS_STATE,state);
                    }
                    else if (demod_state==DEMOD_S2) {
                        state=STATE_DEMOD_S2;
                        if (err==ERROR_NONE) err=fifo_status_write(STATUS_STATE,state);
                    }
                    else if (demod_state==DEMOD_S) {
                        state=STATE_DEMOD_S;
                        if (err==ERROR_NONE) err=fifo_status_write(STATUS_STATE,state);
                    }
                    else if ((demod_state!=DEMOD_HUNTING) && (err==ERROR_NONE)) {
                        printf("ERROR: demodulator returned a bad scan state\n");
                        err=ERROR_BAD_DEMOD_HUNT_STATE; /* not allowed to have any other states */
                    } /* no need for another else, all states covered */
                    break;

                case STATE_DEMOD_FOUND_HEADER:
                    if (err==ERROR_NONE) err=do_report();
                    /* process state changes */
                    err=stv0910_read_scan_state(STV0910_DEMOD_TOP, &demod_state);
                    if (demod_state==DEMOD_HUNTING) {
                        state=STATE_DEMOD_HUNTING;
                        if (err==ERROR_NONE) err=fifo_status_write(STATUS_STATE,state);
                    }
                    else if (demod_state==DEMOD_S2)  {
                        state=STATE_DEMOD_S2;
                        if (err==ERROR_NONE) err=fifo_status_write(STATUS_STATE,state);
                    }
                    else if (demod_state==DEMOD_S)  {
                        state=STATE_DEMOD_S;
                        if (err==ERROR_NONE) err=fifo_status_write(STATUS_STATE,state);
                    }
                    else if ((demod_state!=DEMOD_FOUND_HEADER) && (err==ERROR_NONE)) {
                        printf("ERROR: demodulator returned a bad scan state\n");
                        err=ERROR_BAD_DEMOD_HUNT_STATE; /* not allowed to have any other states */
                    } /* no need for another else, all states covered */
                    break;

                case STATE_DEMOD_S2:
                    if (err==ERROR_NONE) err=do_report();
                    /* process state changes */
                    err=stv0910_read_scan_state(STV0910_DEMOD_TOP, &demod_state);
                    if (demod_state==DEMOD_HUNTING) {
                        state=STATE_DEMOD_HUNTING;
                        if (err==ERROR_NONE) err=fifo_status_write(STATUS_STATE,state);
                    }
                    else if (demod_state==DEMOD_FOUND_HEADER)  {
                        state=STATE_DEMOD_FOUND_HEADER;
                        if (err==ERROR_NONE) err=fifo_status_write(STATUS_STATE,state);
                    }
                    else if (demod_state==DEMOD_S) {
                        state=STATE_DEMOD_S;
                        if (err==ERROR_NONE) err=fifo_status_write(STATUS_STATE,state);
                    }
                    else if ((demod_state!=DEMOD_S2) && (err==ERROR_NONE)) {
                        printf("ERROR: demodulator returned a bad scan state\n");
                        err=ERROR_BAD_DEMOD_HUNT_STATE; /* not allowed to have any other states */
                    } /* no need for another else, all states covered */
                    break;

                case STATE_DEMOD_S:
                    if (err==ERROR_NONE) err=do_report();
                    /* process state changes */
                    err=stv0910_read_scan_state(STV0910_DEMOD_TOP, &demod_state);
                    if (demod_state==DEMOD_HUNTING) {
                        state=STATE_DEMOD_HUNTING;
                        if (err==ERROR_NONE) err=fifo_status_write(STATUS_STATE,state);
                    }
                    else if (demod_state==DEMOD_FOUND_HEADER)  {
                        state=STATE_DEMOD_FOUND_HEADER;
                        if (err==ERROR_NONE) err=fifo_status_write(STATUS_STATE,state);
                    }
                    else if (demod_state==DEMOD_S2) {
                        state=STATE_DEMOD_S2;
                        if (err==ERROR_NONE) err=fifo_status_write(STATUS_STATE,state);
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
        }
    }

    return err;
}
