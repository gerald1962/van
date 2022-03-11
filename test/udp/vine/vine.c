// SPDX-License-Identifier: GPL-2.0

/*
 * Test of the internet interfaces for the van system.
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
#define P         "I>"         /* Internet test prompt. */
#define MY_ADDR   "127.0.0.1"  /* IP address of the van display. */
#define MY_PORT   58062        /* Port number of the van display. */
#define HIS_ADDR  "127.0.0.1"  /* IP address of the van controller. */
#define HIS_PORT  62058        /* Port number of the van controller. */
#define PRIO      OS_THREAD_PRIO_FOREG  /* Thread foreground priority. */
#define Q_SIZE     4           /* Size of the thread input queue. */
#define BUF_SIZE  32           /* Size of the I/O buffer. */
#define LIMIT      0           /* Number of the send cycles. */

/*============================================================================
  MACROS
  ============================================================================*/
/* Vine trace with filter. */
#define TRACE(info_)  do { \
    if (vs.my_trace) \
	    printf info_; \
} while (0)

/*============================================================================
  LOCAL TYPE DEFINITIONS
  ============================================================================*/
/**
 * vine_t - state of the local or remote peer.
 *
 * @inet_cid:  inet communication id.
 * @is_disp:   if 1, the peer is the vdisplay application.
 * @name:      name of the test thread.
 * @my_addr:   local IPv4 address.
 * @my_port:   local UDP port.
 * @his_addr:  remote IPv4 address.
 * @his_port:  remote UDP port.
 * @thr_id:    pointer to the device thread.
 * @limit:     limit of the output cycles.
 * @rd_count:  number of the received packets.
 * @wr_count:  number of the sent packets.
 * @buf:       I/O buffer.
 * @rd_done:   if 1, all packets have been received.
 * @wr_done:   if 1, all packets have been sent.
 * @done:      if 1, everything sent and received, resume the main process.
 **/
typedef struct vine_s {
	int    inet_id;
	int    is_disp;
	char   name[OS_THREAD_NAME_LEN];
	char  *my_addr;
	int    my_port;
	char  *his_addr;
	int    his_port;
	void  *thr_id;
	int    clk_id;
	int    limit;
	int    rd_count;
	int    wr_count;
	char   buf[BUF_SIZE];
	int    rd_done;
	int    wr_done;
	int    done;
} vine_t;
	
/*============================================================================
  LOCAL DATA
  ============================================================================*/
/**
 * vs - van inet test state.
 *
 * @my_trace:  if 1, the vine trace is active.
 * @lp:        local peer.
 * @rp:        remote peer.
 * @suspend:   control semaphore of the main process.
 * @mutex:     protect the critical section in the resume operation for the main
 *             process.
 **/
static struct v_s {
	int     my_trace;
	vine_t  lp;
	vine_t  rp;	
	sem_t   suspend;
	pthread_mutex_t  mutex;	
} vs;
	
/*============================================================================
  LOCAL FUNCTION PROTOTYPES
  ============================================================================*/
/*============================================================================
  LOCAL FUNCTIONS
  ============================================================================*/
/**
 * vine_signal() - if the local and remote peer have processed all test data,
 * resume the main process to terminate the test.
 *
 * Return:	None.
 **/
static void vine_signal(int *trigger)
{
	int done;
	
	/* Protect the resume trigger. */
	os_cs_enter(&vs.mutex);

	/* Update the resume trigger. */
	*trigger = 1;

	/* Test the resume operation. */
	done = vs.rp.done && vs.lp.done;

	/* End of the critical section. */
	os_cs_leave(&vs.mutex);

	/* Test the resume result. */
	if (! done)
		return;

	/* Resume the main process. */
	os_sem_release(&vs.suspend);
}

/**
 * vine_wait() - the main process shall be suspend right to the close of the
 * test.
 *
 * Return:	None.
 **/
static void vine_wait(void)
{
	/* Suspend the main process right to the close of the test. */
	os_sem_wait(&vs.suspend);
}

/** 
 * vine_write() - generate output for the remote or local peer.
 *
 * @vp:  pointer to the remote of local peer state.
 *
 * Return:	0, if all data have been sent.
 **/
static int vine_write(vine_t *vp)
{
	int n, rv;
	
	/* Test the final condition of the producer. */
	if (vp->wr_done)
		return 0;

	/* Test the cycle counter. */
	if (vp->wr_count > vp->limit) {
		/* Restart the send operation and get the fill level of the
		 * output buffer. */
		n = os_inet_sync(vp->inet_id);
		if (n > 0)
			return 1;

		/* Update the final condition of the producer. */
		vp->wr_done = 1;
		return 0;
	}
	
	/* Test the write cycle counter. */
	if (vp->wr_count == vp->limit) {
		/* Save the final response. */
		n = snprintf(vp->buf, BUF_SIZE, "DONE");
		OS_TRAP_IF(n >= BUF_SIZE);
	}
	else {
		/* Generate the test message. */
		n = snprintf(vp->buf, OS_BUF_SIZE, "%d", vp->wr_count);
		OS_TRAP_IF(n >= BUF_SIZE);
	}

	/* Include the message delimiter. */
	n++;
	
	/* Start an attempt of transmission. */
	rv = os_inet_write(vp->inet_id, vp->buf, n);
	OS_TRAP_IF(rv != 0 && rv != n);

	/* Test the success of the transmission. */
	if (rv == 0)
		return 1;

	/* Replace the message delimiter with EOS. */
	vp->buf[n -1] = '\0';
	
	/* The transmission has worked. */
	TRACE(("%s sent: n=\"%s\", b=\"%s\", n=%d\n", P, vp->name, vp->buf, n));
	
	/* Increment the cycle counter. */
	vp->wr_count++;
	return 1;
}

/** 
 * vine_read() - read the input from the remote or local peer.
 *
 * @vp:  pointer to the remote of local peer state.
 *
 * Return:	0, if all data have been received.
 **/
static int vine_read(vine_t *vp)
{
	int n;
	
	/* Test the final condition of the consumer. */
	if (vp->rd_done)
		return 0;

	/* Wait for data from the producer. */
	n = os_inet_read(vp->inet_id, vp->buf, BUF_SIZE);
	if (n < 1)
		return 1;
		
	TRACE(("%s rcvd: n=\"%s\", b=\"%s\", n=%d\n", P, vp->name, vp->buf, n));
		       
	/* Test the end condition of the test. */
	if (os_strcmp(vp->buf, "DONE") == 0) {
		vp->rd_done = 1;
		return 0;
	}

	/* Convert and test the received counter. */
	n = strtol(vp->buf, NULL, 10);
	OS_TRAP_IF(vp->rd_count != n);

	/* Increment the receive counter. */
	vp->rd_count++;
	
	return 1;
}

/** 
 * vine_connect() - establish the connection between vcontroller and vdisplay.
 *
 * @vp:  pointer to the remote of local peer state.
 *
 * Return:	None.
 **/
static void vine_connect(vine_t *vp)
{
	int rv;
	
	/* Wait for the connection establishment. */
	for (;;) {
		/* Test the application type. */
		if (vp->is_disp) {
			/* Send the connection establishment message to
			 * vcontroller.*/
			rv = os_inet_connect(vp->inet_id);
			if (rv == 0)
				return;
		}
		else {
			/* Wait for the vdisplay peer.*/
			rv = os_inet_accept(vp->inet_id);
			if (rv == 0)
				return;
		}
		
		/* Wait some microseconds. */
		usleep(1000);
	}
}
	
/**
 * vine_test_exec() -  exchange test messages between the local and remote peer.
 *
 * @msg:  generic input message.
 *
 * Return:	None.
 **/
static void vine_test_exec(os_queue_elem_t *g_msg)
{
	vine_t *vp;
	int busy_rd, busy_wr;
	
	/* Decode the pointer to the state state. */
	vp = g_msg->param;

	/* Establish the connection between vcontroller and vdisplay. */
	vine_connect(vp);
	
	/* Exchange test messages between the local and remote peer. */
	busy_rd = busy_wr = 1;
	while (busy_rd || busy_wr) {
		/* Read the input from the remote or local peer. */
		busy_rd = vine_read(vp);

		/* Generate the output for the remote or local peer. */
		busy_wr = vine_write(vp);

		/* Wait some microseconds. */
		usleep(1000);
	}

	/* Resume the main process. */
	vine_signal(&vp->done);
}

/**
 * vine_free() -  Free the resources of the remote or local peer.
 *
 * @vp:  pointer to the test state of a peer.
 *
 * Return:	None.
 **/
static void vine_free(vine_t *vp)
{
	/* Close the remote or local socket. */
	os_inet_close(vp->inet_id);
	
	/* Kill the remote or local test peer thread. */
	os_thread_destroy(vp->thr_id);
}

/**
 * vine_cleanup() - free the resources for the internet loop test.
 *
 * Return:	None.
 **/
static void vine_cleanup(void)
{
	/* Free the resources of the remote peer. */
	vine_free(&vs.rp);
	
	/* Free the resources of the remote peer. */
	vine_free(&vs.lp);
	
	/* Free the test control semaphare. */
	os_sem_delete(&vs.suspend);
	
	/* Free the mutex for the critical section in vine signal. */
	os_cs_destroy(&vs.mutex);
}

/**
 * vine_alloc() - allocate the resources for the remote or local peer state.
 *
 * @vp:     pointer to the test state of a peer.
 * @a1:     pointer to the IPv4 address string.
 * @p1:     port number.
 * @a2:     pointer to the IPv4 address string.
 * @p2:     port number.
 * @n:      name of the test thread.
 * @limit:  number of send cycles.
 *
 * Return:	None.
 **/
static void vine_alloc(vine_t *vp, char *a1, int p1, char *a2, int p2,
		       const char *n, int limit)
{
	os_queue_elem_t msg;

	/* Save the IP addresses and ports. */
	vp->my_addr  = a1;
	vp->my_port  = p1;
	vp->his_addr = a2;
	vp->his_port = p2;

	/* Allocate the socket resources. */
	vp->inet_id = os_inet_open(vp->my_addr, vp->my_port,
				   vp->his_addr, vp->his_port);

	/* Copy the name of the test thread. */
	os_strcpy(vp->name, OS_THREAD_NAME_LEN, n);
	
	/* Install the remote or local peer test thread. */
	vp->thr_id = os_thread_create(n, PRIO, Q_SIZE);

	/* Save the number of the send cycles. */
	vp->limit = limit;
	
	/* Start the test thread. */
	os_memset(&msg, 0, sizeof(msg));
	msg.param = vp;
	msg.cb    = vine_test_exec;
	OS_SEND(vp->thr_id, &msg, sizeof(msg));
}

/**
 * vine_init() - allocate the resources for the internet loop test.
 *
 * Return:	None.
 **/
static void vine_init(void)
{
	/* Initialize the van OS resources. */
	os_init(1);
	
	/* Enable or disable the OS trace. */
	os_trace_button(0);

	/* Switch on the vine trace. */
	vs.my_trace = 1;
	
	/* Allocate the mutex for the critical section in vine signal. */
	os_cs_init(&vs.mutex);

	/* Allocate the test control semaphare. */
	os_sem_init(&vs.suspend, 0);

	/* Allocate the resources for the remote peer. */
	vine_alloc(&vs.rp, HIS_ADDR, HIS_PORT, MY_ADDR, MY_PORT,
		   "vine_rp", LIMIT);
	vs.rp.is_disp = 0;
	
	/* Allocate the resources for the local peer. */
	vine_alloc(&vs.lp, MY_ADDR, MY_PORT, HIS_ADDR, HIS_PORT,
		   "vine_lp", LIMIT);
	vs.lp.is_disp = 1;
}

/*============================================================================
  EXPORTED FUNCTIONS
  ============================================================================*/
/**
 * main() - start function of the van internet test.
 *
 * Return:	0 or force a software trap.
 **/
int main(void)
{
	printf("%s van internet test\n", P);

	/* Allocate the resources for the internet loop test. */
	vine_init();

	/* Wait for the end of the inet test loop. */
	vine_wait();

	/* Free the resources for the internet loop test. */
	vine_cleanup();

	/* Release the van OS resources. */
	os_exit();
	
	return(0);
}
