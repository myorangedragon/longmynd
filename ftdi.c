/* -------------------------------------------------------------------------------------------------- */
/* The LongMynd receiver: ftdi.c                                                                      */
/*    - an implementation of the Serit NIM controlling software for the MiniTiouner Hardware          */
/*    - implements all the ftdi i2c accessing routines apart from the usb stuff                       */
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
#include <unistd.h>
#include "ftdi.h"
#include "ftdi_usb.h"
#include "nim.h"
#include "errors.h"

/* -------------------------------------------------------------------------------------------------- */
/* ----------------- DEFINES ------------------------------------------------------------------------ */
/* -------------------------------------------------------------------------------------------------- */

#define FTDI_VID 0x0403
#define FTDI_PID 0x6010

#define FTDI_NUM_TRIES 10
#define FTDI_STOP_START_REPEATS 4
#define FTDI_RDWR_TIMEOUT 100
#define MSB_FALLING_EDGE_CLOCK_BYTE_IN  0x24
#define MSB_FALLING_EDGE_CLOCK_BYTE_OUT 0x11
#define MSB_RISING_EDGE_CLOCK_BIT_IN    0x22
#define MSB_FAILING_EDGE_CLOCK_BIT_IN   0x26

/*
FTDI GPIO Pins
LSB
 - AC0: NIM Reset
 - AC1: TS2SYNC
 - AC2: <unused>
 - AC3: <unused>
 - AC4: LNB Bias Enable
 - AC5: <unused>
 - AC6: <unused>
 - AC7: LNB Bias Voltage Select
MSB
*/

#define FTDI_GPIO_PINID_NIM_RESET  0
#define FTDI_GPIO_PINID_TS2SYNC   1
#define FTDI_GPIO_PINID_LNB_BIAS_ENABLE   4
#define FTDI_GPIO_PINID_LNB_BIAS_VSEL   7

/* -------------------------------------------------------------------------------------------------- */
/* ----------------- GLOBALS ------------------------------------------------------------------------ */
/* -------------------------------------------------------------------------------------------------- */

static int num_bytes_to_send = 0;
static uint8_t out_buffer[256];

/* Default GPIO value 0x6f = 0b01101111 = LNB Bias Off, LNB Voltage 12V, NIM not reset */
static uint8_t ftdi_gpio_value = 0x6f;

/* Default GPIO direction 0xf1 = 0b11110001 = LNB pins, NIM Reset are outputs, TS2SYNC is input (0 for in and 1 for out) */
static uint8_t ftdi_gpio_direction = 0xf1;

/* -------------------------------------------------------------------------------------------------- */
/* ----------------- ROUTINES ----------------------------------------------------------------------- */
/* -------------------------------------------------------------------------------------------------- */

/* -------------------------------------------------------------------------------------------------- */
uint8_t ftdi_setup_ftdi_io(void){
/* -------------------------------------------------------------------------------------------------- */
/* sets up the IO stages of the FTDI and syncs the comms                                              */
/* return: error code                                                                                 */
/* -------------------------------------------------------------------------------------------------- */
    uint8_t err=ERROR_NONE;
    uint8_t *in_buffer;
    uint8_t old_in_buffer=0;
    int i;
    int clock_divisor = 0x0095;

    printf("Flow: FTDI setup io\n");

    /* note we don't accumulate errors for this bit as we are trying to sync */
    /* first we send out a byte to act as our sync */
    num_bytes_to_send = 0;
    out_buffer[num_bytes_to_send++] = 0xAA;
    if (err==ERROR_NONE) ftdi_usb_i2c_write(out_buffer, num_bytes_to_send);
    num_bytes_to_send = 0;
    /* next we keep reading it until we get it back, and the reply before it */
    for (i=0; i<FTDI_NUM_TRIES; i++) {
        if (err==ERROR_NONE) ftdi_usb_i2c_read(&in_buffer);
        if ((old_in_buffer==0xFA) && (*in_buffer==0xAA)) {
             printf("      Status: MPSSE Synched ");
             printf("%.2X %.2X\n",*in_buffer,old_in_buffer);
             break;
        }
        old_in_buffer = *in_buffer;
    }

    out_buffer[num_bytes_to_send++]=0x8A;
    out_buffer[num_bytes_to_send++]=0x97;
    out_buffer[num_bytes_to_send++]=0x8D;
    if (err==ERROR_NONE) err=ftdi_usb_i2c_write(out_buffer, num_bytes_to_send);
    num_bytes_to_send = 0;

    out_buffer[num_bytes_to_send++]=0x80;
    out_buffer[num_bytes_to_send++]=0x13;
    out_buffer[num_bytes_to_send++]=0x13;

    out_buffer[num_bytes_to_send++]=0x82;
    out_buffer[num_bytes_to_send++]=0x6f;
    out_buffer[num_bytes_to_send++]=0xf1;

    out_buffer[num_bytes_to_send++] = 0x86;
    out_buffer[num_bytes_to_send++] = clock_divisor & 0xFF;
    out_buffer[num_bytes_to_send++] = (clock_divisor >> 8) & 0xFF;
    if (err==ERROR_NONE) err=ftdi_usb_i2c_write(out_buffer, num_bytes_to_send);
    num_bytes_to_send = 0;
    usleep(30000);

    out_buffer[num_bytes_to_send++] = 0x85;
    if (err==ERROR_NONE) err=ftdi_usb_i2c_write(out_buffer, num_bytes_to_send);
    num_bytes_to_send = 0;

    /* need to wait a while for it to work */
    usleep(30000);

    if (err!=ERROR_NONE) printf("ERROR: set mpsse mode\n");
    return err;
}

/* -------------------------------------------------------------------------------------------------- */
uint8_t ftdi_i2c_set_start(void) {
/* -------------------------------------------------------------------------------------------------- */
/* sets the i2c start condition on the i2c bus                                                        */
/* return: error code                                                                                 */
/* -------------------------------------------------------------------------------------------------- */
    int count;


    for (count=0; count<FTDI_STOP_START_REPEATS; count++) {
    	out_buffer[num_bytes_to_send++] = 0x80;
    	out_buffer[num_bytes_to_send++] = 0x03;
    	out_buffer[num_bytes_to_send++] = 0x13;
    }

    for (count=0; count<FTDI_STOP_START_REPEATS; count++) {
    	out_buffer[num_bytes_to_send++] = 0x80;
    	out_buffer[num_bytes_to_send++] = 0x01;
    	out_buffer[num_bytes_to_send++] = 0x13;
    }
    /* note we don't send this out yet as there will be more */

    return ERROR_NONE;
 }

/* -------------------------------------------------------------------------------------------------- */
uint8_t ftdi_i2c_set_stop(void) {
/* -------------------------------------------------------------------------------------------------- */
/* sets the i2c stop condition on the i2c bus                                                         */
/* return: error code                                                                                 */
/* -------------------------------------------------------------------------------------------------- */
    int count;

    for (count=0; count<FTDI_STOP_START_REPEATS; count++) {
        out_buffer[num_bytes_to_send++] = 0x80;
        out_buffer[num_bytes_to_send++] = 0x01;
        out_buffer[num_bytes_to_send++] = 0x13;
    }
    for (count=0; count<FTDI_STOP_START_REPEATS; count++) {
        out_buffer[num_bytes_to_send++] = 0x80;
        out_buffer[num_bytes_to_send++] = 0x03;
        out_buffer[num_bytes_to_send++] = 0x13;
    }
    out_buffer[num_bytes_to_send++] = 0x80;
    out_buffer[num_bytes_to_send++] = 0x03;
    out_buffer[num_bytes_to_send++] = 0x10;
    /* note we don't send this out yet as there will be more */

    return ERROR_NONE;
}

/* -------------------------------------------------------------------------------------------------- */
uint8_t ftdi_i2c_send_byte_check_ack(uint8_t b)
/* -------------------------------------------------------------------------------------------------- */
/* writes a byte to the i2c bus and checks we get an ack                                              */
/*      b: the byte to write out                                                                      */
/* return: error code                                                                                 */
/* -------------------------------------------------------------------------------------------------- */
{
    uint8_t *in_buffer;
    uint8_t err;

    out_buffer[num_bytes_to_send++] = 0x80;
    out_buffer[num_bytes_to_send++] = 0x00;
    out_buffer[num_bytes_to_send++] = 0x13;
    out_buffer[num_bytes_to_send++] = 0x11;
    out_buffer[num_bytes_to_send++] = 0x00;
    out_buffer[num_bytes_to_send++] = 0x00; /* Data length of 0x0000 means clock out 1 byte */
    out_buffer[num_bytes_to_send++] = b; /* Actual byte to clock out */
    out_buffer[num_bytes_to_send++] = 0x80;
    out_buffer[num_bytes_to_send++] = 0x00;
    out_buffer[num_bytes_to_send++] = 0x11;
    out_buffer[num_bytes_to_send++] = 0x27;
    out_buffer[num_bytes_to_send++] = 0x00;
    out_buffer[num_bytes_to_send++] = 0x87;
    err=ftdi_usb_i2c_write(out_buffer, num_bytes_to_send);
    num_bytes_to_send = 0;

    if (err==ERROR_NONE) err=ftdi_usb_i2c_read(&in_buffer);
    if ((err==ERROR_NONE) && ((*in_buffer&0x01)!=0)) {
        err=ERROR_I2C_NO_ACK;
    }

    return err;
}

/* -------------------------------------------------------------------------------------------------- */
int ftdi_i2c_read_byte_send_nak(uint8_t *b ) {
/* -------------------------------------------------------------------------------------------------- */
/* reads a byte from the i2c bus                                                                      */
/* *b: a byte to return the read value in                                                             */
/* return: error code                                                                                 */
/* -------------------------------------------------------------------------------------------------- */
    uint8_t err;
    uint8_t *in_buffer;

    /* first send off the commands to do the read */
    out_buffer[num_bytes_to_send++] = 0x80;
    out_buffer[num_bytes_to_send++] = 0x00;
    out_buffer[num_bytes_to_send++] = 0x13;
    out_buffer[num_bytes_to_send++] = 0x80;
    out_buffer[num_bytes_to_send++] = 0x00;
    out_buffer[num_bytes_to_send++] = 0x11; 
    out_buffer[num_bytes_to_send++] = 0x25;
    out_buffer[num_bytes_to_send++] = 0x00;
    out_buffer[num_bytes_to_send++] = 0x00;
    out_buffer[num_bytes_to_send++] = 0x87;
    err=ftdi_usb_i2c_write(out_buffer, num_bytes_to_send);
    num_bytes_to_send = 0;

    /* now read the value back */
    if (err==ERROR_NONE) err=ftdi_usb_i2c_read(&in_buffer);
    *b = *in_buffer;

    return err;
}


/* -------------------------------------------------------------------------------------------------- */
uint8_t ftdi_i2c_output(void) {
/* -------------------------------------------------------------------------------------------------- */
/* once other routines have set up a sequence of bytes to write, this actually sends them             */
/* return: error code */
/* -------------------------------------------------------------------------------------------------- */
    uint8_t err;

    err=ftdi_usb_i2c_write(out_buffer, num_bytes_to_send);
    num_bytes_to_send = 0;

    return err;
}

/* -------------------------------------------------------------------------------------------------- */
uint8_t ftdi_i2c_read_reg16(uint8_t addr, uint16_t reg, uint8_t *val) {
/* -------------------------------------------------------------------------------------------------- */
/* read an i2c 16 bit register from the nim                                                           */
/*   addr: the i2c buser address to access                                                            */
/*    reg: the i2c register to read                                                                   */
/*   *val: the return value for the register we have read                                             */
/* return: error code                                                                                 */
/* -------------------------------------------------------------------------------------------------- */
    int err;
    int i;
    int timeout=0;

    do {
        /* Send the register that needs to be read */
        for(i=0; i<FTDI_NUM_TRIES; i++) {
            err =ftdi_i2c_set_start();
            err|=ftdi_i2c_send_byte_check_ack(addr);
            err|=ftdi_i2c_send_byte_check_ack(reg>>8);
            err|=ftdi_i2c_send_byte_check_ack(reg&0xff);
            if (err==ERROR_NONE) break;
        }

        if (err==ERROR_NONE) {
            /* Read back the contents of that register */
            for(i=0; i<FTDI_NUM_TRIES; i++) {
                err =ftdi_i2c_set_start();
                err|=ftdi_i2c_send_byte_check_ack(addr|0x01);
                err|=ftdi_i2c_read_byte_send_nak(val);
                err|=ftdi_i2c_set_stop();
                err|=ftdi_i2c_output();
                if (err==ERROR_NONE) break;
            }
        }

        timeout++;

    } while ((err!=ERROR_NONE) && (timeout!=FTDI_RDWR_TIMEOUT));

    if (err!=ERROR_NONE) printf("ERROR: i2c read reg16 0x%.2x, 0x%.4x\n",addr,reg);

    return err;
}

/* -------------------------------------------------------------------------------------------------- */
uint8_t ftdi_i2c_write_reg16(uint8_t addr, uint16_t reg, uint8_t val) {
/* -------------------------------------------------------------------------------------------------- */
/* write an 8 bit value into a 16 bit  i2c register                                                   */
/*   addr: the i2c bus address to access                                                              */
/*    reg: the i2c register to write to                                                               */
/*    val: the value to write into the register                                                       */
/* return: error code                                                                                 */
/* -------------------------------------------------------------------------------------------------- */
    int err;
    int i;
    int timeout=0;

    do {
        for (i=0; i<FTDI_NUM_TRIES; i++) {
            err =ftdi_i2c_set_start();
            err|=ftdi_i2c_send_byte_check_ack(addr);
            err|=ftdi_i2c_send_byte_check_ack(reg>>8);
            err|=ftdi_i2c_send_byte_check_ack(reg&0xff);
            err|=ftdi_i2c_send_byte_check_ack(val);
            err|=ftdi_i2c_set_stop();
            err|=ftdi_i2c_output();
            if (err==ERROR_NONE) break;
        }

        timeout++;

    } while ((err!=ERROR_NONE) && (timeout!=FTDI_RDWR_TIMEOUT));

    if (err!=ERROR_NONE) printf("ERROR: i2c write reg16 0x%.2x, 0x%.4x, 0x%.2x\n",addr,reg, val);

    return err;
}

/* -------------------------------------------------------------------------------------------------- */
uint8_t ftdi_i2c_read_reg8(uint8_t addr, uint8_t reg, uint8_t *val) {
/* -------------------------------------------------------------------------------------------------- */
/* read an i2c 8 bit register from the nim                                                            */
/*   addr: the i2c bus address to access                                                              */
/*    reg: the i2c register to read                                                                   */
/*   *val: the return value for the register we have read                                             */
/* return: error code                                                                                 */
/* -------------------------------------------------------------------------------------------------- */
    uint8_t err;
    int i;
    int timeout=0;

    do {
        for (i=0; i<FTDI_NUM_TRIES; i++) {
            err =ftdi_i2c_set_start();
            err|=ftdi_i2c_send_byte_check_ack(addr);
            err|=ftdi_i2c_send_byte_check_ack(reg);
            err|=ftdi_i2c_set_stop();
            err|=ftdi_i2c_output();
            if (err==ERROR_NONE) break;
        }

        if (err==ERROR_NONE) {
            for (i=0; i<FTDI_NUM_TRIES; i++) {
                err =ftdi_i2c_set_start();
                err|=ftdi_i2c_send_byte_check_ack(addr|0x01);
                err|=ftdi_i2c_read_byte_send_nak(val);
                err|=ftdi_i2c_set_stop();
                err|=ftdi_i2c_output();
                if (err==ERROR_NONE) break;
            }
        }

        timeout++;

    } while ((err!=ERROR_NONE) && (timeout!=FTDI_RDWR_TIMEOUT));

    if (err!=ERROR_NONE) printf("ERROR: i2c read reg8 0x%.2x, 0x%.2x\n",addr,reg);

    return err;
}

/* -------------------------------------------------------------------------------------------------- */
uint8_t ftdi_i2c_write_reg8(uint8_t addr, uint8_t reg, uint8_t val) {
/* -------------------------------------------------------------------------------------------------- */
/* write an 8 bit value to an i2c 8 bit register in the nim                                           */
/*   addr: the i2c bus address to access                                                              */
/*    reg: the i2c register to write to                                                               */
/*    val: the value to write into the register                                                       */
/* return: error code                                                                                 */
/* -------------------------------------------------------------------------------------------------- */
    int err;
    int i;
    int timeout=0;

    do {
        for (i=0; i<FTDI_NUM_TRIES; i++) {
            err =ftdi_i2c_set_start();
            err|=ftdi_i2c_send_byte_check_ack(addr);
            err|=ftdi_i2c_send_byte_check_ack(reg);
            err|=ftdi_i2c_send_byte_check_ack(val);
            err|=ftdi_i2c_set_stop();
            err|=ftdi_i2c_output();
            if (err==ERROR_NONE) break;
        }

        timeout++;

    } while ((err!=ERROR_NONE) && (timeout!=FTDI_RDWR_TIMEOUT));

    if (err!=ERROR_NONE) printf("ERROR: i2c_write reg8 0x%.2x, 0x%.2x, 0x%.2x\n",addr,reg,val);

    return err;
}

/* -------------------------------------------------------------------------------------------------- */
uint8_t ftdi_gpio_write(uint8_t pin_id, bool pin_value)
/* -------------------------------------------------------------------------------------------------- */
/* write pin_value to the FTDI GPIO pin AC<pin_id>                                                    */
/* -------------------------------------------------------------------------------------------------- */
{
    printf("Flow: FTDI GPIO Write: pin %d -> value %d\n", pin_id, (int)pin_value);

    if(pin_value)
    {
        ftdi_gpio_value |= (1 << pin_id);
    }
    else
    {
        ftdi_gpio_value &= ~(1 << pin_id);
    }

    num_bytes_to_send = 0;
    out_buffer[num_bytes_to_send++] = 0x82; /* aka. MPSSE_CMD_SET_DATA_BITS_HIGHBYTE */
    out_buffer[num_bytes_to_send++] = ftdi_gpio_value;
    out_buffer[num_bytes_to_send++] = ftdi_gpio_direction;

    ftdi_usb_i2c_write(out_buffer, num_bytes_to_send);

    num_bytes_to_send = 0;

    return ERROR_NONE;
}

/* -------------------------------------------------------------------------------------------------- */
uint8_t ftdi_nim_reset(void)
/* -------------------------------------------------------------------------------------------------- */
/* toggle the reset line on the nim                                                                    */
/* -------------------------------------------------------------------------------------------------- */
{
    printf("Flow: FTDI nim reset\n");

    ftdi_gpio_write(FTDI_GPIO_PINID_NIM_RESET, 0);
    usleep(10000);

    ftdi_gpio_write(FTDI_GPIO_PINID_NIM_RESET, 1);
    usleep(10000);

    return ERROR_NONE;
}

/* -------------------------------------------------------------------------------------------------- */
uint8_t ftdi_set_polarisation_supply(bool supply_enable, bool supply_horizontal)
/* -------------------------------------------------------------------------------------------------- */
/* Controls RT5047A LNB Power Supply IC, fitted to an additional board.                               */
/* -------------------------------------------------------------------------------------------------- */
{
    if(supply_enable) {
        /* Set Voltage */
        if(supply_horizontal) {
            ftdi_gpio_write(FTDI_GPIO_PINID_LNB_BIAS_VSEL, 1);
        }
        else {
            ftdi_gpio_write(FTDI_GPIO_PINID_LNB_BIAS_VSEL, 0);
        }
        /* Then enable output */
        ftdi_gpio_write(FTDI_GPIO_PINID_LNB_BIAS_ENABLE, 1);
    }
    else {
        /* Disable output */
        ftdi_gpio_write(FTDI_GPIO_PINID_LNB_BIAS_ENABLE, 0);
    }

    return ERROR_NONE;
}

/* -------------------------------------------------------------------------------------------------- */
uint8_t ftdi_init(uint8_t usb_bus, uint8_t usb_addr) {
/* -------------------------------------------------------------------------------------------------- */
/* initialises the ftdi module on the minitiouner                                                    */
/* -------------------------------------------------------------------------------------------------- */
    uint8_t err;

    printf("Flow: FTDI init\n");

    err=ftdi_usb_init_i2c(usb_bus, usb_addr, FTDI_VID, FTDI_PID);
    if (err==ERROR_NONE) err=ftdi_usb_set_mpsse_mode_i2c();
    if (err==ERROR_NONE) err=ftdi_usb_init_ts(usb_bus, usb_addr, FTDI_VID, FTDI_PID);
    if (err==ERROR_NONE) err=ftdi_usb_set_mpsse_mode_ts();
    if (err==ERROR_NONE) err=ftdi_setup_ftdi_io();
    if (err==ERROR_NONE) err=ftdi_nim_reset();
    
    if (err!=ERROR_NONE) printf("ERROR: FTDI init\n");

    return err;
}

