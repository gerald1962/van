// SPDX-License-Identifier: GPL-2.0

/*
 * Test of the box peer.
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
#define P         "B>"              /* Prompt of the box peer. */
#define MY_ADDR   "10.0.2.15"       /* IP address of the box peer. */
#define MY_PORT   58062             /* Port number of the box peer. */
#define HIS_ADDR  "192.168.178.96"  /* IP address of the room peer. */
#define HIS_PORT  62058             /* Port number of the room peer. */
#define BUF_SIZE  32                /* Size of the I/O buffer. */
#define WR_FREQ   2                 /* Relation between read and write. */
#define WR_LIMIT  999999            /* Number of the send cycles. */

/*============================================================================
  MACROS
  ============================================================================*/
/* Vine trace with filter. */
#define TRACE(info_)  do { \
    if (bp.my_trace) \
	    printf info_; \
} while (0)

/*============================================================================
  LOCAL TYPE DEFINITIONS
  ============================================================================*/
/*============================================================================
  LOCAL DATA
  ============================================================================*/
/**
 * bp - state of the box peer.
 *
 * @my_trace:  if 1, the trace is active.
 * @inet_id:   inet id.
 * @wr_limit:  number of the generator cycles.
 * @wr_edge:   actual rising coordinate of the write edge graph.
 * @wr_freq:   the relationship between write and read opertions.
 * @rd_count:  current read cycles.
 * @wr_count:  current write cycles.
 * @buf:       I/O buffer.
 * @rd_done:   if 1, all input messages have been consumed.
 * @wr_done:   if 1, all output messages have generated.
 **/
static struct mp_s {
	int   my_trace;
	int   inet_id;
	int   wr_limit;
	int   wr_edge;
	int   wr_freq;
	int   rd_count;
	int   wr_count;
	char  buf[BUF_SIZE];
	int   rd_done;
	int   wr_done;
	int   done;
} bp;
	
/*============================================================================
  LOCAL FUNCTION PROTOTYPES
  ============================================================================*/
/*============================================================================
  LOCAL FUNCTIONS
  ============================================================================*/
/** 
 * box_write() - generate the output for the room peer.
 *
 * Return:	0, if all data have been sent.
 **/
static int box_write(void)
{
	int n, rv;
	
	/* Test the final condition of the producer. */
	if (bp.wr_done)
		return 0;

	/* Test the cycle counter. */
	if (bp.wr_count > bp.wr_limit) {
		/* Restart the send operation and get the fill level of the
		 * output buffer. */
		n = os_inet_sync(bp.inet_id);
		if (n > 0)
			return 1;

		/* Update the final condition of the producer. */
		bp.wr_done = 1;
		return 0;
	}

	/* Test the send frequency. */
	if (bp.wr_edge < bp.wr_freq) {
		/* Increase the coordinate of the send edge graph. */
		bp.wr_edge++;
		return 1;
	}

	/* Jump back to the origin of the send edge graph. */
	bp.wr_edge = 0;
	
	/* Test the write cycle counter. */
	if (bp.wr_count == bp.wr_limit) {
		/* Save the final response. */
		n = snprintf(bp.buf, BUF_SIZE, "DONE");
		OS_TRAP_IF(n >= BUF_SIZE);
	}
	else {
		/* Generate the test message. */
		n = snprintf(bp.buf, OS_BUF_SIZE, "%d", bp.wr_count);
		OS_TRAP_IF(n >= BUF_SIZE);
	}

	/* Include the message delimiter. */
	n++;
	
	/* Start an attempt of transmission. */
	rv = os_inet_write(bp.inet_id, bp.buf, n);
	OS_TRAP_IF(rv != 0 && rv != n);

	/* Test the success of the transmission. */
	if (rv == 0)
		return 1;

	/* Replace the message delimiter with EOS. */
	bp.buf[n -1] = '\0';
	
	/* The transmission has worked. */
	TRACE(("%s sent: b=\"%s\", n=%d\n", P, bp.buf, n));
	
	/* Increment the cycle counter. */
	bp.wr_count++;
	return 1;
}

/** 
 * box_read() - read the input from the room peer.
 *
 * Return:	0, if all data have been received.
 **/
static int box_read(void)
{
	int n;
	
	/* Test the final condition of the consumer. */
	if (bp.rd_done)
		return 0;

	/* Wait for the data from the producer. */
	n = os_inet_read(bp.inet_id, bp.buf, BUF_SIZE);
	if (n < 1)
		return 1;
		
	TRACE(("%s rcvd: b=\"%s\", n=%d\n", P, bp.buf, n));
		       
	/* Test the end condition of the test. */
	if (os_strcmp(bp.buf, "DONE") == 0) {
		bp.rd_done = 1;
		return 0;
	}

	/* Convert and test the received counter. */
	n = strtol(bp.buf, NULL, 10);
	OS_TRAP_IF(bp.rd_count != n);

	/* Increment the receive counter. */
	bp.rd_count++;
	
	return 1;
}

/**
 * box_test() - produce/consume messages for/from the room peer.
 *
 * Return:	None.
 **/
static void box_test(void)
{
	int busy_rd, busy_wr;

	/* Produce/consume messages for/from the box peer.*/
	busy_rd = busy_wr = 1;
	while (busy_rd || busy_wr) {
		/* Read the input from the room peer. */
		busy_rd = box_read();

		/* Generate the output for the room peer. */
		busy_wr = box_write();

		/* Wait some microseconds. */
		usleep(1);
	}
}

/**
 * box_cleanup() - free the resources of the box peer.
 *
 * Return:	None.
 **/
static void box_cleanup(void)
{
	/* Close the box socket. */
	os_inet_close(bp.inet_id);
}

/**
 * box_init() - allocate the resources for the test with the box peer.
 *
 * Return:	None.
 **/
static void box_init(void)
{
	int rv;
	
	/* Initialize the van OS resources. */
	os_init(1);
	
	/* Enable or disable the OS trace. */
	os_trace_button(0);

	/* Switch on the trace of the box peer. */
	bp.my_trace = 1;
	
	/* Allocate the socket resources. */
	bp.inet_id = os_inet_open(MY_ADDR, MY_PORT, HIS_ADDR, HIS_PORT);
	
	/* Save the number of the send cycles. */
	bp.wr_limit = WR_LIMIT;

	/* Save the write frequency. */
	bp.wr_freq = WR_FREQ;
	
	/* Wait for the connection establishment. */
	for (;;) {
		/* Send the connection request to the room peer and wait for
		 * the response. */
		rv = os_inet_connect(bp.inet_id);
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
 * main() - start function of the box peer.
 *
 * Return:	0 or force a software trap.
 **/
int main(void)
{
	printf("%s box peer\n", P);

	/* Allocate the resources for the internet loop test. */
	box_init();

	/* Produce/consume messages for/from the room peer. */
	box_test();

	/* Free the resources for the internet loop test. */
	box_cleanup();

	/* Release the van OS resources. */
	os_exit();
	
	return(0);
}
