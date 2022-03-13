// SPDX-License-Identifier: GPL-2.0

/*
 * Test of the room peer.
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
#define P         "R>"              /* Prompt of the room peer. */
#define MY_ADDR   "192.168.178.96"  /* IP address of the room peer. */
#define MY_PORT   62058             /* Port number of the room peer. */
#define HIS_ADDR  "192.168.178.1"   /* IP address of the box peer. */
#define HIS_PORT  58062             /* Port number of the box peer. */
#define BUF_SIZE  32                /* Size of the I/O buffer. */
#define LIMIT      9                /* Number of the send cycles. */

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
 * rp - state of the room peer.
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
} rp;
	
/*============================================================================
  LOCAL FUNCTION PROTOTYPES
  ============================================================================*/
/*============================================================================
  LOCAL FUNCTIONS
  ============================================================================*/
/** 
 * room_write() - generate the output for the box peer.
 *
 * Return:	0, if all data have been sent.
 **/
static int room_write(void)
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
 * room_read() - read the input from the box peer.
 *
 * Return:	0, if all data have been received.
 **/
static int room_read(void)
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
 * room_test() - produce/consume messages for/from the box peer.
 *
 * Return:	None.
 **/
static void room_test(void)
{
	int busy_rd, busy_wr;
	
	/* Produce/consume messages for/from the box peer.*/
	busy_rd = busy_wr = 1;
	while (busy_rd || busy_wr) {
		/* Read the input from the box peer. */
		busy_rd = room_read();

		/* Generate the output for the box peer. */
		busy_wr = room_write();

		/* Wait some microseconds. */
		usleep(1000);
	}
}

/**
 * room_cleanup() - free the resources of the room peer.
 *
 * Return:	None.
 **/
static void room_cleanup(void)
{
	/* Close the room socket. */
	os_inet_close(rp.inet_id);
}

/**
 * room_init() - allocate the resources for the test with the room peer.
 *
 * Return:	None.
 **/
static void room_init(void)
{
	int rv;
	
	/* Initialize the van OS resources. */
	os_init(1);
	
	/* Enable or disable the OS trace. */
	os_trace_button(0);

	/* Switch on the trace of the room peer. */
	rp.my_trace = 1;
	
	/* Allocate the socket resources. */
	
	rp.inet_id = os_inet_open(MY_ADDR, MY_PORT, HIS_ADDR, HIS_PORT);
	
	/* Save the number of the send cycles. */
	rp.limit = LIMIT;
	
	/* Wait for the connection establishment. */
	for (;;) {
		/* Wait for the connection request from the box peer and send
		 * the response. */
		rv = os_inet_accept(rp.inet_id);
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
 * main() - start function of the room peer.
 *
 * Return:	0 or force a software trap.
 **/
int main(void)
{
	printf("%s room peer\n", P);

	/* Allocate the resources for the internet loop test. */
	room_init();

	/* Produce/consume messages for/from the box peer. */
	room_test();

	/* Free the resources for the internet loop test. */
	room_cleanup();

	/* Release the van OS resources. */
	os_exit();
	
	return(0);
}
