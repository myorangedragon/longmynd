/* -------------------------------------------------------------------------------------------------- */
/* The LongMynd receiver: stv0910.h                                                                   */
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

#ifndef STV0910_H
#define STV0910_H

#include <stdbool.h>

#define DEMOD_HUNTING 0
#define DEMOD_FOUND_HEADER 1
#define DEMOD_S2 2
#define DEMOD_S 3

#define STV0910_PLL_LOCK_TIMEOUT 100 

#define STV0910_SCAN_BLIND_BEST_GUESS 0x15

#define STV0910_DEMOD_TOP 1
#define STV0910_DEMOD_BOTTOM 2

#define STV0910_PUNCTURE_1_2 0x0d
#define STV0910_PUNCTURE_2_3 0x12
#define STV0910_PUNCTURE_3_4 0x15
#define STV0910_PUNCTURE_5_6 0x18
#define STV0910_PUNCTURE_6_7 0x19
#define STV0910_PUNCTURE_7_8 0x1a

uint8_t stv0910_read_car_freq(uint8_t, int32_t*);
uint8_t stv0910_read_constellation(uint8_t, uint8_t*, uint8_t*);
uint8_t stv0910_read_sr(uint8_t demod, uint32_t*);
uint8_t stv0910_read_puncture_rate(uint8_t, uint8_t*);
uint8_t stv0910_read_power(uint8_t, uint8_t*, uint8_t*);
uint8_t stv0910_read_err_rate(uint8_t, uint32_t*);
uint8_t stv0910_read_ber(uint8_t, uint32_t*);
uint8_t stv0910_read_dvbs2_mer(uint8_t, uint32_t*);
uint8_t stv0910_read_errors_bch_uncorrected(uint8_t demod, bool *errors_bch_uncorrected);
uint8_t stv0910_read_errors_bch_count(uint8_t demod, uint32_t *errors_bch_count);
uint8_t stv0910_read_errors_ldpc_count(uint8_t demod, uint32_t *errors_ldpc_count);
uint8_t stv0910_read_mer(uint8_t, uint32_t*);
uint8_t stv0910_read_modcod_and_type(uint8_t, uint32_t*, bool*, bool*);
uint8_t stv0910_init(uint32_t, uint32_t);
uint8_t stv0910_init_regs(void);
uint8_t stv0910_setup_timing_loop(uint8_t, uint32_t);
uint8_t stv0910_setup_carrier_loop(uint8_t); 
uint8_t stv0910_read_scan_state(uint8_t, uint8_t *);
uint8_t stv0910_start_scan(uint8_t);
uint8_t stv0910_setup_search_params(uint8_t);
uint8_t stv0910_setup_clocks();

#endif

