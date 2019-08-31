/* -------------------------------------------------------------------------------------------------- */
/* The LongMynd receiver: stvvglna_utils.c                                                            */
/*    - an implementation of the Serit NIM controlling software for the MiniTiouner Hardware          */
/*    - abstracting out the lna register read and write routines                                      */
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

#include <stdint.h>
#include "nim.h"
#include "stvvglna_regs.h"

/* -------------------------------------------------------------------------------------------------- */
/* ----------------- ROUTINES ----------------------------------------------------------------------- */
/* -------------------------------------------------------------------------------------------------- */

/* -------------------------------------------------------------------------------------------------- */
uint8_t stvvglna_read_reg(uint8_t lna_addr, uint8_t reg, uint8_t *val) {
/* -------------------------------------------------------------------------------------------------- */
/* passes the register read through to the underlying register reading routines                       */
/* -------------------------------------------------------------------------------------------------- */
    return nim_read_lna(lna_addr, reg, val);
}

/* -------------------------------------------------------------------------------------------------- */
uint8_t stvvglna_write_reg(uint8_t lna_addr, uint8_t reg, uint8_t val) {
/* -------------------------------------------------------------------------------------------------- */
/* passes the register write through to the underlying register writing routines                      */
/* -------------------------------------------------------------------------------------------------- */
    return nim_write_lna(lna_addr, reg, val);
}
