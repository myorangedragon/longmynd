/* -------------------------------------------------------------------------------------------------- */
/* The LongMynd receiver: stvvglna.h                                                                  */
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

#ifndef STVVGLNA_H
#define STVVGLNA_H

#include "stvvglna_regs.h"
#include <stdint.h>
#include <stdbool.h>

#define STVVGLNA_AGC_TIMEOUT 20
#define STVVGLNA_OFF 0
#define STVVGLNA_ON  1

uint8_t stvvglna_read_agc(uint8_t, uint8_t *, uint8_t*);
uint8_t stvvglna_init(uint8_t, uint8_t,  bool*);
void stvvglna_read_regs (uint8_t);

#endif
