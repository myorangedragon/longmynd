/* -------------------------------------------------------------------------------------------------- */
/* The LongMynd receiver: udp.c                                                                       */
/*    - an implementation of the Serit NIM controlling software for the MiniTiouner Hardware          */
/*    - linux udp handler to send the TS data to a remote display                                     */
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
#include <stdlib.h> 
#include <sys/socket.h> 
#include <arpa/inet.h> 
#include <netinet/in.h> 
#include "errors.h"
#include "udp.h"

/* -------------------------------------------------------------------------------------------------- */
/* ----------------- GLOBALS ------------------------------------------------------------------------ */
/* -------------------------------------------------------------------------------------------------- */

struct sockaddr_in servaddr_status; 
struct sockaddr_in servaddr_ts;
int sockfd_status; 
int sockfd_ts;

/* -------------------------------------------------------------------------------------------------- */
/* ----------------- DEFINES ------------------------------------------------------------------------ */
/* -------------------------------------------------------------------------------------------------- */

/* -------------------------------------------------------------------------------------------------- */
/* ----------------- ROUTINES ----------------------------------------------------------------------- */
/* -------------------------------------------------------------------------------------------------- */

/* -------------------------------------------------------------------------------------------------- */
uint8_t udp_ts_write(uint8_t *buffer, uint32_t len) {
/* -------------------------------------------------------------------------------------------------- */
/* takes a buffer and writes out the contents to udp socket                                           */
/* *buffer: the buffer that contains the data to be sent                                              */
/*     len: the length (number of bytes) of data to be sent                                           */
/*  return: error code                                                                                */
/* -------------------------------------------------------------------------------------------------- */
    uint8_t err=ERROR_NONE;
    int32_t remaining_len; /* note it is signed so can go negative */
    uint32_t write_size;

    remaining_len=len;
    /* we need to loop round sending 510 byte chunks so that we can skip the 2 extra bytes put in by */
    /* the FTDI chip every 512 bytes of USB message */
    while (remaining_len>0) {
        if (remaining_len>510) {
             /* calculate where to start in the buffer and how many bytes to send */
             write_size=510;
             sendto(sockfd_ts, &buffer[len-remaining_len], write_size, 0,
                                (const struct sockaddr *) &servaddr_ts,  sizeof(struct sockaddr)); 
             /* note we skip over the 2 bytes inserted by the FTDI */
             remaining_len-=512;
        } else {
             write_size=remaining_len;
             sendto(sockfd_ts, &buffer[len-remaining_len], write_size, 0,
                                (const struct sockaddr *) &servaddr_ts,  sizeof(struct sockaddr)); 
             remaining_len-=write_size; /* should be 0 if all went well */
        }
    }

    /* if someting went bad with our calcs, remaining will not be 0 */
    if ((err==ERROR_NONE) && (remaining_len!=0)) {
        printf("ERROR: UDP socket write incorrect number of bytes\n");
        err=ERROR_UDP_WRITE;
    }

    if (err!=ERROR_NONE) printf("ERROR: UDP socket ts write\n");

    return err;
}

/* -------------------------------------------------------------------------------------------------- */
uint8_t udp_status_write(uint8_t message, uint32_t data) {
/* -------------------------------------------------------------------------------------------------- */
/* takes a buffer and writes out the contents to udp socket                                           */
/* *buffer: the buffer that contains the data to be sent                                              */
/*     len: the length (number of bytes) of data to be sent                                           */
/*  return: error code                                                                                */
/* -------------------------------------------------------------------------------------------------- */
    uint8_t err=ERROR_NONE;
    char status_message[30];

    sprintf(status_message, "$%i,%i\n", message, data);

    sendto(sockfd_status, status_message, strlen(status_message), 0, (const struct sockaddr *)&servaddr_status,  sizeof(struct sockaddr)); 

    return err;
}

/* -------------------------------------------------------------------------------------------------- */
uint8_t udp_status_string_write(uint8_t message, char *data) {
/* -------------------------------------------------------------------------------------------------- */
/* takes a buffer and writes out the contents to udp socket                                           */
/* *buffer: the buffer that contains the data to be sent                                              */
/*     len: the length (number of bytes) of data to be sent                                           */
/*  return: error code                                                                                */
/* -------------------------------------------------------------------------------------------------- */
    uint8_t err=ERROR_NONE;
    char status_message[5+128];

    sprintf(status_message, "$%i,%s\n", message, data);

    sendto(sockfd_status, status_message, strlen(status_message), 0, (const struct sockaddr *)&servaddr_status,  sizeof(struct sockaddr)); 

    return err;
}


/* -------------------------------------------------------------------------------------------------- */
static uint8_t udp_init(struct sockaddr_in *servaddr_ptr, int *sockfd_ptr, char *udp_ip, int udp_port) {
/* -------------------------------------------------------------------------------------------------- */
/* initialises the udp socket                                                                         */
/*    udp_ip: the ip address (as a string) of the socket to open                                      */
/*  udp_port: the UDP port to be opened at the given IP address                                       */
/*    return: error code                                                                              */
/* -------------------------------------------------------------------------------------------------- */
    uint8_t err=ERROR_NONE;
  
    printf("Flow: UDP Init\n");

    /* Creat the socket  for IPv4 and UDP */
    if ((*sockfd_ptr = socket(AF_INET, SOCK_DGRAM, 0)) < 0 ) { 
        printf("ERROR: socket creation failed\n"); 
        err=ERROR_UDP_SOCKET_OPEN; 
    } else {
        /* setup all the destination fields */
        memset(servaddr_ptr, 0, sizeof(struct sockaddr_in)); 
        servaddr_ptr->sin_family = AF_INET; 
        servaddr_ptr->sin_port = htons(udp_port); 
        servaddr_ptr->sin_addr.s_addr = inet_addr(udp_ip); // INADDR_ANY; 
    }
    if (err!=ERROR_NONE) printf("ERROR: UDP init\n");

    return err;
}

uint8_t udp_status_init(char *udp_ip, int udp_port) {
    return udp_init(&servaddr_status, &sockfd_status, udp_ip, udp_port);
}

uint8_t udp_ts_init(char *udp_ip, int udp_port) {
    return udp_init(&servaddr_ts, &sockfd_ts, udp_ip, udp_port);
}

/* -------------------------------------------------------------------------------------------------- */
uint8_t udp_close(void) {
/* -------------------------------------------------------------------------------------------------- */
/* closes the udp socket                                                                              */
/* return: error code                                                                                 */
/* -------------------------------------------------------------------------------------------------- */
    uint8_t err=ERROR_NONE;
    int ret;

    printf("Flow: UDP Close\n");

    ret=close(sockfd_ts); 
    if (ret!=0) {
        err=ERROR_UDP_CLOSE;
        printf("ERROR: TS UDP close\n");
    }
    ret=close(sockfd_status); 
    if (ret!=0) {
        err=ERROR_UDP_CLOSE;
        printf("ERROR: Status UDP close\n");
    }

    return err;
}

