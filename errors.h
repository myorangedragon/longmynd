/* -------------------------------------------------------------------------------------------------- */
/* The LongMynd receiver: errors.h                                                                    */
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

#ifndef ERRORS_H
#define ERRORS_H

/* possible error states */
#define ERROR_NONE        0
#define ERROR_ARGS        1
#define ERROR_ARGS_INPUT  2
#define ERROR_STATE       3
#define ERROR_DEMOD_STATE 4
#define ERROR_FTDI_READ_HIGH 5
#define ERROR_READ_DEMOD 6
#define ERROR_WRITE_DEMOD 7
#define ERROR_READ_OTHER 8
#define ERROR_WRITE_OTHER 9
#define ERROR_FTDI_USB_INIT_LIBUSB 101
#define ERROR_FTDI_USB_VID_PID 102
#define ERROR_FTDI_USB_CLAIM 103
#define ERROR_FTDI_USB_I2C_WRITE 104
#define ERROR_FTDI_USB_I2C_READ 105
#define ERROR_FTDI_USB_CMD 106
#define ERROR_FTDI_USB_INIT 107
#define ERROR_FTDI_SYNC_AA 10
#define ERROR_FTDI_SYNC_AB 11
#define ERROR_NIM_INIT 12
#define ERROR_LNA_ID 13
#define ERROR_TUNER_ID 14
#define ERROR_TUNER_LOCK_TIMEOUT 15
#define ERROR_TUNER_CAL_TIMEOUT 16
#define ERROR_TUNER_CAL_LOWPASS_TIMEOUT 17
#define ERROR_I2C_NO_ACK 18
#define ERROR_FTDI_I2C_WRITE_LEN 19
#define ERROR_FTDI_I2C_READ_LEN 20
#define ERROR_MPSSE 21
#define ERROR_DEMOD_INIT 22
#define ERROR_BAD_DEMOD_HUNT_STATE 23
#define ERROR_TS_FIFO_WRITE 24
#define ERROR_OPEN_TS_FIFO 25
#define ERROR_TS_FIFO_CREATE 26
#define ERROR_STATUS_FIFO_CREATE 27
#define ERROR_OPEN_STATUS_FIFO 28
#define ERROR_TS_FIFO_CLOSE 29
#define ERROR_STATUS_FIFO_CLOSE 30
#define ERROR_USB_TS_READ 31
#define ERROR_LNA_AGC_TIMEOUT 32
#define ERROR_DEMOD_PLL_TIMEOUT 33
#define ERROR_FTDI_USB_DEVICE_LIST 34
#define ERROR_FTDI_USB_BAD_DEVICE_NUM 35
#define ERROR_FTDI_USB_DEVICE_NUM_OPEN 36
#define ERROR_UDP_WRITE 37
#define ERROR_UDP_SOCKET_OPEN 38
#define ERROR_UDP_CLOSE 39
#define ERROR_VITERBI_PUNCTURE_RATE 40
#define ERROR_TS_BUFFER_MALLOC 41
#define ERROR_THREAD_ERROR 41

#endif

