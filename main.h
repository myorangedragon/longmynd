/* -------------------------------------------------------------------------------------------------- */
/* The LongMynd receiver: main.h                                                                      */
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

#ifndef MAIN_H
#define MAIN_H

#include <stdint.h>
#include <stdbool.h>
#include <inttypes.h>
#include <pthread.h>

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
#define STATUS_SERVICE_NAME       13
#define STATUS_SERVICE_PROVIDER_NAME  14
#define STATUS_TS_NULL_PERCENTAGE 15
#define STATUS_ES_PID             16
#define STATUS_ES_TYPE            17
#define STATUS_MODCOD             18
#define STATUS_SHORT_FRAME        19
#define STATUS_PILOTS             20

/* The number of constellation peeks we do for each background loop */
#define NUM_CONSTELLATIONS 16

#define NUM_ELEMENT_STREAMS 16

typedef struct {
    bool port_swap;
    uint8_t port;
    uint32_t freq_requested;
    uint32_t sr_requested;
    bool beep_enabled;

    uint8_t device_usb_bus;
    uint8_t device_usb_addr;

    bool ts_use_ip;
    bool ts_reset;
    char ts_fifo_path[128];
    char ts_ip_addr[16];
    int ts_ip_port;

    bool status_use_ip;
    char status_fifo_path[128];
    char status_ip_addr[16];
    int status_ip_port;

    bool polarisation_supply;
    bool polarisation_horizontal; // false -> 13V, true -> 18V

    bool new;
    pthread_mutex_t mutex;
} longmynd_config_t;

typedef struct {
    uint8_t state;
    uint8_t demod_state;
    bool lna_ok;
    uint16_t lna_gain;
    uint8_t power_i;
    uint8_t power_q;
    uint32_t frequency_requested;
    int32_t frequency_offset;
    uint32_t symbolrate;
    uint32_t viterbi_error_rate; // DVB-S1
    uint32_t bit_error_rate; // DVB-S2
    uint32_t modulation_error_rate; // DVB-S2
    uint8_t constellation[NUM_CONSTELLATIONS][2]; // { i, q }
    uint8_t puncture_rate;
    char service_name[255];
    char service_provider_name[255];
    uint8_t ts_null_percentage;
    uint16_t ts_elementary_streams[NUM_ELEMENT_STREAMS][2]; // { pid, type }
    uint32_t modcod;
    bool short_frame;
    bool pilots;

    bool new;
    pthread_mutex_t mutex;
    pthread_cond_t signal;
} longmynd_status_t;

typedef struct {
    uint8_t *main_state_ptr;
    uint8_t *main_err_ptr;
    uint8_t thread_err;
    longmynd_config_t *config;
    longmynd_status_t *status;
} thread_vars_t;

#endif

