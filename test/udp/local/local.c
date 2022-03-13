// SPDX-License-Identifier: GPL-2.0

/*
 * Test of the local peer.
 *
 * Copyright (C) 2022 Gerald Schueller <gerald.schueller@web.de>
 */

/*============================================================================
  IMPORTED INCLUDE REFERENCES
  ============================================================================*/
#include "os.h"      /* Operating system: os_inet_sopen(). */

/*============================================================================
  EXPORTED INCLUDE REFERENCES
  ============================================================================*/
/*============================================================================
  LOCAL NAME CONSTANTS DEFINITIONS
  ============================================================================*/
#define P         "R>"         /* Prompt of the local peer. */
#define MY_ADDR   "127.0.0.1"  /* IP address of the van display. */
#define MY_PORT   58062        /* Port number of the van display. */
#define HIS_ADDR  "127.0.0.1"  /* IP address of the van controller. */
#define HIS_PORT  62058        /* Port number of the van controller. */
#define BUF_SIZE  32           /* Size of the I/O buffer. */
#define LIMIT      9           /* Number of the send cycles. */

/*============================================================================
  MACROS
  ============================================================================*/
/* Vine trace with filter. */
#define TRACE(info_)  do { \
    if (lp.my_trace) \
	    printf info_; \
} while (0)

/*============================================================================
  LOCAL TYPE DEFINITIONS
  ============================================================================*/
/*============================================================================
  LOCAL DATA
  ============================================================================*/
/**
 * lp - state of the local peer.
 *
 * @my_trace:  if 1, the trace is active.
 * @lp:        local peer.
 * @rp:        local peer.
 * @suspend:   control semaphore of the main process.
 * @mutex:     protect the critical section in the resume operation for the main
 *             process.
 **/
static struct v_s {
	int   my_trace;
	int   inet_id;
	int   limit;
	int   rd_count;
	int   wr_count;
	char  buf[BUF_SIZE];
	int   rd_done;
	int   wr_done;
	int   done;
} lp;
	
/*============================================================================
  LOCAL FUNCTION PROTOTYPES
  ============================================================================*/
/*============================================================================
  LOCAL FUNCTIONS
  ============================================================================*/
/** 
 * local_write() - generate the output for the local peer.
 *
 * Return:	0, if all data have been sent.
 **/
static int local_write(void)
{
	int n, rv;
	
	/* Test the final condition of the producer. */
	if (lp.wr_done)
		return 0;

	/* Test the cycle counter. */
	if (lp.wr_count > lp.limit) {
		/* Restart the send operation and get the fill level of the
		 * output buffer. */
		n = os_inet_sync(lp.inet_id);
		if (n > 0)
			return 1;

		/* Update the final condition of the producer. */
		lp.wr_done = 1;
		return 0;
	}
	
	/* Test the write cycle counter. */
	if (lp.wr_count == lp.limit) {
		/* Save the final response. */
		n = snprintf(lp.buf, BUF_SIZE, "DONE");
		OS_TRAP_IF(n >= BUF_SIZE);
	}
	else {
		/* Generate the test message. */
		n = snprintf(lp.buf, OS_BUF_SIZE, "%d", lp.wr_count);
		OS_TRAP_IF(n >= BUF_SIZE);
	}

	/* Include the message delimiter. */
	n++;
	
	/* Start an attempt of transmission. */
	rv = os_inet_write(lp.inet_id, lp.buf, n);
	OS_TRAP_IF(rv != 0 && rv != n);

	/* Test the success of the transmission. */
	if (rv == 0)
		return 1;

	/* Replace the message delimiter with EOS. */
	lp.buf[n -1] = '\0';
	
	/* The transmission has worked. */
	TRACE(("%s sent: b=\"%s\", n=%d\n", P, lp.buf, n));
	
	/* Increment the cycle counter. */
	lp.wr_count++;
	return 1;
}

/** 
 * local_read() - read the input from the local peer.
 *
 * Return:	0, if all data have been received.
 **/
static int local_read(void)
{
	int n;
	
	/* Test the final condition of the consumer. */
	if (lp.rd_done)
		return 0;

	/* Wait for the data from the producer. */
	n = os_inet_read(lp.inet_id, lp.buf, BUF_SIZE);
	if (n < 1)
		return 1;
		
	TRACE(("%s rcvd: b=\"%s\", n=%d\n", P, lp.buf, n));
		       
	/* Test the end condition of the test. */
	if (os_strcmp(lp.buf, "DONE") == 0) {
		lp.rd_done = 1;
		return 0;
	}

	/* Convert and test the received counter. */
	n = strtol(lp.buf, NULL, 10);
	OS_TRAP_IF(lp.rd_count != n);

	/* Increment the receive counter. */
	lp.rd_count++;
	
	return 1;
}

/**
 * local_test() - produce/consume messages for/from the local peer.
 *
 * Return:	None.
 **/
static void local_test(void)
{
	int busy_rd, busy_wr;
	
	/* Produce/consume messages for/from the local peer.*/
	busy_rd = busy_wr = 1;
	while (busy_rd || busy_wr) {
		/* Read the input from the local peer. */
		busy_rd = local_read();

		/* Generate the output for the local peer. */
		busy_wr = local_write();

		/* Wait some microseconds. */
		//usleep(1000);
	}
}

/**
 * local_cleanup() - free the resources of the local peer.
 *
 * Return:	None.
 **/
static void local_cleanup(void)
{
	/* Close the local socket. */
	os_inet_close(lp.inet_id);
}

/**
 * local_init() - allocate the resources for the test with the local peer.
 *
 * Return:	None.
 **/
static void local_init(void)
{
	int rv;
	
	/* Initialize the van OS resources. */
	os_init(0);
	
	/* Enable or disable the OS trace. */
	os_trace_button(0);

	/* Switch on the trace of the local peer. */
	lp.my_trace = 1;
	
	/* Allocate the socket resources. */
	
	lp.inet_id = os_inet_open(MY_ADDR, MY_PORT, HIS_ADDR, HIS_PORT);
	
	/* Save the number of the send cycles. */
	lp.limit = LIMIT;
	
	/* Wait for the connection establishment. */
	for (;;) {
		/* Send the connection establishment message to vcontroller.*/
		rv = os_inet_connect(lp.inet_id);
		if (rv == 0)
			return;
		
		/* Wait some microseconds. */
		usleep(1000);
	}
}

/*============================================================================
  EXPORTED FUNCTIONS
  ============================================================================*/
/**
 * main() - start function of the local peer.
 *
 * Return:	0 or force a software trap.
 **/
int main(void)
{
	printf("%s local peer\n", P);

	/* Allocate the resources for the internet loop test. */
	local_init();

	/* Produce/consume messages for/from the local peer. */
	local_test();

	/* Free the resources for the internet loop test. */
	local_cleanup();

	/* Release the van OS resources. */
	os_exit();
	
	return(0);
}
