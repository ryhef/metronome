/*
 * nameattach.h
 *
 *  Created on: 2014-03-20
 *      Author: Wade
 */

#ifndef NAMEATTACH_H_
#define NAMEATTACH_H_

#include <stdio.h>
#include <errno.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/dispatch.h>

#define ATTACH_POINT "metronome"


/* We specify the header as being at least a pulse */
typedef struct _pulse msg_header_t;

/* Our real data comes after the header */
typedef struct _my_data {
    msg_header_t hdr;
    int data;
} my_data_t;

#endif /* NAMEATTACH_H_ */
