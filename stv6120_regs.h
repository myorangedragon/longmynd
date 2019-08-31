/* -------------------------------------------------------------------------------------------------- */
/* The LongMynd receiver: stv6120_regs.h                                                              */
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

#ifndef STV6120_REGS_H
#define STV6120_REGS_H

/* common registers toboth tuners */
#define STV6120_CTRL1  0x00
#define STV6120_CTRL1_K_SHIFT       3
#define STV6120_CTRL1_K_MASK        0xf8
#define STV6120_CTRL1_RDIV_SHIFT    2
#define STV6120_CTRL1_OSHAPE_SHIFT  1
#define STV6120_CTRL1_OSHAPE_SINE       0
#define STV6120_CTRL1_OSHAPE_SQUARE     1
#define STV6120_CTRL1_MCLKDIV_SHIFT 0
#define STV6120_CTRL1_MCLKDIV_2         0
#define STV6120_CTRL1_MCLKDIV_4         1

#define STV6120_CTRL2  0x01
#define STV6120_CTRL2_DCLOOPOFF_SHIFT     7
#define STV6120_CTRL2_DCLOOPOFF_ENABLE      0
#define STV6120_CTRL2_DCLOOPOFF_DISABLE     1
#define STV6120_CTRL2_SDOFF_SHIFT         6
#define STV6120_CTRL2_SDOFF_OFF             0
#define STV6120_CTRL2_SDOFF_ON              1
#define STV6120_CTRL2_SYN_SHIFT           5
#define STV6120_CTRL2_SYN_OFF               0
#define STV6120_CTRL2_SYN_ON                1
#define STV6120_CTRL2_REFOUTSEL_SHIFT     4
#define STV6120_CTRL2_REFOUTSEL_VCC_DIV_2   0
#define STV6120_CTRL2_REFOUTSEL_1_25V       1
#define STV6120_CTRL2_BBGAIN_SHIFT        0
#define STV6120_CTRL2_BBGAIN_0DB       0x0
#define STV6120_CTRL2_BBGAIN_2DB       0x1
#define STV6120_CTRL2_BBGAIN_4DB       0x2
#define STV6120_CTRL2_BBGAIN_6DB       0x3
#define STV6120_CTRL2_BBGAIN_8DB       0x4
#define STV6120_CTRL2_BBGAIN_10DB      0x5
#define STV6120_CTRL2_BBGAIN_12DB      0x6
#define STV6120_CTRL2_BBGAIN_14DB      0x7
#define STV6120_CTRL2_BBGAIN_16DB      0x8

/* registers for tuner 1 */
#define STV6120_CTRL3  0x02

#define STV6120_CTRL4  0x03
#define STV6120_CTRL4_F_6_0_SHIFT  1
#define STV6120_CTRL4_F_6_0_MASK   0xfe
#define STV6120_CTRL4_N_9_SHIFT    0

#define STV6120_CTRL5  0x04

#define STV6120_CTRL6  0x05
#define STV6120_CTRL6_ICP_SHIFT      4
#define STV6120_CTRL6_ICP_MASK       0x70
#define STV6120_CTRL6_ICP_300UA           0
#define STV6120_CTRL6_ICP_325UA           1
#define STV6120_CTRL6_ICP_360UA           2
#define STV6120_CTRL6_ICP_400UA           3
/* note 400uA has 2 definitions */
#define STV6120_CTRL6_ICP_400UA_2         4
#define STV6120_CTRL6_ICP_450UA           5
#define STV6120_CTRL6_ICP_525UA           6
#define STV6120_CTRL6_ICP_600UA           7
#define STV6120_CTRL6_F_17_15_SHIFT  0
#define STV6120_CTRL6_F_17_15_MASK   0x07
#define STV6120_CTRL6_RESERVED  0x08

#define STV6120_CTRL7  0x06
#define STV6120_CTRL7_RCCLKOFF_SHIFT  7
#define STV6120_CTRL7_RCCLKOFF_ENABLE   0
#define STV6120_CTRL7_RCCLKOFF_DISABLE  1
#define STV6120_CTRL7_PDIV_SHIFT      5
#define STV6120_CTRL7_CF_SHIFT        0
#define STV6120_CTRL7_CF_5MHZ           0x00

#define STV6120_CTRL8  0x07
#define STV6120_CTRL8_TCAL_SHIFT     6
#define STV6120_CTRL8_TCAL_MASK      0xc0
#define STV6120_CTRL8_TCAL_DIV_1          0
#define STV6120_CTRL8_TCAL_DIV_2          1
#define STV6120_CTRL8_TCAL_DIV_4          2
#define STV6120_CTRL8_TCAL_DIV_8          3
#define STV6120_CTRL8_CALTIME_SHIFT  5
#define STV6120_CTRL8_CALTIME_500US       0
#define STV6120_CTRL8_CALTIME_1MS         1
#define STV6120_CTRL8_CFHF_SHIFT     0
#define STV6120_CTRL8_CFHF_MASK      0x1f

#define STV6120_STAT1  0x08
#define STV6120_STAT1_CALVCOSTRT_SHIFT   2
#define STV6120_STAT1_CALVCOSTRT_FINISHED   0
#define STV6120_STAT1_CALVCOSTRT_START      1
#define STV6120_STAT1_CALRCSTRT_SHIFT    1
#define STV6120_STAT1_CALRCSTRT_FINISHED    0
#define STV6120_STAT1_CALRCSTRT_START       1
#define STV6120_STAT1_LOCK_SHIFT         0
#define STV6120_STAT1_LOCK_NOT_IN_LOCK      0
#define STV6120_STAT1_LOCK_LOCKED           1
#define STV6120_STAT1_RESERVED   0x08

/* refsel fields for both tuners and path select */
#define STV6120_CTRL9  0x09
#define STV6120_CTRL9_RFSEL_2_SHIFT  2
#define STV6120_CTRL9_RFSEL_2_MASK   0x0c
#define STV6120_CTRL9_RFSEL_RFA_IN         0
#define STV6120_CTRL9_RFSEL_RFB_IN         1
#define STV6120_CTRL9_RFSEL_RFC_IN         2
#define STV6120_CTRL9_RFSEL_RFD_IN         3
#define STV6120_CTRL9_RFSEL_1_SHIFT  0
#define STV6120_CTRL9_RFSEL_1_MASK   0x03
#define STV6120_CTRL9  0x09
#define STV6120_CTRL9_RESERVED  0xf0

/* path select for both tuners */
#define STV6120_CTRL10 0x0a
#define STV6120_CTRL10_DEFAULT 0xbf
#define STV6120_CTRL10_ID_SHIFT       6
#define STV6120_CTRL10_ID_MASK        0xc0
#define STV6120_CTRL10_LNADON_SHIFT   5
#define STV6120_CTRL10_LNACON_SHIFT   4
#define STV6120_CTRL10_LNA_OFF            0
#define STV6120_CTRL10_LNA_ON             1
#define STV6120_CTRL10_LNABON_SHIFT   3
#define STV6120_CTRL10_LNAAON_SHIFT   2
#define STV6120_CTRL10_PATHON_2_SHIFT 1
#define STV6120_CTRL10_PATH_OFF           0
#define STV6120_CTRL10_PATH_ON            1
#define STV6120_CTRL10_PATHON_1_SHIFT 0

/* Regs CTRL11 to 18 and STAT2 are all Tuner2 regs */
#define STV6120_CTRL11 0x0b
#define STV6120_CTRL12 0x0c
#define STV6120_CTRL13 0x0d
#define STV6120_CTRL14 0x0e
#define STV6120_CTRL15 0x0f
#define STV6120_CTRL16 0x10
#define STV6120_CTRL17 0x11
#define STV6120_STAT2  0x12

/* test registers */
#define STV6120_CTRL18 0x13
#define STV6120_CTRL18_DEFAULT 0x00
#define STV6120_CTRL19 0x14
#define STV6120_CTRL19_DEFAULT 0x00

/* vco 1 amplfier and test */
#define STV6120_CTRL20 0x15
#define STV6120_CTRL20_VCOAMP_SHIFT  6
#define STV6120_CTRL20_VCOAMP_MASK   0xc0
#define STV6120_CTRL20_VCOAMP_AUTO        0
#define STV6120_CTRL20_VCOAMP_LOW         1
#define STV6120_CTRL20_VCOAMP_NORMAL      2
#define STV6120_CTRL20_VCOAMP_VERY_LOW    3
#define STV6120_CTRL20_RESERVED  0x0c

/* test registers */
#define STV6120_CTRL21 0x16
#define STV6120_CTRL21_DEFAULT 0x00
#define STV6120_CTRL22 0x17
#define STV6120_CTRL22_DEFAULT 0x00

/* vco 2 amplfier and test */
#define STV6120_CTRL23 0x18

#endif

