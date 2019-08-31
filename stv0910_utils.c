/* -------------------------------------------------------------------------------------------------- */
/* The LongMynd receiver: stv0910_utils.c                                                             */
/*    - an implementation of the Serit NIM controlling software for the MiniTiouner Hardware          */
/*    - These routines abstract the register read and writes for the demodulator (STV0910)            */
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
#include <stdint.h>
#include "stv0910_regs.h"
#include "stv0910_utils.h"
#include "errors.h"
#include "nim.h"

/* in order to do bitfields efficiently, we need to keep a shadow register set */
uint8_t stv0910_shadow_regs[STV0910_END_ADDR - STV0910_START_ADDR + 1];

/* -------------------------------------------------------------------------------------------------- */
/* ----------------- ROUTINES ----------------------------------------------------------------------- */
/* -------------------------------------------------------------------------------------------------- */

/* -------------------------------------------------------------------------------------------------- */
uint8_t stv0910_write_reg_field(uint32_t field, uint8_t field_val) {
/* -------------------------------------------------------------------------------------------------- */
/* changes a bitfield of a register using a shadow register array to do the read/modify/write         */
/*     field: the #define of the bitfield as given in stv0910_regs.h (from the datasheet)             */
/* field_val: what to set the field to (in the LSBs of the value                                      */
/*    return: error code                                                                              */
/* -------------------------------------------------------------------------------------------------- */
    uint8_t err=ERROR_NONE;
    uint16_t reg;
    uint8_t val;

    /* firsr we need to work out which register to use */
    reg=field >> 16;
    /* now we calculate the new value for this reg by reading the shadow array, */
    /*  masking out the field we want, and putting the new value in             */
    val=((stv0910_shadow_regs[reg-STV0910_START_ADDR] & ~(field & 0xff)) |
         (field_val << ((field >> 12) & 0x0f))        );
    /* now we can write the new value back to the demodulator and the shadow registers */
    if (err==ERROR_NONE) err=nim_write_demod(reg, val);
    if (err==ERROR_NONE) stv0910_shadow_regs[reg-STV0910_START_ADDR]=val;

    if (err!=ERROR_NONE) printf("ERROR: STV0910 write field\n");

    return err;
}

/* -------------------------------------------------------------------------------------------------- */
uint8_t stv0910_read_reg_field(uint32_t field, uint8_t *field_val) {
/* -------------------------------------------------------------------------------------------------- */
/* reads a bitfield of a register. This cannot go via the shadows as the regs are volatile            */
/*      field: the #define of the bitfield as given in stv0910_regs.h (from the datasheet)            */
/* *field_val: the contents of the field once read                                                    */
/*     return: error code                                                                             */
/* -------------------------------------------------------------------------------------------------- */
    uint8_t err=ERROR_NONE;
    uint8_t val;

    /* we can read the register value first */
    if (err==ERROR_NONE) err=nim_read_demod((uint16_t)(field >> 16), &val);
    /* and then do the masks and shifts to get at the specific bits */
    *field_val = ((val) & (field & 0xff)) >> ((field >> 12) & 0x0f);

    if (err!=ERROR_NONE) printf("ERROR: STV0910 read field\n");

    return err;
}

/* -------------------------------------------------------------------------------------------------- */
uint8_t stv0910_write_reg(uint16_t reg, uint8_t val) {
/* -------------------------------------------------------------------------------------------------- */
/* abstracts a hardware register write to the stv0910                                                 */
/*    return: error code                                                                              */
/* -------------------------------------------------------------------------------------------------- */

    stv0910_shadow_regs[reg-STV0910_START_ADDR]=val;

    return nim_write_demod(reg, val);
}

/* -------------------------------------------------------------------------------------------------- */
uint8_t stv0910_read_reg(uint16_t reg, uint8_t *val) {
/* -------------------------------------------------------------------------------------------------- */
/* abstracts a hardware register read from the stv0910                                                */
/*    return: error code                                                                              */
/* -------------------------------------------------------------------------------------------------- */

    return nim_read_demod(reg, val);
}

