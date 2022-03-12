// SPDX-License-Identifier: GPL-2.0

/*
 * Test of the store peer.
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
#define P         "S>"              /* Prompt of the store peer. */
#define MY_ADDR   "192.168.178.96"  /* IP address of the store peer. */
#define MY_PORT   62058             /* Port number of the store peer. */
#define HIS_ADDR  "192.168.178.1"   /* IP address of the mail peer. */
#define HIS_PORT  58062             /* Port number of the mail peer. */
#define BUF_SIZE  32                /* Size of the I/O buffer. */
#define LIMIT      9                /* Number of the send cycles. */

/*============================================================================
  MACROS
  ============================================================================*/
/* Vine trace with filter. */
#define TRACE(info_)  do { \
    if (sp.my_trace) \
	    printf info_; \
} while (0)

/*============================================================================
  LOCAL TYPE DEFINITIONS
  ============================================================================*/
/*============================================================================
  LOCAL DATA
  ============================================================================*/
/**
 * sp - state of the store peer.
 *
 * @my_trace:  if 1, the trace is active.
 * @inet_id:   inet id.
 * @limit:     number of the generator cycles.
 * @rd_count:  current read cycles.
 * @wr_count:  current write cycles.
 * @buf:       I/O buffer.
 * @rd_done:   if 1, all input messages have been consumed.
 * @wr_done:   if 1, all output messages have generated.
 **/
static struct sp_s {
	int   my_trace;
	int   inet_id;
	int   limit;
	int   rd_count;
	int   wr_count;
	char  buf[BUF_SIZE];
	int   rd_done;
	int   wr_done;
	int   done;
} sp;
	
/*============================================================================
  LOCAL FUNCTION PROTOTYPES
  ============================================================================*/
/*============================================================================
  LOCAL FUNCTIONS
  ============================================================================*/
/** 
 * store_write() - generate the output for the mail peer.
 *
 * Return:	0, if all data have been sent.
 **/
static int store_write(void)
{
	int n, rv;
	
	/* Test the final condition of the producer. */
	if (sp.wr_done)
		return 0;

	/* Test the cycle counter. */
	if (sp.wr_count > sp.limit) {
		/* Restart the send operation and get the fill level of the
		 * output buffer. */
		n = os_inet_sync(sp.inet_id);
		if (n > 0)
			return 1;

		/* Update the final condition of the producer. */
		sp.wr_done = 1;
		return 0;
	}
	
	/* Test the write cycle counter. */
	if (sp.wr_count == sp.limit) {
		/* Save the final response. */
		n = snprintf(sp.buf, BUF_SIZE, "DONE");
		OS_TRAP_IF(n >= BUF_SIZE);
	}
	else {
		/* Generate the test message. */
		n = snprintf(sp.buf, OS_BUF_SIZE, "%d", sp.wr_count);
		OS_TRAP_IF(n >= BUF_SIZE);
	}

	/* Include the message delimiter. */
	n++;
	
	/* Start an attempt of transmission. */
	rv = os_inet_write(sp.inet_id, sp.buf, n);
	OS_TRAP_IF(rv != 0 && rv != n);

	/* Test the success of the transmission. */
	if (rv == 0)
		return 1;

	/* Replace the message delimiter with EOS. */
	sp.buf[n -1] = '\0';
	
	/* The transmission has worked. */
	TRACE(("%s sent: b=\"%s\", n=%d\n", P, sp.buf, n));
	
	/* Increment the cycle counter. */
	sp.wr_count++;
	return 1;
}

/** 
 * store_read() - read the input from the mail peer.
 *
 * Return:	0, if all data have been received.
 **/
static int store_read(void)
{
	int n;
	
	/* Test the final condition of the consumer. */
	if (sp.rd_done)
		return 0;

	/* Wait for the data from the producer. */
	n = os_inet_read(sp.inet_id, sp.buf, BUF_SIZE);
	if (n < 1)
		return 1;
		
	TRACE(("%s rcvd: b=\"%s\", n=%d\n", P, sp.buf, n));
		       
	/* Test the end condition of the test. */
	if (os_strcmp(sp.buf, "DONE") == 0) {
		sp.rd_done = 1;
		return 0;
	}

	/* Convert and test the received counter. */
	n = strtol(sp.buf, NULL, 10);
	OS_TRAP_IF(sp.rd_count != n);

	/* Increment the receive counter. */
	sp.rd_count++;
	
	return 1;
}

/**
 * store_test() - produce/consume messages for/from the mail peer.
 *
 * Return:	None.
 **/
static void store_test(void)
{
	int busy_rd, busy_wr;
	
	/* Produce/consume messages for/from the mail peer.*/
	busy_rd = busy_wr = 1;
	while (busy_rd || busy_wr) {
		/* Read the input from the mail peer. */
		busy_rd = store_read();

		/* Generate the output for the mail peer. */
		busy_wr = store_write();

		/* Wait some microseconds. */
		usleep(1000);
	}
}

/**
 * store_cleanup() - free the resources of the store peer.
 *
 * Return:	None.
 **/
static void store_cleanup(void)
{
	/* Close the store socket. */
	os_inet_close(sp.inet_id);
}

/**
 * store_init() - allocate the resources for the test with the store peer.
 *
 * Return:	None.
 **/
static void store_init(void)
{
	int rv;
	
	/* Initialize the van OS resources. */
	os_init(1);
	
	/* Enable or disable the OS trace. */
	os_trace_button(0);

	/* Switch on the trace of the store peer. */
	sp.my_trace = 1;
	
	/* Allocate the socket resources. */
	
	sp.inet_id = os_inet_open(MY_ADDR, MY_PORT, HIS_ADDR, HIS_PORT);
	
	/* Save the number of the send cycles. */
	sp.limit = LIMIT;
	
	/* Wait for the connection establishment. */
	for (;;) {
		/* Wait for the connection request from the mail peer and send
		 * the response. */
		rv = os_inet_accept(sp.inet_id);
		if (rv == 0)
			return;
		
		/* Wait some microseconds. */
		sleep(0.1);
	}
}

/*============================================================================
  EXPORTED FUNCTIONS
  ============================================================================*/
/**
 * main() - start function of the store peer.
 *
 * Return:	0 or force a software trap.
 **/
int main(void)
{
	printf("%s store peer\n", P);

	/* Allocate the resources for the internet loop test. */
	store_init();

	/* Produce/consume messages for/from the mail peer. */
	store_test();

	/* Free the resources for the internet loop test. */
	store_cleanup();

	/* Release the van OS resources. */
	os_exit();
	
	return(0);
}
