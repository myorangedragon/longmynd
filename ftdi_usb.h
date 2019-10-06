/* -------------------------------------------------------------------------------------------------- */
/* The LongMynd receiver: ftdi_usb.h                                                                  */
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

#ifndef FTDI_USB_H
#define FTDI_USB_H

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>

/* Definitions for flow control */
#define USB_TIMEOUT 5000
#define USB_FAST_TIMEOUT 500

uint8_t ftdi_usb_i2c_write( uint8_t *, uint8_t);
uint8_t ftdi_usb_i2c_read( uint8_t **);
uint8_t ftdi_usb_set_mpsse_mode_i2c(void);
uint8_t ftdi_usb_set_mpsse_mode_ts(void);
uint8_t ftdi_usb_ts_read(uint8_t *, uint16_t *, uint32_t);
uint8_t ftdi_usb_init_i2c(uint8_t, uint8_t, uint16_t, uint16_t);
uint8_t ftdi_usb_init_ts(uint8_t, uint8_t, uint16_t, uint16_t);

#endif

