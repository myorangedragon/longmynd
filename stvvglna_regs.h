/* -------------------------------------------------------------------------------------------------- */
/* The LongMynd receiver: stvvglna_regs.h                                                             */
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

#ifndef STVVGLNA_REGS_H
#define STVVGLNA_REGS_H

#define STVVGLNA_I2C_ADDR0 0xc8
#define STVVGLNA_I2C_ADDR1 0xca
#define STVVGLNA_I2C_ADDR2 0xcc
#define STVVGLNA_I2C_ADDR3 0xce

#define STVVGLNA_REG0 0x00
#define STVVGLNA_REG0_IDENT_SHIFT         4
#define STVVGLNA_REG0_IDENT_MASK          0xf0
#define STVVGLNA_REG0_IDENT_DEFAULT       0x20
#define STVVGLNA_REG0_AGC_TUPD_SHIFT      3
#define STVVGLNA_REG0_AGC_TUPD_FAST            0
#define STVVGLNA_REG0_AGC_TUPD_SLOW            1
#define STVVGLNA_REG0_AGC_TLOCK_SHIFT     2
#define STVVGLNA_REG0_AGC_TLOCK_SLOW           0
#define STVVGLNA_REG0_AGC_TLOCK_FAST           1
#define STVVGLNA_REG0_RFAGC_HIGH_SHIFT    1
#define STVVGLNA_REG0_RFAGC_HIGH_NOT_HIGH      0
#define STVVGLNA_REG0_RFAGC_HIGH_IS_HIGH       1
#define STVVGLNA_REG0_RFAGC_LOW_SHIFT     0
#define STVVGLNA_REG0_RFAGC_LOW_NOT_LOW        0
#define STVVGLNA_REG0_RFAGC_LOW_IS_LOW         1

#define STVVGLNA_REG1 0x01
#define STVVGLNA_REG1_LNAGC_PWD_SHIFT 7
#define STVVGLNA_REG1_LNAGC_PWD_POWER_ON   0
#define STVVGLNA_REG1_LNAGC_PWD_POWER_OFF  1
#define STVVGLNA_REG1_GETOFF_SHIFT    6
#define STVVGLNA_REG1_GETOFF_ACQUISITION_MODE 0
#define STVVGLNA_REG1_GETOFF_VGO_4_0          1
#define STVVGLNA_REG1_GETAGC_SHIFT    5
#define STVVGLNA_REG1_GETAGC_FORCED        0
#define STVVGLNA_REG1_GETAGC_START         1
#define STVVGLNA_REG1_VGO_SHIFT       0
#define STVVGLNA_REG1_VGO_MASK        0x1f

#define STVVGLNA_REG2 0x02
#define STVVGLNA_REG2_PATH2OFF_SHIFT   7
#define STVVGLNA_REG2_PATH_ACTIVE           0
#define STVVGLNA_REG2_PATH_OFF              1
#define STVVGLNA_REG2_RFAGC_PREF_SHIFT 4
#define STVVGLNA_REG2_RFAGC_PREF_MASK  0x70
#define STVVGLNA_REG2_RFAGC_PREF_N25DBM     0x0
#define STVVGLNA_REG2_RFAGC_PREF_N24DBM     0x1
#define STVVGLNA_REG2_RFAGC_PREF_N23DBM     0x2
#define STVVGLNA_REG2_RFAGC_PREF_N22DBM     0x3
#define STVVGLNA_REG2_RFAGC_PREF_N21DBM     0x4
#define STVVGLNA_REG2_RFAGC_PREF_N20DBM     0x5
#define STVVGLNA_REG2_RFAGC_PREF_N19DBM     0x6
#define STVVGLNA_REG2_RFAGC_PREF_N18DBM     0x7
#define STVVGLNA_REG2_PATH1OFF_SHIFT   3
#define STVVGLNA_REG2_RFAGC_MODE_SHIFT 0
#define STVVGLNA_REG2_RFAGC_MODE_MASK  0x07
#define STVVGLNA_REG2_RFAGC_MODE_AUTO_TRACK            0x0
#define STVVGLNA_REG2_RFAGC_MODE_AUTO_REQUEST          0x1
#define STVVGLNA_REG2_RFAGC_MODE_MINIMAL_GAIN_INTERNAL 0x2
#define STVVGLNA_REG2_RFAGC_MODE_MAXIMAL_GAIN_INTERNAL 0x3
#define STVVGLNA_REG2_RFAGC_MODE_EXTERNAL_AGC_EXTERNAL 0x4
#define STVVGLNA_REG2_RFAGC_MODE_AGC_LOOP_EXTERNAL     0x5
#define STVVGLNA_REG2_RFAGC_MODE_MINIMAL_AGC_EXTERNAL  0x6
#define STVVGLNA_REG2_RFAGC_MODE_MAXIMAL_AGC_EXTERNAL  0x7


#define STVVGLNA_REG3 0x03
#define STVVGLNA_REG3_LCAL_SHIFT           4 
#define STVVGLNA_REG3_LCAL_MASK            0x70 
#define STVVGLNA_REG3_LCAL_68KHZ                 0x0 
#define STVVGLNA_REG3_LCAL_34KHZ                 0x1 
#define STVVGLNA_REG3_LCAL_17KHZ                 0x2 
#define STVVGLNA_REG3_LCAL_8_5KHZ                0x3 
#define STVVGLNA_REG3_LCAL_4_2KHZ                0x4 
#define STVVGLNA_REG3_LCAL_2_1KHZ                0x5 
#define STVVGLNA_REG3_LCAL_1_0KHZ                0x6 
#define STVVGLNA_REG3_LCAL_0_5KHZ                0x7 
#define STVVGLNA_REG3_RFAGC_UPDATE_SHIFT   3
#define STVVGLNA_REG3_RFAGC_UPDATE_FORCED        0
#define STVVGLNA_REG3_RFAGC_UPDATE_START         1
#define STVVGLNA_REG3_RFAGC_CALSTART_SHIFT 2
#define STVVGLNA_REG3_RFAGC_CALSTART_FORCED      0
#define STVVGLNA_REG3_RFAGC_CALSTART_START       1
#define STVVGLNA_REG3_SWLNAGAIN_SHIFT      0 
#define STVVGLNA_REG3_SWLNAGAIN_MASK       0x03
#define STVVGLNA_REG3_SWLNAGAIN_LOWEST            0x0
#define STVVGLNA_REG3_SWLNAGAIN_INTERMEDIATE_LOW  0x1
#define STVVGLNA_REG3_SWLNAGAIN_INTERMEDIATE_HIGH 0x2
#define STVVGLNA_REG3_SWLNAGAIN_HIGHEST           0x3

#endif

