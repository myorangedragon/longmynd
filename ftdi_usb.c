/* -------------------------------------------------------------------------------------------------- */
/* The LongMynd receiver: ftdi_usb.c                                                                  */
/*    - an implementation of the Serit NIM controlling software for the MiniTiouner Hardware          */
/*    - here we have all the usb interactions with the ftdi module                                    */
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
#include <libusb-1.0/libusb.h>
#include <memory.h>
#include <unistd.h>
#include "errors.h"
#include "ftdi_usb.h"
#include "ftdi.h"

/* -------------------------------------------------------------------------------------------------- */
/* ----------------- DEFINES ------------------------------------------------------------------------ */
/* -------------------------------------------------------------------------------------------------- */

#define FTDI_USB_READ_RETRIES 2

/* Requests */
#define SIO_RESET_REQUEST             SIO_RESET
#define SIO_SET_BAUDRATE_REQUEST      SIO_SET_BAUD_RATE
#define SIO_SET_DATA_REQUEST          SIO_SET_DATA
#define SIO_SET_FLOW_CTRL_REQUEST     SIO_SET_FLOW_CTRL
#define SIO_SET_MODEM_CTRL_REQUEST    SIO_MODEM_CTRL
#define SIO_POLL_MODEM_STATUS_REQUEST 0x05
#define SIO_SET_EVENT_CHAR_REQUEST    0x06
#define SIO_SET_ERROR_CHAR_REQUEST    0x07
#define SIO_SET_LATENCY_TIMER_REQUEST 0x09
#define SIO_GET_LATENCY_TIMER_REQUEST 0x0A
#define SIO_SET_BITMODE_REQUEST       0x0B
#define SIO_READ_PINS_REQUEST         0x0C
#define SIO_READ_EEPROM_REQUEST       0x90
#define SIO_WRITE_EEPROM_REQUEST      0x91
#define SIO_ERASE_EEPROM_REQUEST      0x92

#define LATENCY_MS	16

#define SIO_RESET          0 /* Reset the port */
#define SIO_MODEM_CTRL     1 /* Set the modem control register */
#define SIO_SET_FLOW_CTRL  2 /* Set flow control register */
#define SIO_SET_BAUD_RATE  3 /* Set baud rate */
#define SIO_SET_DATA       4 /* Set the data characteristics of the port */

#define SIO_RESET_SIO      0
#define SIO_RESET_PURGE_RX 1
#define SIO_RESET_PURGE_TX 2

#define FTDI_DEVICE_OUT_REQTYPE (LIBUSB_REQUEST_TYPE_VENDOR | LIBUSB_RECIPIENT_DEVICE | LIBUSB_ENDPOINT_OUT)
#define FTDI_DEVICE_IN_REQTYPE (LIBUSB_REQUEST_TYPE_VENDOR | LIBUSB_RECIPIENT_DEVICE | LIBUSB_ENDPOINT_IN)

#define FTDI_RX_CHUNK_SIZE 4096
#define FTDI_TX_CHUNK_SIZE 4096

/* -------------------------------------------------------------------------------------------------- */
/* ----------------- GLOBALS ------------------------------------------------------------------------ */
/* -------------------------------------------------------------------------------------------------- */

uint8_t rx_chunk[FTDI_RX_CHUNK_SIZE];

/* MPSSE bitbang modes */
enum ftdi_mpsse_mode
{
    BITMODE_RESET  = 0x00,    /* switch off bitbang mode, back to regular serial/FIFO */
    BITMODE_BITBANG= 0x01,    /* classical asynchronous bitbang mode, introduced with B-type chips */
    BITMODE_MPSSE  = 0x02,    /* MPSSE mode, available on 2232x chips */
    BITMODE_SYNCBB = 0x04,    /* synchronous bitbang mode, available on 2232x and R-type chips  */
    BITMODE_MCU    = 0x08,    /* MCU Host Bus Emulation mode, available on 2232x chips */
                              /* CPU-style fifo mode gets set via EEPROM */
    BITMODE_OPTO   = 0x10,    /* Fast Opto-Isolated Serial Interface Mode, available on 2232x chips  */
    BITMODE_CBUS   = 0x20,    /* Bitbang on CBUS pins of R-type chips, configure in EEPROM before */
    BITMODE_SYNCFF = 0x40,    /* Single Channel Synchronous FIFO mode, available on 2232H chips */
};

static libusb_device_handle *usb_device_handle_i2c; // interface 0, endpoints: 0x81, 0x02
static libusb_device_handle *usb_device_handle_ts; // interface 1, endpoints: 0x83, 0x04
static libusb_context *usb_context_i2c;
static libusb_context *usb_context_ts;

/* -------------------------------------------------------------------------------------------------- */
/* ----------------- ROUTINES ----------------------------------------------------------------------- */
/* -------------------------------------------------------------------------------------------------- */

/* -------------------------------------------------------------------------------------------------- */
uint8_t ftdi_usb_i2c_write( uint8_t *buffer, uint8_t len ){
/* -------------------------------------------------------------------------------------------------- */
/* writes data out to the usb                                                                         */
/* *buffer: the buffer containing the data to be written out                                          */
/*     len: the number of bytes to send                                                               */
/* return : error code                                                                                */
/* -------------------------------------------------------------------------------------------------- */
    uint8_t err=ERROR_NONE;
    int sent=0;
    int res;

    res=libusb_bulk_transfer(usb_device_handle_i2c, 0x02, buffer, len, &sent, USB_TIMEOUT);
    if (res<0) {
        printf("ERROR: USB Cmd Write failure %d\n",res);
        err=ERROR_FTDI_USB_CMD;
    }

    if ((err==ERROR_NONE) && (sent!=len)) {
        printf("ERROR: i2c write incorrect num bytes sent=%i, len=%i\n",sent,len);
        err=ERROR_FTDI_I2C_WRITE_LEN;
    }

    return err;
}

/* -------------------------------------------------------------------------------------------------- */
uint8_t ftdi_usb_i2c_read( uint8_t **buffer) {
/* -------------------------------------------------------------------------------------------------- */
/* reads one byte from the usb and returns it. Keeping any other data bytes for later                 */ 
/* Note: we only ever need to read one byte of actual data so we can avoid data copying by using the  */
/* internal buffers of the usb reads to keep the data                                                 */
/* *buffer: iretruned as a pointer the the actual data read into the usb                              */
/*     len: the number of bytes to read                                                               */
/* return : error code                                                                                */
/* -------------------------------------------------------------------------------------------------- */
    uint8_t err=ERROR_NONE;
    static int rxed=0;
    static int posn=0;
    int res;
    int n;

    /* if we have unused characters in the buffer then use them up first */
    if (posn!=rxed) {
        *buffer=&rx_chunk[posn++];
    } else {
        /* if we couldn't do it with data we already have then get a new buffer */
        /* the data may not be available immediatly so try a few times until it appears (or we error) */
        for (n=0; n<FTDI_USB_READ_RETRIES; n++) {
            /* we use endpoint 0x81 for the i2c traffic */
            if ((res=libusb_bulk_transfer(usb_device_handle_i2c, 0x81, rx_chunk, FTDI_RX_CHUNK_SIZE, &rxed, USB_TIMEOUT))<0) {
                printf("ERROR: USB Cmd Read failure %d\n",res);
                err=ERROR_FTDI_USB_CMD;
                break;
            }
            /* we always get 2 bytes header from the FTDI, so we only have valid data with more than this */
            if (rxed>2) break;
        }
        /* check we didn't timeout */
        if (n==FTDI_USB_READ_RETRIES) err=ERROR_FTDI_I2C_READ_LEN;
        else if (err==ERROR_NONE) {
            /* once we have good data, use it to fulfil the request */
            posn=2;
            *buffer=&rx_chunk[posn++];
        }
    }

    return err;
}

/* -------------------------------------------------------------------------------------------------- */
static uint8_t ftdi_usb_set_mpsse_mode(libusb_device_handle *_device_handle){
/* -------------------------------------------------------------------------------------------------- */
/* setup the FTDI USB interface and MPSEE mode                                                        */
/* return : error code                                                                                */
/* -------------------------------------------------------------------------------------------------- */
    uint16_t val;
    int res;
    uint8_t err=ERROR_NONE;

    printf("Flow: FTDI set mpsse mode\n");

    /* clear out the receive buffers */
    if ((res=libusb_control_transfer(_device_handle, FTDI_DEVICE_OUT_REQTYPE, SIO_RESET_REQUEST,
                                            SIO_RESET_PURGE_RX, 1, NULL, 0, USB_TIMEOUT))<0) {
        printf("ERROR: USB RX Purge failed %d",res);
        err=ERROR_MPSSE;
    }   

    /* clear out the transmit buffers */
    if ((res=libusb_control_transfer(_device_handle, FTDI_DEVICE_OUT_REQTYPE, SIO_RESET_REQUEST,
                                            SIO_RESET_PURGE_TX, 1, NULL, 0, USB_TIMEOUT))<0) {
        printf("ERROR: USB TX Purge failed %d",res);
        err=ERROR_MPSSE;
    }
    if ((res=libusb_control_transfer(_device_handle, FTDI_DEVICE_OUT_REQTYPE, SIO_RESET_REQUEST,
                                           SIO_RESET_SIO,      1, NULL, 0, USB_TIMEOUT))<0) {
        printf("ERROR: USB Reset failed %d",res);
        err=ERROR_MPSSE;
    }

    /* set the latence of the bus */
    if ((res=libusb_control_transfer(_device_handle, FTDI_DEVICE_OUT_REQTYPE, SIO_SET_LATENCY_TIMER_REQUEST,
                                         LATENCY_MS, 1, NULL, 0, USB_TIMEOUT))<0) {
        printf("ERROR: USB Set Latency failed %d",res);
        err=ERROR_MPSSE;
    }

    /* set the bit modes */
    val = (BITMODE_RESET<<8);
    if ((res=libusb_control_transfer(_device_handle, FTDI_DEVICE_OUT_REQTYPE, SIO_SET_BITMODE_REQUEST,
                                        val, 1, NULL, 0, USB_TIMEOUT))<0) {
        printf("USB Reset Bitmode failed %d\n",res);
        err=ERROR_MPSSE;
    }

    val = (BITMODE_MPSSE<<8);
    if ((res=libusb_control_transfer(_device_handle, FTDI_DEVICE_OUT_REQTYPE, SIO_SET_BITMODE_REQUEST,
                                        val, 1, NULL, 0, USB_TIMEOUT))<0) {
        printf("USB Set MPSSE failed %d\n",res);
        err=ERROR_MPSSE;
    }

    usleep(1000);

    return err;

}

uint8_t ftdi_usb_set_mpsse_mode_i2c(void){
    return ftdi_usb_set_mpsse_mode(usb_device_handle_i2c);
}

uint8_t ftdi_usb_set_mpsse_mode_ts(void){
    return ftdi_usb_set_mpsse_mode(usb_device_handle_ts);
}

/* -------------------------------------------------------------------------------------------------- */
static uint8_t ftdi_usb_init(libusb_context **usb_context_ptr, libusb_device_handle **usb_device_handle_ptr, int interface_num, uint8_t usb_bus, uint8_t usb_addr, uint16_t vid, uint16_t pid) {
/* -------------------------------------------------------------------------------------------------- */
/* initialise the usb device of choice (or via vid/pid if no USB selected)                            */
/* return : error code                                                                                */
/* -------------------------------------------------------------------------------------------------- */
    uint8_t err=ERROR_NONE;
    ssize_t count;
    libusb_device **usb_device_list;
    int error_code;
    struct libusb_device_descriptor usb_descriptor;
    libusb_device *usb_candidate_device;
    uint8_t usb_device_count;

    printf("Flow: FTDI USB init\n");

    if (libusb_init(usb_context_ptr)<0) {
    	printf("ERROR: Unable to initialise LIBUSB\n");
        err=ERROR_FTDI_USB_INIT_LIBUSB;
    }

    /* turn on debug */
    #if LIBUSB_API_VERSION >= 0x01000106
    libusb_set_option(*usb_context_ptr, LIBUSB_LOG_LEVEL_INFO);
    #else
    libusb_set_debug(*usb_context_ptr, LIBUSB_LOG_LEVEL_INFO);
    #endif

    /* now we need to decide if we are opening by VID and PID or by device number */
    if ((err==ERROR_NONE) && (usb_bus==0) && (usb_addr==0)) {
        /* if we are using vid and pid it is easy */
        if ((*usb_device_handle_ptr = libusb_open_device_with_vid_pid(*usb_context_ptr, vid, pid))==NULL) {
            printf("ERROR: Unable to open device with VID and PID\n");
            printf("       Is the USB cable plugged in?\n");
            err=ERROR_FTDI_USB_VID_PID;
        }

    /* if  we are finding by usb device number then we have to take a look at the IDs to check we are */
    /* being asked to open the right one. sTto do this we get a list of all the USB devices on the system */
    } else if (err==ERROR_NONE) { 
        printf("Flow: Searching for bus/device=%i,%i\n",usb_bus,usb_addr);
        count=libusb_get_device_list(*usb_context_ptr, &usb_device_list);
        if (count<=0) {
            printf("ERROR: failed to get the list of devices\n");
            err=ERROR_FTDI_USB_DEVICE_LIST;
        }

        if (err==ERROR_NONE) {
            /* we need to find the device we have been told to use */
            for (usb_device_count=0; usb_device_count<count; usb_device_count++) {
                if ((libusb_get_bus_number(usb_device_list[usb_device_count])==usb_bus) &&
                    (libusb_get_device_address(usb_device_list[usb_device_count])==usb_addr)) break;
            }
        }
        if ((err==ERROR_NONE) && (usb_device_count==count)) {
            printf("ERROR: invalid USB bus/device number\n");
            err=ERROR_FTDI_USB_BAD_DEVICE_NUM;
        } else if(err==ERROR_NONE) {

            /* now we check our one has the right VID and PID */
            usb_candidate_device=usb_device_list[usb_device_count];
            /* some of it we have to do looking in the device descriptor */
            error_code=libusb_get_device_descriptor(usb_candidate_device, &usb_descriptor);
            /* if we have found our one then we can start using it */
            if ((usb_descriptor.idVendor==vid) && (usb_descriptor.idProduct==pid)) {
                /* first we open it */
                error_code=libusb_open(usb_candidate_device, usb_device_handle_ptr);
                if (error_code==0) printf("      Status: successfully opened USB Device %i,%i\n",
                                                                        usb_bus,usb_addr);
                else {
                    printf("ERROR: Unable to open Minitiouner\n");
                    err=ERROR_FTDI_USB_DEVICE_NUM_OPEN;
                }
            } else {
                printf("ERROR: This bus/device is not a Minitiouner\n");
                err=ERROR_FTDI_USB_DEVICE_NUM_OPEN;
            }
        }
        /* importantly, we now free up the list */
        libusb_free_device_list(usb_device_list, 1);
    }

    if (err==ERROR_NONE) {
        /* now we should have the device handle of the device we are going to us */
        /* we have two interfaces on the ftdi device (0 and 1) */
        /* so next we make sure we are the only people using this device and this */
        if (libusb_kernel_driver_active(*usb_device_handle_ptr, interface_num)) libusb_detach_kernel_driver(*usb_device_handle_ptr, interface_num);

        /* finally we claim both interfaces as ours */
        if (libusb_claim_interface(*usb_device_handle_ptr, interface_num)<0) {
            libusb_close(*usb_device_handle_ptr);
            libusb_exit(*usb_context_ptr);
        	printf("ERROR: Unable to claim interface\n");
            err=ERROR_FTDI_USB_CLAIM;
        }
    }

    return err;
}

uint8_t ftdi_usb_init_i2c(uint8_t usb_bus, uint8_t usb_addr, uint16_t vid, uint16_t pid) {
    return ftdi_usb_init(&usb_context_i2c, &usb_device_handle_i2c, 0, usb_bus, usb_addr, vid, pid);
}

uint8_t ftdi_usb_init_ts(uint8_t usb_bus, uint8_t usb_addr, uint16_t vid, uint16_t pid) {
    return ftdi_usb_init(&usb_context_ts, &usb_device_handle_ts, 1, usb_bus, usb_addr, vid, pid);
}

/* -------------------------------------------------------------------------------------------------- */
uint8_t ftdi_usb_ts_read(uint8_t *buffer, uint16_t *len, uint32_t frame_size) {
/* -------------------------------------------------------------------------------------------------- */
/* every now and again we check to see if there is any transport stream available                     */
/* *buffer: the buffer to collect the ts data into                                                    */
/*    *len: how many bytes we put into the buffer                                                     */
/* return : error code                                                                                */
/* -------------------------------------------------------------------------------------------------- */
    uint8_t err=ERROR_NONE;
    int rxed=0;
    int res=0;

    /* the TS traffic is on endpoint 0x83 */
    res=libusb_bulk_transfer(usb_device_handle_ts, 0x83, buffer, frame_size, &rxed, USB_FAST_TIMEOUT);

    if (res<0) {
        printf("ERROR: USB TS Data Read %i (%s), received %i\n",res,libusb_error_name(res),rxed);
        err=ERROR_USB_TS_READ;
    } else *len=rxed; /* just type converting */

    if (err!=ERROR_NONE) printf("ERROR: FTDI USB ts read\n");

    return err;
}
