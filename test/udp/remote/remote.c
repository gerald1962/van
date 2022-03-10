// SPDX-License-Identifier: GPL-2.0

/*
 * Test of the remote peer.
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
#define P         "R>"         /* Prompt of the remote peer. */
#define MY_ADDR   "127.0.0.1"  /* IP address of the van display. */
#define MY_PORT   62058        /* Port number of the van controller. */
#define HIS_ADDR  "127.0.0.1"  /* IP address of the van controller. */
#define HIS_PORT  58062        /* Port number of the van display. */
#define BUF_SIZE  32           /* Size of the I/O buffer. */
#define LIMIT      9           /* Number of the send cycles. */

/*============================================================================
  MACROS
  ============================================================================*/
/* Vine trace with filter. */
#define TRACE(info_)  do { \
    if (rp.my_trace) \
	    printf info_; \
} while (0)

/*============================================================================
  LOCAL TYPE DEFINITIONS
  ============================================================================*/
/*============================================================================
  LOCAL DATA
  ============================================================================*/
/**
 * rp - state of the remote peer.
 *
 * @my_trace:  if 1, the trace is active.
 * @lp:        local peer.
 * @rp:        remote peer.
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
} rp;
	
/*============================================================================
  LOCAL FUNCTION PROTOTYPES
  ============================================================================*/
/*============================================================================
  LOCAL FUNCTIONS
  ============================================================================*/
/** 
 * remote_write() - generate the output for the local peer.
 *
 * Return:	0, if all data have been sent.
 **/
static int remote_write(void)
{
	int n, rv;
	
	/* Test the final condition of the producer. */
	if (rp.wr_done)
		return 0;

	/* Test the cycle counter. */
	if (rp.wr_count > rp.limit) {
		/* Restart the send operation and get the fill level of the
		 * output buffer. */
		n = os_inet_sync(rp.inet_id);
		if (n > 0)
			return 1;

		/* Update the final condition of the producer. */
		rp.wr_done = 1;
		return 0;
	}
	
	/* Test the write cycle counter. */
	if (rp.wr_count == rp.limit) {
		/* Save the final response. */
		n = snprintf(rp.buf, BUF_SIZE, "DONE");
		OS_TRAP_IF(n >= BUF_SIZE);
	}
	else {
		/* Generate the test message. */
		n = snprintf(rp.buf, OS_BUF_SIZE, "%d", rp.wr_count);
		OS_TRAP_IF(n >= BUF_SIZE);
	}

	/* Include the message delimiter. */
	n++;
	
	/* Start an attempt of transmission. */
	rv = os_inet_write(rp.inet_id, rp.buf, n);
	OS_TRAP_IF(rv != 0 && rv != n);

	/* Test the success of the transmission. */
	if (rv == 0)
		return 1;

	/* Replace the message delimiter with EOS. */
	rp.buf[n -1] = '\0';
	
	/* The transmission has worked. */
	TRACE(("%s sent: b=\"%s\", n=%d\n", P, rp.buf, n));
	
	/* Increment the cycle counter. */
	rp.wr_count++;
	return 1;
}

/** 
 * remote_read() - read the input from the local peer.
 *
 * Return:	0, if all data have been received.
 **/
static int remote_read(void)
{
	int n;
	
	/* Test the final condition of the consumer. */
	if (rp.rd_done)
		return 0;

	/* Wait for the data from the producer. */
	n = os_inet_read(rp.inet_id, rp.buf, BUF_SIZE);
	if (n < 1)
		return 1;
		
	TRACE(("%s rcvd: b=\"%s\", n=%d\n", P, rp.buf, n));
		       
	/* Test the end condition of the test. */
	if (os_strcmp(rp.buf, "DONE") == 0) {
		rp.rd_done = 1;
		return 0;
	}

	/* Convert and test the received counter. */
	n = strtol(rp.buf, NULL, 10);
	OS_TRAP_IF(rp.rd_count != n);

	/* Increment the receive counter. */
	rp.rd_count++;
	
	return 1;
}

/**
 * remote_wait() - wait for the identifier from the local peer.
 *
 * Return:	None.
 **/
static void remote_wait(void)
{
	/* Meeting with the local peer. */
	for (;;) {
		/* Wait for the identifier from the local peer. */
		rv = os_inet_wait(rp.inet_id, "hello vcontroller");
		if (rv == 0)
			return;

		/* Wait some time, until the local peer is present. */
		usleep(1);
	}	
}

/**
 * remote_test() - produce/consume messages for/from the local peer.
 *
 * Return:	None.
 **/
static void remote_test(void)
{
	int busy_rd, busy_wr;

	/* Produce/consume messages for/from the local peer.*/
	busy_rd = busy_wr = 1;
	while (busy_rd || busy_wr) {
		/* Wait for the identifier from the local peer. */
		remote_wait();

		/* Read the input from the local peer. */
		busy_rd = remote_read();

		/* Generate the output for the local peer. */
		busy_wr = remote_write();

		/* Wait some microseconds. */
		usleep(1000);
	}
}

/**
 * remote_cleanup() - free the resources of the remote peer.
 *
 * Return:	None.
 **/
static void remote_cleanup(void)
{
	/* Close the remote socket. */
	os_inet_close(rp.inet_id);
}

/**
 * remote_init() - allocate the resources for the test with the remote peer.
 *
 * Return:	None.
 **/
static void remote_init(void)
{
	/* Initialize the van OS resources. */
	os_init(1);
	
	/* Enable or disable the OS trace. */
	os_trace_button(0);

	/* Switch on the trace of the remote peer. */
	rp.my_trace = 1;
	
	/* Allocate the socket resources. */
	rp.inet_id = os_inet_open(MY_ADDR, MY_PORT, HIS_ADDR, HIS_PORT);
	
	/* Save the number of the send cycles. */
	rp.limit = LIMIT;
}

/*============================================================================
  EXPORTED FUNCTIONS
  ============================================================================*/
/**
 * main() - start function of the remote peer.
 *
 * Return:	0 or force a software trap.
 **/
int main(void)
{
	printf("%s remote peer\n", P);

	/* Allocate the resources for the internet loop test. */
	remote_init();

	/* Produce/consume messages for/from the local peer. */
	remote_test();

	/* Free the resources for the internet loop test. */
	remote_cleanup();

	/* Release the van OS resources. */
	os_exit();
	
	return(0);
}
