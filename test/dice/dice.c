// SPDX-License-Identifier: GPL-2.0

/*
 * dice - display cable entry points about I/O buffering.
 *
 * Copyright (C) 2022 Gerald Schueller <gerald.schueller@web.de>
 */

/*============================================================================
  IMPORTED INCLUDE REFERENCES
  ============================================================================*/
#include "os.h"  /* Operating system: os_sem_create(). */

/*============================================================================
  EXPORTED INCLUDE REFERENCES
  ============================================================================*/
/*============================================================================
  LOCAL NAME CONSTANTS DEFINITIONS
  ============================================================================*/
#define P  "D>"  /* Dice prompt. */

/*============================================================================
  MACROS
  ============================================================================*/
/*============================================================================
  LOCAL TYPE DEFINITIONS
  ============================================================================*/
/*============================================================================
  LOCAL DATA
  ============================================================================*/
/*============================================================================
  LOCAL FUNCTION PROTOTYPES
  ============================================================================*/
/*============================================================================
  LOCAL FUNCTIONS
  ============================================================================*/
/**
 * dice_write() - generate a buffered display message.
 *
 * @d_id:  end point id of the display.
 *
 * Return:	1, if the last message has been generated, otherwise 1.
 **/
static int dice_write(int d_id, int cycles)
{
	static int count = 0, done = 0;
	char buf[64];
	int size, n;

	/* Entry condition. */
	if (done) {
		/* Get size of the output buffer. */
		n = os_buffered(d_id, 1);
		return (n > 0) ? 0 : 1;
	}
	
	/* Save the buffer size. */
	size = 64;
	
	/* Test the write cycle counter. */
	if (count == cycles) {
		/* Create the final response. */
		n = snprintf(buf, size, "DONE");
		done = 1;
	}
	else {
		/* Create the digit sequence. */
		n = snprintf(buf, size, "%d", count);
	}

	/* Include end of string. */
	n++;

	/* Send the message to the controller. */
	n = os_bwrite(d_id, buf, n);
	if (n < 1) {
		done = 0;
		return 0;
	}
	
	/* Update the cycle counter. */
	count++;

	printf("%s sent: [b:\"%s\", s:%d]\n", P, buf, n);

	return 0;
}

/**
 * dice_read() - read and test a buffered controller message.
 *
 * @d_id:  end point id of the display.
 *
 * Return:	1, if the last message has been processed, otherwise 0.
 **/
static int dice_read(int d_id)
{
	static int count = 0, done = 0;
	char buf[64];
	int n;

	/* Entry condition. */
	if (done)
		return 1;
	
	/* Read all c ontroller packets. */
	n = os_bread(d_id, buf, 64);
	if (n < 1)
		return 0;
	
	printf("%s rcvd: [b:\"%s\", s:%d]\n", P, buf, n);

	/* Test the end condition of the test. */
	if (os_strcmp(buf, "DONE") == 0) {
		done = 1;
		return 1;
	}

	/* Convert and test the received counter. */
	n = strtol(buf, NULL, 10);
	OS_TRAP_IF(count != n);

	/* Update the cycle counter. */
	count++;

	return 0;
}

/*============================================================================
  EXPORTED FUNCTIONS
  ============================================================================*/
/**
 * main() - start function of the display channel entry points.
 *
 * Return:	0 or force a software trap.
 **/
int main(void)
{
	int cycles, sleep, d_id, done, rv;
	
	printf("Display cable entry operations\n");

	/* Define the number of the generator cycles and the sleep time in
	 * millisecond. */
	/* XXX */
	cycles = 99999;
	sleep  = 11;
	
	/* Initialize the operating system. */
	os_init(0);

	/* Disable the OS trace. */
	os_trace_button(0);

	/* Open the display entry point with I/O buffering of the cable between
	 * controller and display. */
	d_id = os_bopen("/display");

	/* I/O loop. */
	for (done = 0; ! done;) {
		/* Read and test a buffered controller message. */
		done = dice_read(d_id);
		
		/* Generate a buffered controller message. */
		rv = dice_write(d_id, cycles);
		done = done && rv;

		/* Simulate the display event loop. */
		os_clock_msleep(sleep);		
	}
	
	/* Close the buffered display entry point. */
	os_bclose(d_id);
	
	/* Release the OS resources. */
	os_exit();

	printf("\n%s executed successfully\n", P);
			
	return (0);
}
