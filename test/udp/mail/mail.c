// SPDX-License-Identifier: GPL-2.0

/*
 * Test of the mail peer.
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
#define P         "M>"              /* Prompt of the mail peer. */
#define MY_ADDR   "10.0.2.15"       /* IP address of the mail peer. */
#define MY_PORT   58062             /* Port number of the mail peer. */
#define HIS_ADDR  "192.168.178.96"  /* IP address of the store peer. */
#define HIS_PORT  62058             /* Port number of the store peer. */
#define BUF_SIZE  32                /* Size of the I/O buffer. */
#define LIMIT      9                /* Number of the send cycles. */

/*============================================================================
  MACROS
  ============================================================================*/
/* Vine trace with filter. */
#define TRACE(info_)  do { \
    if (mp.my_trace) \
	    printf info_; \
} while (0)

/*============================================================================
  LOCAL TYPE DEFINITIONS
  ============================================================================*/
/*============================================================================
  LOCAL DATA
  ============================================================================*/
/**
 * mp - state of the mail peer.
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
static struct mp_s {
	int   my_trace;
	int   inet_id;
	int   limit;
	int   rd_count;
	int   wr_count;
	char  buf[BUF_SIZE];
	int   rd_done;
	int   wr_done;
	int   done;
} mp;
	
/*============================================================================
  LOCAL FUNCTION PROTOTYPES
  ============================================================================*/
/*============================================================================
  LOCAL FUNCTIONS
  ============================================================================*/
/** 
 * mail_write() - generate the output for the store peer.
 *
 * Return:	0, if all data have been sent.
 **/
static int mail_write(void)
{
	int n, rv;
	
	/* Test the final condition of the producer. */
	if (mp.wr_done)
		return 0;

	/* Test the cycle counter. */
	if (mp.wr_count > mp.limit) {
		/* Restart the send operation and get the fill level of the
		 * output buffer. */
		n = os_inet_sync(mp.inet_id);
		if (n > 0)
			return 1;

		/* Update the final condition of the producer. */
		mp.wr_done = 1;
		return 0;
	}
	
	/* Test the write cycle counter. */
	if (mp.wr_count == mp.limit) {
		/* Save the final response. */
		n = snprintf(mp.buf, BUF_SIZE, "DONE");
		OS_TRAP_IF(n >= BUF_SIZE);
	}
	else {
		/* Generate the test message. */
		n = snprintf(mp.buf, OS_BUF_SIZE, "%d", mp.wr_count);
		OS_TRAP_IF(n >= BUF_SIZE);
	}

	/* Include the message delimiter. */
	n++;
	
	/* Start an attempt of transmission. */
	rv = os_inet_write(mp.inet_id, mp.buf, n);
	OS_TRAP_IF(rv != 0 && rv != n);

	/* Test the success of the transmission. */
	if (rv == 0)
		return 1;

	/* Replace the message delimiter with EOS. */
	mp.buf[n -1] = '\0';
	
	/* The transmission has worked. */
	TRACE(("%s sent: b=\"%s\", n=%d\n", P, mp.buf, n));
	
	/* Increment the cycle counter. */
	mp.wr_count++;
	return 1;
}

/** 
 * mail_read() - read the input from the store peer.
 *
 * Return:	0, if all data have been received.
 **/
static int mail_read(void)
{
	int n;
	
	/* Test the final condition of the consumer. */
	if (mp.rd_done)
		return 0;

	/* Wait for the data from the producer. */
	n = os_inet_read(mp.inet_id, mp.buf, BUF_SIZE);
	if (n < 1)
		return 1;
		
	TRACE(("%s rcvd: b=\"%s\", n=%d\n", P, mp.buf, n));
		       
	/* Test the end condition of the test. */
	if (os_strcmp(mp.buf, "DONE") == 0) {
		mp.rd_done = 1;
		return 0;
	}

	/* Convert and test the received counter. */
	n = strtol(mp.buf, NULL, 10);
	OS_TRAP_IF(mp.rd_count != n);

	/* Increment the receive counter. */
	mp.rd_count++;
	
	return 1;
}

/**
 * mail_test() - produce/consume messages for/from the store peer.
 *
 * Return:	None.
 **/
static void mail_test(void)
{
	int busy_rd, busy_wr;

	/* Produce/consume messages for/from the mail peer.*/
	busy_rd = busy_wr = 1;
	while (busy_rd || busy_wr) {
		/* Read the input from the store peer. */
		busy_rd = mail_read();

		/* Generate the output for the store peer. */
		busy_wr = mail_write();

		/* Wait some microseconds. */
		usleep(1000);
	}
}

/**
 * mail_cleanup() - free the resources of the mail peer.
 *
 * Return:	None.
 **/
static void mail_cleanup(void)
{
	/* Close the mail socket. */
	os_inet_close(mp.inet_id);
}

/**
 * mail_init() - allocate the resources for the test with the mail peer.
 *
 * Return:	None.
 **/
static void mail_init(void)
{
	int rv;
	
	/* Initialize the van OS resources. */
	os_init(1);
	
	/* Enable or disable the OS trace. */
	os_trace_button(0);

	/* Switch on the trace of the mail peer. */
	mp.my_trace = 1;
	
	/* Allocate the socket resources. */
	mp.inet_id = os_inet_open(MY_ADDR, MY_PORT, HIS_ADDR, HIS_PORT);
	
	/* Save the number of the send cycles. */
	mp.limit = LIMIT;

	/* Wait for the connection establishment. */
	for (;;) {
		/* Send the connection request to the store peer and wait for
		 * the response.*/
		rv = os_inet_connect(mp.inet_id);
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
 * main() - start function of the mail peer.
 *
 * Return:	0 or force a software trap.
 **/
int main(void)
{
	printf("%s mail peer\n", P);

	/* Allocate the resources for the internet loop test. */
	mail_init();

	/* Produce/consume messages for/from the store peer. */
	mail_test();

	/* Free the resources for the internet loop test. */
	mail_cleanup();

	/* Release the van OS resources. */
	os_exit();
	
	return(0);
}
