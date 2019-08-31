/* -------------------------------------------------------------------------------------------------- */
/* The LongMynd receiver: stv6120.h                                                                   */
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

#ifndef STV6120_H
#define STV6120_H

#include <stdint.h>

#define TUNER_LOCKED     0
#define TUNER_NOT_LOCKED 1

#define TUNER_1 0
#define TUNER_2 1

#define STV6120_RDIV_THRESHOLD  27000
#define STV6120_P_THRESHOLD_1  299000
#define STV6120_P_THRESHOLD_2  596000
#define STV6120_P_THRESHOLD_3 1191000

#define STV6120_CAL_TIMEOUT 200

uint8_t stv6120_init(uint32_t, uint32_t, bool);
uint8_t stv6120_set_freq(uint8_t, uint32_t);
uint8_t stv6120_cal_lowpass(uint8_t);
void stv6120_print_settings();

#endif

