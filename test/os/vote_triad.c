// SPDX-License-Identifier: GPL-2.0

/*
 * triad - test the controller-battery-display cabling.
 *
 * Triyright (C) 2022 Gerald Schueller <gerald.schueller@web.de>
 */

/*============================================================================
  IMPORTED INCLUDE REFERENCES
  ============================================================================*/
#include "vote.h"    /* Van OS test environment. */

/*============================================================================
  EXPORTED INCLUDE REFERENCES
  ============================================================================*/
/*============================================================================
  LOCAL NAME CONSTANTS DEFINITIONS
  ============================================================================*/
#define PRIO    OS_THREAD_PRIO_FOREG  /* Thread foreground priority. */
#define Q_SIZE  4                     /* Size of the thread input queue. */

/* Default value for the clock interval in milliseconds. */
#define CLOCK_INTERVAL  1

/*============================================================================
  MACROS
  ============================================================================*/
/* tri trace with filter. */
#define TRACE(info_)  do { \
		if (tri_data.my_trace) \
			printf info_; \
} while (0)

/*============================================================================
  LOCAL TYPE DEFINITIONS
  ============================================================================*/
/**
 * tri_ep_t - state of a cable endpoint
 *
 * @name:      name of the cable endpoint.
 * @tread:     pointer to the device thread.
 * @dev_id:    device id.
 * @t_id:      timer id for the clock.
 * @interval:  repeating clock interval in milliseconds.
 * @use_wait:  if 1, execute the test with os_c_wait().
 * @wait_id:   wait conditions of the cable endpoint.
 * @cycles:    number of the output cycles.
 * @wr_count:  current cycle value.
 * @rd_count:  number of the received packets.
 * @wr_done:   if 1, everything sent, resume the main process.
 * @rd_done:   if 1, everything received, resume the main process.
 **/
typedef struct {
	char  *name;
	void  *thread;
	int    dev_id;
	int    t_id;
	int    interval;
	int    use_wait;
	int    wait_id;
	int    cycles;
	int    wr_count;
	int    rd_count;
	atomic_int  wr_done;
	atomic_int  rd_done;
} tri_ep_t;

/*============================================================================
  LOCAL FUNCTION PROTOTYPES
  ============================================================================*/
static int tri_start(void);
static int tri_stop(void);

/*============================================================================
  LOCAL DATA
  ============================================================================*/
/* Define vot_p for TEST_ASSERT_EQ. */
static test_stat_t *vote_p;

/**
 * tri_data - state of the control technology platform. 
 *
 * @my_trace:     if 1, activate the tri trace.
 * @clock_trace:  if 1, activate the clock trace.
 * @suspend:      suspend the main process while the test is running.
 *
 * @c_thr:        pointer to the controller thread.
 * @c_wait_id:    controller wait conditions for the cables.
 *
 * @c_batt:       state of the cable controller endpoint towards battery.
 * @c_disp:       state of the cable controller endpoint towards display.
 *
 * @n_batt:       state of the cable battery endpoint towards controller.
 * @n_disp:       state of the cable display endpoint towards controller.
 **/
static struct tri_data_s {
	int    my_trace;
	int    clock_trace;
	sem_t  suspend;
	
	void  *c_thr;
	int    c_wait_id;
	
	tri_ep_t  c_batt;
	tri_ep_t  c_disp;

	tri_ep_t  n_batt;
	tri_ep_t  n_disp;
} tri_data;

/* List of the of the cable interworking test cases. */
static test_elem_t tri_system[] = {
	{ TEST_ADD(tri_start), 0 },
	{ TEST_ADD(tri_stop), 0 },
	{ NULL, NULL, 0 }
};

/*============================================================================
  LOCAL FUNCTIONS
  ============================================================================*/
/**
 * tri_resume() - resume the main process.
 *
 * @done: pointer to the final trigger of a participating data transfer thread.
 *
 * Return:	None.
 **/
static void tri_resume(atomic_int *done)
{
	/* Resume the main process. */
	atomic_store(done, 1);
	os_sem_release(&tri_data.suspend);
}

/**
 * tri_d_read() -  the display expects data with the non blocking
 * synchronous buffered read operation from the controller.
 *
 * @ep:  pointer to the display state.
 *
 * Return:	0, if the write operation is complete.
 **/
static int tri_d_read(tri_ep_t *ep)
{
	char buf[OS_BUF_SIZE];
	int done, n;
	
	/* Test the final condition of the consumer. */
	done = atomic_load(&ep->rd_done);
	if (done)
		return 0;

	/* Wait for data from the producer. */
	n = os_bread(ep->dev_id, buf, OS_BUF_SIZE);
	if (n < 1)
		return 1;
		
	TRACE(("%s rcvd: [e:%s, b:\"%s\", s:%d]\n", P, ep->name, buf, n));
		       
	/* Test the end condition of the test. */
	if (os_strcmp(buf, "DONE") == 0) {
		tri_resume(&ep->rd_done);
		return 0;
	}

	/* Convert and test the received counter. */
	n = strtol(buf, NULL, 10);
	OS_TRAP_IF(ep->rd_count != n);

	/* Increment the receive counter. */
	ep->rd_count++;
	
	return 1;
}

/**
 * tri_d_write() -  the display sends data with the non blocking synchronous
 * buffered write operation to the controller.
 *
 * @ep:  pointer to the display state.
 *
 * Return:	0, if the write operation is complete, otherwise 1.
 **/
static int tri_d_write(tri_ep_t *ep)
{
	char buf[OS_BUF_SIZE];
	int done, n, rv;
	
	/* Test the final condition of the producer. */
	done = atomic_load(&ep->wr_done);
	if (done)
		return 0;

	/* Test the cycle counter. */
	if (ep->wr_count > ep->cycles) {
		/* Get the fill level of the output buffer. */
		n = os_bsync(ep->dev_id);
		if (n > 0)
			return 1;

		/* Resume the main process. */
		tri_resume(&ep->wr_done);
		return 0;
	}
	
	/* Test the write cycle counter. */
	if (ep->wr_count == ep->cycles) {
		/* Create the final response. */
		n = snprintf(buf, OS_BUF_SIZE, "DONE");
	}
	else {
		/* Create the digit sequence. */
		n = snprintf(buf, OS_BUF_SIZE, "%d", ep->wr_count);
	}

	/* Include the message delimiter. */
	n++;
	
	/* Start an attempt of transmission. */
	rv = os_bwrite(ep->dev_id, buf, n);
	OS_TRAP_IF(rv != 0 && rv != n);

	/* Test the success of the transmission. */
	if (rv == 0)
		return 1;

	/* XXX Replace the message delimiter with EOS. */
	buf[n -1] = '\0';
	
	/* The transmission has worked. */
	TRACE(("%s sent: [e:%s, b:\"%s\", s:%d]\n", P, ep->name, buf, n));
	
	/* Increment the cycle counter. */
	ep->wr_count++;
	return 1;
}

/**
 * tri_disp_exec() - the display neighbour of the controller sends and receives
 * data with the non blocking synchronous buffered operations.
 *
 * @msg:  pointer to the generic input message.
 *
 * Return:	None.
 **/
static void tri_disp_exec(os_queue_elem_t *msg)
{
	tri_ep_t *ep;
	int busy_rd, busy_wr;

	/* Initialize the return values. */
	busy_rd = 1;
	busy_wr = 1;
	
	/* Get the pointer to the endpoint state. */
	ep = &tri_data.n_disp;

	/* The display thread analyzes or generates data from or to the
	 * controller with the non blocking syncronous buffered triy read or
	 * write  operation. */
	while (busy_rd || busy_wr) {
		/* Read the input from the controller. */
		busy_rd = tri_d_read(ep);
		
		/* Generate output for the controller. */
		busy_wr = tri_d_write(ep);

		/* Simulate the display event loop. */
		os_clock_msleep(ep->interval);		
	}
	
	TRACE(("%s nops: [e:%s, s:done]\n", P, ep->name));
}

/**
 * tri_read() - generic read operation for all endpoints of any cable: any
 * controller or a neighbour endpoint expects data with the non blocking
 * synchronous read operation from the other endpoint.
 *
 * @ep:         pointer to the endpoint state.
 * @wait_cond:  if we return 1, repeat the read operation.
 *
 * Return:	0, if the write operation is complete.
 **/
static int tri_read(tri_ep_t *ep, int *wait_cond)
{
	char buf[OS_BUF_SIZE];
	int done, n;
	
	/* Initialize the return value. */
	if (wait_cond != NULL)
		*wait_cond = 0;

	/* Test the final condition of the consumer. */
	done = atomic_load(&ep->rd_done);
	if (done)
		return 0;
	
	/* Wait for data from the producer. */
	n = os_c_read(ep->dev_id, buf, OS_BUF_SIZE);
	if (n < 1) {
		if (wait_cond != NULL)
			*wait_cond = 1;
		return 1;
	}
		
	TRACE(("%s rcvd: [e:%s, b:\"%s\", s:%d]\n", P, ep->name, buf, n));
		       
	/* Test the end condition of the test. */
	if (os_strcmp(buf, "DONE") == 0) {
		tri_resume(&ep->rd_done);
		return 0;
	}
	
	/* XXX Terminate the message with EOS. */
	buf[n] = '\0';

	/* Convert and test the received counter. */
	n = strtol(buf, NULL, 10);
	OS_TRAP_IF(ep->rd_count != n);

	/* Increment the receive counter. */
	ep->rd_count++;
	
	return 1;
}

/**
 * tri_write() - generic write operation for all endpoints of any cable: the
 * controller or a neighbour sends data with the non blocking synchronous write
 * operation to the other endpoint.
 *
 * @ep:         pointer to the endpoint state.
 * @wait_cond:  if we return 1, repeat the write operation.
 *
 * Return:	0, if the write operation is complete.
 **/
static int tri_write(tri_ep_t *ep, int *wait_cond)
{
	char buf[OS_BUF_SIZE];
	int done, n, rv;
	
	/* Initialize the return value. */
	if (wait_cond != NULL)
		*wait_cond = 0;
	
	/* Test the final condition of the producer. */
	done = atomic_load(&ep->wr_done);
	if (done)
		return 0;

	/* Test the cycle counter. */
	if (ep->wr_count > ep->cycles) {
		/* Resume the main process. */
		tri_resume(&ep->wr_done);
		return 0;
	}
	
	/* Test the write cycle counter. */
	if (ep->wr_count == ep->cycles) {
		/* Create the final response. */
		n = snprintf(buf, OS_BUF_SIZE, "DONE");
	}
	else {
		/* Create the digit sequence. */
		n = snprintf(buf, OS_BUF_SIZE, "%d", ep->wr_count);
	}

	/* Include end of string. */
	n++;

	/* Start an attempt of transmission. */
	rv = os_c_write(ep->dev_id, buf, n);
	OS_TRAP_IF(rv != 0 && rv != n);

	/* Test the success of the transmission. */
	if (rv == 0) {
		if (wait_cond != NULL)
			*wait_cond = 1;
		return 1;
	}

	/* The transmission has worked. */
	TRACE(("%s sent: [e:%s, b:\"%s\", s:%d]\n", P, ep->name, buf, n));
	
	/* Increment the cycle counter. */
	ep->wr_count++;
	return 1;
}

/**
 * tri_batt_exec() - generic neighbour transition: the neighbour of the
 * controller sends and receives data with the non blocking synchronous
 * operations.
 *
 * @msg:  pointer to the generic input message.
 *
 * Return:	None.
 **/
static void tri_batt_exec(os_queue_elem_t *msg)
{
	tri_ep_t *ep;
	int busy_rd, busy_wr, no_c_inp, no_c_out;

	/* Get the pointer to the endpoint state. */
	ep = msg->param;

	/* Initialize the return values. */
	busy_rd = 1;
	busy_wr = 1;
	
	/* Initialize the wait condition for cable endpoint operations. */
	no_c_inp = 0;
	no_c_out = 0;

	/* The neighbour thread analyzes or generates data from or to the
	 * controller with the non blocking syncronous triy read or write
	 * operation. */
	while (busy_rd || busy_wr) {
		/* Read the input from the controller. */
		busy_rd = tri_read(ep, &no_c_inp);

		/* Generate output for the controller. */
		busy_wr = tri_write(ep, &no_c_out);
		
		/* Avoid an endles loop, if no I/O data are available. */
		if (ep->use_wait && no_c_inp && no_c_out)
			os_c_wait(ep->wait_id);
	}
	
	TRACE(("%s nops: [e:%s, s:done]\n", P, ep->name));
}

/**
 * tri_clock_down() - power down the controller clock.
 *
 * @ep:     pointer to the endpoint state.
 * @trace:  if 1, print the clock trace information.
 *
 * Return:	None.
 **/
static void tri_clock_down(tri_ep_t *ep, int trace)
{
	/* Stop the periodic timer. */
	os_clock_stop(ep->t_id);
	
	/* Print the test cycle information. */
	if (trace)
		os_clock_trace(ep->t_id, OS_CT_LAST);
		
	/* Destroy the  interval timer. */
	os_clock_delete(ep->t_id);
}

/**
 * tri_clock_in_time() - wait for the controller clock tick.
 *
 * @ep:     pointer to the endpoint state.
 * @trace:  if 1, print the clock trace information.
 *
 * Return:	None.
 **/
static void tri_clock_in_time(tri_ep_t *ep, int trace)
{
	long exec_time;
	
	/* Simulate the current execute time. */
	exec_time = random() % ep->interval - 1;
	if (exec_time < 1)
		exec_time = 1;

	/* Sleep n milliseconds. */
	os_clock_msleep(exec_time);

	/* Wait for the timer expiration. */
	os_clock_barrier(ep->t_id);
		
	/* Print the test cycle information. */
	if (trace)
		os_clock_trace(ep->t_id, OS_CT_MIDDLE);
}

/**
 * tri_clock_up() - power up the controller clock.
 *
 * @ep:     pointer to the endpoint state.
 * @trace:  if 1, print the clock trace information.
 *
 * Return:	None.
 **/
static void tri_clock_up(tri_ep_t *ep, int trace)
{
	/* Create the interval timer. */
	ep->t_id = os_clock_init("ctrl", ep->interval);

	/* Start the periodic timer. */
	os_clock_start(ep->t_id);
	
	/* Print the first clock trace. */
	if (trace)
		os_clock_trace(ep->t_id, OS_CT_FIRST);

	/* Take the second value of the current time as random start value for the
	 * random number generator. */
	srand(time(NULL));
}

/**
 * tri_ctrl_exec() - the controller sends and receives data with the non blocking
 * synchronous operations either to the battery or to the display.
 *
 * @msg:  pointer to the generic input message.
 *
 * Return:	None.
 **/
static void tri_ctrl_exec(os_queue_elem_t *msg)
{
	struct tri_data_s *c;
	tri_ep_t *batt, *disp;
	int busy, rv;
		
	/* Get the pointer to the tri state. */
	c = &tri_data;

	/* Get the pointer to the endpoint states. */
	batt = &c->c_batt;
	disp = &c->c_disp;

	/* Power up the controller clock. */
	tri_clock_up(batt, c->clock_trace);
	
	/* Initialize the I/O loop condition. */
	busy = 1;
	
	/* The control thread analyzes or generates data from or to the
	 * neighbour endpoints with the non blocking syncronous triy read or
	 * write operation. */
	for (busy = 1; busy;) {
		busy = 0;

		/* Read the input from the display. */
		rv = tri_read(disp, NULL);
		busy = busy || rv;

		/* Read the input from the battery. */
		rv = tri_read(batt, NULL);
		busy = busy || rv;
		
		/* Generate output for the battery. */
		rv = tri_write(batt, NULL);
		busy = busy || rv;
		
		/* Generate output for the display. */
		rv = tri_write(disp, NULL);
		busy = busy || rv;
		
		/* Wait for the controller clock tick. */
		tri_clock_in_time(batt, c->clock_trace);				
	}
	
	/* Power down the controller clock. */
	tri_clock_down(batt, c->clock_trace);
	
	TRACE(("%s nops: [e:%s/%s, s:done]\n", P, batt->name, disp->name));
}

/**
 * tri_wait() - the main process is waiting for the end of the data transfer.
 *
 * Return:	None.
 **/
static void tri_wait(void)
{
	tri_ep_t *c_batt, *c_disp, *n_batt, *n_disp;
	int done, wr_done, rd_done;
	
	/* Get the pointer to all endpoint states. */
	c_batt = &tri_data.c_batt;
	c_disp = &tri_data.c_disp;
	n_batt = &tri_data.n_batt;
	n_disp = &tri_data.n_disp;

	/* Initialize the control status. */
	done = 0;
	
	/* Test the status of the data transfer threads. */
	while(! done) {
		/* Suspend the main process. */
		os_sem_wait(&tri_data.suspend);
		
		/* Analyze the status of all data transfer threads. */
		done = 1;

		/* Controller thread. */
		wr_done = atomic_load(&c_batt->wr_done);
		rd_done = atomic_load(&c_batt->rd_done);
		done = done && wr_done && rd_done;
			
		wr_done = atomic_load(&c_disp->wr_done);
		rd_done = atomic_load(&c_disp->rd_done);
		done = done && wr_done && rd_done;
						
		/* Battery thread. */
		wr_done = atomic_load(&n_batt->wr_done);
		rd_done = atomic_load(&n_batt->rd_done);
		done = done && wr_done && rd_done;
			
		/* Display thread. */
		wr_done = atomic_load(&n_disp->wr_done);
		rd_done = atomic_load(&n_disp->rd_done);
		done = done && wr_done && rd_done;
	}
}

/**
 * tri_ep_cleanup() - release the resources of a cable endpooint.
 *
 * @ep:  pointer to the endpoint state.
 *
 * Return:	None.
 **/
static void tri_ep_cleanup(tri_ep_t *ep)
{
	/* Remove the test thread. */
	if (ep->thread != NULL)
		os_thread_destroy(ep->thread);

	/* Delete the endpoint device. */
	os_c_close(ep->dev_id);
	
	/* Release the wait ressources. */
	if (ep->wait_id != -1)
		os_c_wait_release(ep->wait_id);
}

/**
 * tri_ctrl_cleanup() - release the resources of the cable controller.
 *
 * @c: pointer to the tri state.
 *
 * Return:	None.
 **/
static void tri_ctrl_cleanup(struct tri_data_s *c)
{
	/* Remove the controller threads */
	os_thread_destroy(c->c_thr);

	/* Release the controller resources. */
	tri_ep_cleanup(&c->c_batt);
	tri_ep_cleanup(&c->c_disp);
}

/**
 * tri_disp_cleanup() - release the resources of the display endpooint.
 *
 * @ep:  pointer to the endpoint state.
 *
 * Return:	None.
 **/
static void tri_disp_cleanup(tri_ep_t *ep)
{
	/* Remove the display testt thread. */
	os_thread_destroy(ep->thread);

	/* Delete the display endpoint device. */
	os_bclose(ep->dev_id);
}

/**
 * tri_stop() - release the platform for the control technology.
 *
 * Return:	None.
 **/
static int tri_stop(void)
{
	os_statistics_t expected = { 6, 4, 0, 2346, 2346, 0 };
	struct tri_data_s *c;
	int stat;
	
	/* Get the pointer to the tri state. */
	c = &tri_data;
	
	/* Battery. */
	tri_ep_cleanup(&c->n_batt);

	/* Display. */
	tri_disp_cleanup(&c->n_disp);

	/* Controller. */
	tri_ctrl_cleanup(c);
	
	/* Release the control semaphore for the main process. */
	os_sem_delete(&c->suspend);

	/* Verify the OS state. */
	stat = test_os_stat(&expected);

	return stat;
}

/**
 * tri_ep_init() - initialize a cable endpoint.
 *
 * @ep:      pointer to the endpoint state.
 * @ep_n:    trace name of the cable endpoint.
 * @dev_n:   device name of the cable endpoint.
 * @wait:    if 1, initialize the wait condition.
 * @cycles:  number of the output cycles.
 *
 * Return:	None.
 **/
static void tri_ep_init(tri_ep_t *ep, char *ep_n, char *thr_n, char *dev_n,
			int wait)
{
	int wait_list[1];

	/* Save the trace name of the cable endpoint. */
	ep->name = ep_n;

	/* Install the test thread, */
	if (thr_n != NULL)
		ep->thread = os_thread_create(thr_n, PRIO, Q_SIZE);
	else
		ep->thread = NULL;
	
	/* Create the endpoint device of a cable. */
	ep->dev_id = os_c_open(dev_n, O_NBLOCK);
	
	/* Default value for the clock intervall. */
	if (ep->interval < 1)
		ep->interval = CLOCK_INTERVAL;

	/* Initialize the wait condition. */
	if (wait) {
		wait_list[0] = ep->dev_id;
		ep->wait_id = os_c_wait_init(wait_list, 1);
	}
	else {
		/* Ignore the wait condition. */
		ep->wait_id = -1;
	}
}

/**
 * tri_display_init() - install the display resources.
 *
 * @c: pointer to the tri state.
 *
 * Return:	None.
 **/
static void tri_display_init(struct tri_data_s *c)
{
	os_queue_elem_t msg;
	tri_ep_t *disp;

	/* Get the pointer to the battery state. */
	disp = &c->n_disp;

	/* Save the trace name of the cable endpoint. */
	disp->name = "n_disp";

	/* Install the test thread, */
	disp->thread = os_thread_create("display", PRIO, Q_SIZE);
	
	/* Open the display entry point with I/O buffering of the cable between
	 * controller and display. */
	disp->dev_id = os_bopen("/van/display", 0);
	
	/* Default value for the clock intervall. */
	if (disp->interval < 1)
		disp->interval = CLOCK_INTERVAL;
	
	/* Ignore the wait condition. */
	disp->wait_id = -1;
	
	/* Start the display. */
	os_memset(&msg, 0, sizeof(msg));
	msg.param = disp;
	msg.cb    = tri_disp_exec;
	OS_SEND(c->n_disp.thread, &msg, sizeof(msg));	
}

/**
 * tri_battery_init() - install the battery resources.
 *
 * @c: pointer to the tri state.
 *
 * Return:	None.
 **/
static void tri_battery_init(struct tri_data_s *c)
{
	os_queue_elem_t msg;
	tri_ep_t *batt;
	
	/* Get the pointer to the battery state. */
	batt = &c->n_batt;
	
	/* Initialize the battery endpoint. */
	tri_ep_init(batt, "n_batt", "battery", "/van/battery", 1);

	/* Start the battery. */
	os_memset(&msg, 0, sizeof(msg));	
	msg.param = batt;
	msg.cb    = tri_batt_exec;
	OS_SEND(c->n_batt.thread, &msg, sizeof(msg));	
}

/**
 * tri_controller_init() - install the controller resources.
 *
 * @c: pointer to the tri state.
 *
 * Return:	None.
 **/
static void tri_controller_init(struct tri_data_s *c)
{
	tri_ep_t *batt, *disp;
	os_queue_elem_t msg;

	/* Get the pointer to the controller state. */
	batt = &c->c_batt;
	disp = &c->c_disp;

	/* Install the controller thread. */
	c->c_thr = os_thread_create("controller", PRIO, Q_SIZE);
	
	/* Initialize the cable controller endpoints. */
	tri_ep_init(batt, "c_batt", NULL, "/van/ctrl_batt", 0);
	tri_ep_init(disp, "c_disp", NULL, "/van/ctrl_disp", 0);

	/* Start the controller. */
	os_memset(&msg, 0, sizeof(msg));	
	msg.cb = tri_ctrl_exec;
	OS_SEND(c->c_thr, &msg, sizeof(msg));	
}

/**
 * tri_conf() - define the test settings.
 *
 * @c: pointer to the tri state.
 *
 * * Return:	None.
 **/
static void tri_conf(struct tri_data_s *c)
{
	/* Suspend the test threads, if no I/O data are available. */
	c->n_batt.use_wait = 1;

	/* Define the number of the test cycles. */
	c->c_batt.cycles = 11;
	c->c_disp.cycles = 22;
	c->n_batt.cycles = 44;
	c->n_disp.cycles = 999;

	/* Activate the transfer trace. */
	c->my_trace = 1;
	
	/* XXX */
	// os_trace_button(1);
}

/**
 * tri_start() - install the platform for the control technology.
 *
 * Return:	the test status.
 **/
static int tri_start(void)
{
	// os_statistics_t expected = { 26, 28, 7, 2346, 2332, 7 };
	struct tri_data_s *c;
	int stat;

	/* Initialize the return value. */
	stat = 0;
	
	/* Get the pointer to the tri state. */
	c = &tri_data;

	/* Define the test settings. */
	tri_conf(c);

	/* Control the lifetime of the loop test. */
	os_sem_init(&c->suspend, 0);

	/* Install the controller resources. */
	tri_controller_init(c);

	/* Install the battery resources. */
	tri_battery_init(c);

	/* Install the display resources. */
	tri_display_init(c);

	/* The main process shall be suspended, as long as the transfer is in
	 * progess. */
	tri_wait();

	/* XXX Delay for os_free(). */
	// usleep(100);
	
	/* Verify the OS state. */
	// stat = test_os_stat(&expected);

	return stat;
}

/*============================================================================
  EXPORTED FUNCTIONS
  ============================================================================*/
/**
 * tri_init() - define vot_p for TEST_ASSERT_EQ.
 *
 * @stat:  pointer to the test configuration.
 *
 * Return:	None.
 **/
void tri_init(test_stat_t *stat)
{
	vote_p = stat;
}

/**
 * tri_run() - the van controller interworking with the battery device.
 *
 * Return:	None.
 **/
void tri_run(void)
{
	/* Execute the list of the test cases. */
	test_set_process(TEST_ADD(tri_system));
}
