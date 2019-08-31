/* -------------------------------------------------------------------------------------------------- */
/* The LongMynd receiver: stv6120_utils.h                                                             */
/*    - an implementation of the Serit NIM controlling software for the MiniTiouner Hardware          */
/*    - top level (main) and command line procesing                                                   */
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

#ifndef STV6120_UTILS_H
#define STV6120_UTILS_H

uint8_t stv6120_read_reg(uint8_t, uint8_t *);
uint8_t stv6120_write_reg(uint8_t, uint8_t);

#endif

