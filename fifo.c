/* -------------------------------------------------------------------------------------------------- */
/* The LongMynd receiver: fifo.c                                                                      */
/*    - an implementation of the Serit NIM controlling software for the MiniTiouner Hardware          */
/*    - linux fifo handlers for the transport stream (TS) and the status stream (status)              */
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
#include <string.h> 
#include <fcntl.h> 
#include <sys/stat.h> 
#include <sys/types.h> 
#include <stdint.h>
#include <unistd.h>
#include <stdbool.h>
#include "errors.h"
#include "fifo.h"

/* -------------------------------------------------------------------------------------------------- */
/* ----------------- GLOBALS ------------------------------------------------------------------------ */
/* -------------------------------------------------------------------------------------------------- */

int fd_ts_fifo;
int fd_status_fifo;

/* -------------------------------------------------------------------------------------------------- */
/* ----------------- ROUTINES ----------------------------------------------------------------------- */
/* -------------------------------------------------------------------------------------------------- */

/* -------------------------------------------------------------------------------------------------- */
uint8_t fifo_ts_write(uint8_t *buffer, uint32_t len) {
/* -------------------------------------------------------------------------------------------------- */
/* takes a buffer and writes out the contents to the ts fifo but removes the unwanted bytes           */
/* *buffer: the buffer that contains the data to be sent                                              */
/*     len: the length (number of bytes) of data to be sent                                           */
/*  return: error code                                                                                */
/* -------------------------------------------------------------------------------------------------- */
    uint8_t err=ERROR_NONE;
    int ret;
    int32_t remaining_len; /* note it is signed so can go negative */
    uint32_t write_size;

    remaining_len=len;
    /* we need to loop round sending 510 byte chunks so that we can skip the 2 extra bytes put in by */
    /* the FTDI chip every 512 bytes of USB message */
    while (remaining_len>0) {
        if (remaining_len>510) {
             /* calculate where to start in the buffer and how many bytes to send */
             write_size=510;
             ret=write(fd_ts_fifo, &buffer[len-remaining_len], write_size); 
             /* note we skip over the 2 bytes inserted by the FTDI */
             remaining_len-=512;
        } else {
             write_size=remaining_len;
             ret=write(fd_ts_fifo, &buffer[len-remaining_len], write_size);
             remaining_len-=write_size; /* should be 0 if all went well */
        }
        if (ret!=(int)write_size) {
            printf("ERROR: ts fifo write\n");
            err=ERROR_TS_FIFO_WRITE;
            break;
        }
    }

    /* if someting went bad with our calcs, remaining will not be 0 */
    if ((err==ERROR_NONE) && (remaining_len!=0)) {
        printf("ERROR: ts fifo write incorrect number of bytes\n");
        err=ERROR_TS_FIFO_WRITE;
    }

    if (err!=ERROR_NONE) printf("ERROR: fifo ts write\n");

    return err;
}

/* -------------------------------------------------------------------------------------------------- */
uint8_t fifo_status_write(uint8_t message, uint32_t data) {
/* -------------------------------------------------------------------------------------------------- */
/* *message: the string to write out that identifies the status message                               */
/*     data: an integer to be sent out (as a decimal number string)                                   */
/*   return: error code                                                                               */
/* -------------------------------------------------------------------------------------------------- */
    uint8_t err=ERROR_NONE;
    int ret;
    char status_message[30];

    sprintf(status_message, "$%i,%i\n", message, data);
    ret=write(fd_status_fifo, status_message, strlen(status_message));
    if (ret!=(int)strlen(status_message)) {
        printf("ERROR: status fifo write\n");
        err=ERROR_TS_FIFO_WRITE;
    }

    if (err!=ERROR_NONE) printf("ERROR: fifo status write\n");

    return err;
}

/* -------------------------------------------------------------------------------------------------- */
uint8_t fifo_status_string_write(uint8_t message, char *data) {
/* -------------------------------------------------------------------------------------------------- */
/* *message: the string to write out that identifies the status message                               */
/*     data: an integer to be sent out (as a decimal number string)                                   */
/*   return: error code                                                                               */
/* -------------------------------------------------------------------------------------------------- */
    uint8_t err=ERROR_NONE;
    int ret;
    char status_message[5+128];

    sprintf(status_message, "$%i,%s\n", message, data);
    ret=write(fd_status_fifo, status_message, strlen(status_message));
    if (ret!=(int)strlen(status_message)) {
        printf("ERROR: status fifo write\n");
        err=ERROR_TS_FIFO_WRITE;
    }

    if (err!=ERROR_NONE) printf("ERROR: fifo status write\n");

    return err;
}

/* -------------------------------------------------------------------------------------------------- */
static uint8_t fifo_init(int *fd_ptr, char *fifo_path) {
/* -------------------------------------------------------------------------------------------------- */
/* initialises the ts and status fifos                                                                */
/*     ts_fifo: the name of the fifo to use for the TS                                                */
/* status_fifo: the name of the fifo to use for the status output                                     */
/*      return: error code                                                                            */
/* -------------------------------------------------------------------------------------------------- */
    uint8_t err=ERROR_NONE;
  
    printf("Flow: Fifo Init\n");

    /* if we are using the TS FIFO then set it up first */
    *fd_ptr = open(fifo_path, O_WRONLY); 
    if (err==ERROR_NONE) {
        if (*fd_ptr<0) {
            printf("ERROR: Failed to open fifo %s\n",fifo_path);
            err=ERROR_OPEN_TS_FIFO;
        } else printf("      Status: opened fifo ok\n");
    }

    if (err!=ERROR_NONE) printf("ERROR: fifo init\n");

    return err;
}

uint8_t fifo_ts_init(char *fifo_path) {
    return fifo_init(&fd_ts_fifo, fifo_path);
}

uint8_t fifo_status_init(char *fifo_path) {
    return fifo_init(&fd_status_fifo, fifo_path);
}

/* -------------------------------------------------------------------------------------------------- */
uint8_t fifo_close(bool ignore_ts_fifo) {
/* ------------------------------------------------------------------------------------------------- */
/* closes the fifo's                                                                                  */
/* return: error code                                                                                 */
/* -------------------------------------------------------------------------------------------------- */
    uint8_t err;
    int ret;

    if (!ignore_ts_fifo) {
        ret=close(fd_ts_fifo);
        if (ret!=0) {
            printf("ERROR: ts fifo close\n");
            err=ERROR_TS_FIFO_CLOSE;
        }
    }

    ret=close(fd_status_fifo); 
    if (ret!=0) {
        printf("ERROR: status fifo close\n");
        err=ERROR_STATUS_FIFO_CLOSE;
    }

    if (err!=ERROR_NONE) printf("ERROR: fifo close\n");

    return err;
}

