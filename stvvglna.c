/* -------------------------------------------------------------------------------------------------- */
/* The LongMynd receiver: stvvglna.c                                                                  */
/*    - an implementation of the Serit NIM controlling software for the MiniTiouner Hardware          */
/*    - the LNA support routines (STVVGLNA). The Serit NIM has 2 LNAs                                 */
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
#include <stdint.h>
#include "stvvglna.h"
#include "nim.h"
#include "stvvglna_regs.h"
#include "stvvglna_utils.h"
#include "errors.h"

/* -------------------------------------------------------------------------------------------------- */
/* ----------------- ROUTINES ----------------------------------------------------------------------- */
/* -------------------------------------------------------------------------------------------------- */

/* -------------------------------------------------------------------------------------------------- */
uint8_t stvvglna_read_agc(uint8_t input, uint8_t *gain, uint8_t *vgo) {
/* -------------------------------------------------------------------------------------------------- */
/* once we are running, this routine will read the gain and AGC for the LNA specified                 */
/*   input: NIM_INPUT_TOP | NIM_INPUT_BOTTOM: which LNA is being worked on                            */
/*  return: error code                                                                                */
/*   *gain: the gain read from the specified LNA                                                      */
/*    *vgo: the vgo read from the specified LNA                                                       */
/* -------------------------------------------------------------------------------------------------- */
    uint8_t err=ERROR_NONE;
    uint8_t lna_addr;
    uint16_t timeout=0;
    uint8_t status;

    /* first we decide which LNA to use */
    if (input==NIM_INPUT_TOP) lna_addr=NIM_LNA_0_ADDR;
    else lna_addr=NIM_LNA_1_ADDR;

    /* in fully auto, we can read the gain sn SWLNAGAIN and VGO[4:0]. First we get the LNA to measure the */
    /* variable part of the gain for us. Note, it is ok to write 0 to the other bitfields */
    if (err==ERROR_NONE) err=stvvglna_write_reg(lna_addr, STVVGLNA_REG1,
                              STVVGLNA_REG1_GETAGC_START << STVVGLNA_REG1_GETAGC_SHIFT);
    
    do {
        err=stvvglna_read_reg(lna_addr, STVVGLNA_REG1, &status);  /* read out the status */
        timeout++;
        if ((err==ERROR_NONE) && (timeout==STVVGLNA_AGC_TIMEOUT)) {
            err=ERROR_LNA_AGC_TIMEOUT;
            printf("Error: read AGC timeout\n");
        }
    }
    while ((err==ERROR_NONE) && (((status >> STVVGLNA_REG1_GETAGC_SHIFT) & 1)!=STVVGLNA_REG1_GETAGC_FORCED));
    stvvglna_read_reg(lna_addr, STVVGLNA_REG0, &status);  /* read out the RFAGC high and low bits */

    if (err==ERROR_NONE) err=stvvglna_read_reg(lna_addr, STVVGLNA_REG3, gain);  /* read out the gain curves */
    *gain=((*gain & STVVGLNA_REG3_SWLNAGAIN_MASK) >> STVVGLNA_REG3_SWLNAGAIN_SHIFT);
    if (err==ERROR_NONE) err=stvvglna_read_reg(lna_addr, STVVGLNA_REG1, vgo); /* read out the Vagc value */
    *vgo=((*vgo & STVVGLNA_REG1_VGO_MASK) >> STVVGLNA_REG1_VGO_SHIFT);

    if (err!=ERROR_NONE) printf("ERROR: Failed LNA aquire AGC %i\n", input);
    return err;
}

/* -------------------------------------------------------------------------------------------------- */
uint8_t stvvglna_init(uint8_t input, uint8_t state, bool *lna_ok) {
/* -------------------------------------------------------------------------------------------------- */
/* this is the init routine for the LNA                                                               */
/*   input: NIM_INPUT_TOP | NIM_INPUT_BOTTOM  : specifies which LNA we are processing                 */
/*  lna_ok: if we find a NIM with an LNA then we set this to true otherwise false                     */
/*  return: error code                                                                                */
/* -------------------------------------------------------------------------------------------------- */
    uint8_t err;
    uint8_t val;
    uint8_t lna_addr;

    printf("Flow: LNA init %i\n",input);

    /* first we decide which LNA to use */
    if (input==NIM_INPUT_TOP) lna_addr=NIM_LNA_0_ADDR;
    else lna_addr=NIM_LNA_1_ADDR;

    /* now check to see if there is an LNA present */
    err=stvvglna_read_reg(lna_addr, STVVGLNA_REG0, &val);
    if (err!=ERROR_NONE) {
       printf("      Status: found an older NIM with no LNA\n");
       *lna_ok=false; /* tell caller that there is no LNA */
       err=ERROR_NONE; /* we do not throw an error, just exit init */
    } else {
       /* otherwise, lna is there so we go on to us it */
        printf("      Status: found new NIM with LNAs\n");
        *lna_ok=true;
 
        /* now check it has a good ID */
        if ((val & STVVGLNA_REG0_IDENT_MASK) != STVVGLNA_REG0_IDENT_DEFAULT) {
            printf("ERROR: failed to recognise LNA ID %i %i\n",input,val);
            err=ERROR_LNA_ID;
        }
  
        if (state==STVVGLNA_ON) {
            /* set up the defaults. We are going to use fully automatic mode */
            if (err==ERROR_NONE) err=stvvglna_write_reg(lna_addr,STVVGLNA_REG0,
                                     (STVVGLNA_REG0_AGC_TUPD_FAST        << STVVGLNA_REG0_AGC_TUPD_SHIFT)   |
                                     (STVVGLNA_REG0_AGC_TLOCK_SLOW       << STVVGLNA_REG0_AGC_TLOCK_SHIFT)  );
            if (err==ERROR_NONE) err=stvvglna_write_reg(lna_addr,STVVGLNA_REG1,
                                     (STVVGLNA_REG1_LNAGC_PWD_POWER_ON      << STVVGLNA_REG1_LNAGC_PWD_SHIFT) |
                                     (STVVGLNA_REG1_GETOFF_ACQUISITION_MODE << STVVGLNA_REG1_GETOFF_SHIFT)    |
                                     (STVVGLNA_REG1_GETAGC_FORCED           << STVVGLNA_REG1_GETAGC_SHIFT)    );
            if (err==ERROR_NONE) err=stvvglna_write_reg(lna_addr,STVVGLNA_REG2,
                                     (STVVGLNA_REG2_PATH_ACTIVE           << STVVGLNA_REG2_PATH2OFF_SHIFT)    |
                                     (STVVGLNA_REG2_RFAGC_PREF_N20DBM     << STVVGLNA_REG2_RFAGC_PREF_SHIFT)  |
                                     (STVVGLNA_REG2_PATH_ACTIVE           << STVVGLNA_REG2_PATH1OFF_SHIFT)    |
                                     (STVVGLNA_REG2_RFAGC_MODE_AUTO_TRACK << STVVGLNA_REG2_RFAGC_MODE_SHIFT)  );
            /* note that in REG3 the gain curves (SWLNAGAIN) are read only in fully automatic mod, so no need to write to them */
            if (err==ERROR_NONE) err=stvvglna_write_reg(lna_addr,STVVGLNA_REG3,
                                     (STVVGLNA_REG3_LCAL_17KHZ            <<  STVVGLNA_REG3_LCAL_SHIFT)          |
                                     (STVVGLNA_REG3_RFAGC_UPDATE_FORCED   << STVVGLNA_REG3_RFAGC_UPDATE_SHIFT)   |
                                     (STVVGLNA_REG3_RFAGC_CALSTART_FORCED << STVVGLNA_REG3_RFAGC_CALSTART_SHIFT) );
        } else { /* state==STVVGLAN_OFF so disable it as we are not turning it on */
            if (err==ERROR_NONE) err=stvvglna_write_reg(lna_addr,STVVGLNA_REG2,
                                     (STVVGLNA_REG2_PATH_OFF              << STVVGLNA_REG2_PATH2OFF_SHIFT)    |
                                     (STVVGLNA_REG2_PATH_OFF              << STVVGLNA_REG2_PATH1OFF_SHIFT)    );
        }

    }

    if (err!=ERROR_NONE) printf("ERROR: LNA init %i\n",input);

    return err;
}

/* -------------------------------------------------------------------------------------------------- */
void stvvglna_read_regs(uint8_t addr) {
/* -------------------------------------------------------------------------------------------------- */
/* support routine for debug                                                                          */
/*    addr: the i2c address of the LNA to be dumped                                                   */
/*  return: error code                                                                                */
/* -------------------------------------------------------------------------------------------------- */
    uint8_t val;

    printf("LNA 0x%.2x regs:\n",addr);
    for(int i=0; i<4; i++) {
       stvvglna_read_reg(addr,i,&val);
       printf(" 0x%.2x = 0x%.2x\n",i,val);
    }
} 
