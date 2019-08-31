/* -------------------------------------------------------------------------------------------------- */
/* The LongMynd receiver: stvvglna_utils.h                                                            */
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

#ifndef STVVGLNA_UTILS_H
#define STVVGLNA_UTILS_H

#include <stdint.h>

uint8_t stvvglna_read_reg(uint8_t, uint8_t, uint8_t *);
uint8_t stvvglna_write_reg(uint8_t, uint8_t, uint8_t);

#endif
