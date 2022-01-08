// SPDX-License-Identifier: GPL-2.0

/*
 * cop - control technology platform.
 *
 * Copyright (C) 2022 Gerald Schueller <gerald.schueller@web.de>
 */

/*============================================================================
  IMPORTED INCLUDE REFERENCES
  ============================================================================*/
#include <getopt.h>  /* Common Unix interfaces: getopt_long_only().*/
#include "os.h"      /* Operating system: os_sem_create(). */

/*============================================================================
  EXPORTED INCLUDE REFERENCES
  ============================================================================*/
/*============================================================================
  LOCAL NAME CONSTANTS DEFINITIONS
  ============================================================================*/
#define P  "C>"  /* Prefix of the trace output. */

#define PRIO    OS_THREAD_PRIO_FOREG  /* Thread foreground priority. */
#define Q_SIZE  4                     /* Size of the thread input queue. */

/*============================================================================
  MACROS
  ============================================================================*/
/* cop trace with filter. */
#define TRACE(info_)  do { \
		if (cop_data.my_trace) \
			printf info_; \
} while (0)

/*============================================================================
  LOCAL TYPE DEFINITIONS
  ============================================================================*/
/**
 * cop_ep_t - state of a cable endpoint
 *
 * @alone:     if 1, start cop as ctrl, batt or disp program.
 * @name:      name of the cable endpoint.
 * @tread:     pointer to the device thread.
 * @dev_id:    device id.
 * @use_wait:  if 1, execute the test with os_c_wait().
 * @wait_id:   wait conditions of the cable endpoint.
 * @cycles:    number of the output cycles.
 * @wr_count:  current cycle value.
 * @rd_count:  number of the received packets.
 * @wr_done:   if 1, everything sent, resume the main process.
 * @rd_done:   if 1, everything received, resume the main process.
 **/
typedef struct {
	int    alone;
	char  *name;
	void  *thread;
	int    dev_id;
	int    use_wait;
	int    wait_id;
	int    cycles;
	int    wr_count;
	int    rd_count;
	atomic_int  wr_done;
	atomic_int  rd_done;
} cop_ep_t;

/*============================================================================
  LOCAL DATA
  ============================================================================*/
/**
 * cop_data - state of the control technology platform. 
 *
 * @distributed:  if 0, this program includes all endpoints.
 * @my_trace:     if 1, activate the cop trace.
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
static struct cop_data_s {
	int    distributed;
	int    my_trace;
	sem_t  suspend;
	
	void  *c_thr;
	int    c_wait_id;
	
	cop_ep_t  c_batt;
	cop_ep_t  c_disp;

	cop_ep_t  n_batt;
	cop_ep_t  n_disp;
} cop_data;

/*============================================================================
  LOCAL FUNCTION PROTOTYPES
  ============================================================================*/
static void cop_usage(void);

/*============================================================================
  LOCAL FUNCTIONS
  ============================================================================*/
/**
 * cop_resume() - resume the main process.
 *
 * @done: pointer to the final trigger of a participating data transfer thread.
 *
 * Return:	None.
 **/
static void cop_resume(atomic_int *done)
{
	/* Resume the main process. */
	atomic_store(done, 1);
	os_sem_release(&cop_data.suspend);
}

/**
 * cop_read() - generic read operation for all endpoints of any cable: any
 * controller or a neighbour endpoint expects data with the non blocking
 * synchronous read operation from the other endpoint.
 *
 * @ep:         pointer to the endpoint state.
 * @wait_cond:  if we return 1, repeat the read operation.
 *
 * Return:	0, if the write operation is complete.
 **/
static int cop_read(cop_ep_t *ep, int *wait_cond)
{
	char buf[OS_BUF_SIZE];
	int done, n;
	
	/* Initialize the return value. */
	*wait_cond = 0;

	/* Test the final condition of the consumer. */
	done = atomic_load(&ep->rd_done);
	if (done)
		return 0;

	/* Wait for data from the producer. */
	n = os_c_read(ep->dev_id, buf, OS_BUF_SIZE);
	if (n < 1) {
		*wait_cond = 1;
		return 1;
	}
		
	TRACE(("%s rcvd: [e:%s, b:\"%s\", s:%d]\n", P, ep->name, buf, n));
		       
	/* Test the end condition of the test. */
	if (os_strcmp(buf, "DONE") == 0) {
		cop_resume(&ep->rd_done);
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
 * cop_write() - generic write operation for all endpoints of any cable: the
 * controller or a neighbour sends data with the non blocking synchronous write
 * operation to the other endpoint.
 *
 * @ep:         pointer to the endpoint state.
 * @wait_cond:  if we return 1, repeat the write operation.
 *
 * Return:	0, if the write operation is complete.
 **/
static int cop_write(cop_ep_t *ep, int *wait_cond)
{
	char buf[OS_BUF_SIZE];
	int done, n, rv;
	
	/* Initialize the return value. */
	*wait_cond = 0;
	
	/* Test the final condition of the producer. */
	done = atomic_load(&ep->wr_done);
	if (done)
		return 0;

	/* Test the cycle counter. */
	if (ep->wr_count > ep->cycles) {
		/* Resume the main process. */
		cop_resume(&ep->wr_done);
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
 * cop_batt_exec() - the battery neighbour of the controller sends and receives
 * data with the non blocking synchronous operations.
 *
 * @msg:  pointer to the generic input message.
 *
 * Return:	None.
 **/
static void cop_batt_exec(os_queue_elem_t *msg)
{
	cop_ep_t *ep;
	int busy_rd, busy_wr, no_c_inp, no_c_out;

	/* Initialize the return values. */
	busy_rd = 1;
	busy_wr = 1;
	
	/* Initialize the wait condition for cable endpoint operations. */
	no_c_inp = 0;
	no_c_out = 0;

	/* Get the pointer to the endpoint state. */
	ep = &cop_data.n_batt;

	/* The battery thread analyzes or generates data from or to the
	 * controller with the non blocking syncronous copy read or write
	 * operation. */
	while (busy_rd || busy_wr) {
		/* Read the input from the controller. */
		busy_rd = cop_read(ep, &no_c_inp);

		/* Generate output for the controller. */
		busy_wr = cop_write(ep, &no_c_out);
		
		/* Avoid an endles loop, if no I/O data are available. */
		if (ep->use_wait && no_c_inp && no_c_out)
			os_c_wait(ep->wait_id);
	}
	
	TRACE(("%s nops: [e:%s, s:done]\n", P, ep->name));
}

/**
 * cop_ctrl_exec() - the controller sends and receives data with the non blocking
 * synchronous operations either to the battery or to the display.
 *
 * @msg:  pointer to the generic input message.
 *
 * Return:	None.
 **/
static void cop_ctrl_exec(os_queue_elem_t *msg)
{
	struct cop_data_s *c;
	cop_ep_t *batt, *disp;
	int d_busy_rd, b_busy_rd, b_busy_wr, d_busy_wr,
		no_d_inp, no_b_inp, no_b_out, no_d_out, use_wait;
		
	/* Initialize the return values. */
	d_busy_rd = 1;
	b_busy_rd = 1;
	b_busy_wr = 1;
	d_busy_wr = 1;
	
	/* Initialize the wait conditions for cable endpoint operations. */
	no_d_inp = 0;
	no_b_inp = 0;
	no_b_out = 0;
	no_d_out = 0;
	
	/* Get the pointer to the cop state. */
	c = &cop_data;

	/* Get the pointer to the endpoint states. */
	batt = &c->c_batt;
	disp = &c->c_disp;

	/* Test the wait condition. */
	use_wait = batt->use_wait || disp->use_wait;
	
	/* The control thread analyzes or generates data from or to the
	 * neighbour endpoints with the non blocking syncronous copy read or
	 * write operation. */
	while (d_busy_rd || b_busy_rd || b_busy_wr || d_busy_wr) {
		/* Read the input from the display. */
		d_busy_rd = cop_read(disp, &no_d_inp);

		/* Read the input from the battery. */
		b_busy_rd = cop_read(batt, &no_b_inp);
		
		/* Generate output for the battery. */
		b_busy_wr = cop_write(batt, &no_b_out);
		
		/* Generate output for the display. */
		d_busy_wr = cop_write(disp, &no_d_out);
		
		/* Avoid an endles loop, if no data from neighbour endpoints are
		 * available. */
		if (use_wait && no_d_inp && no_b_inp && no_b_out && no_d_out)
			os_c_wait(c->c_wait_id);
	}
	
	TRACE(("%s nops: [e:%s/%s, s:done]\n", P, batt->name, disp->name));
}

/**
 * cop_disp_exec() - the display neighbour of the controller sends and receives
 * data with the non blocking synchronous operations.
 *
 * @msg:  pointer to the generic input message.
 *
 * Return:	None.
 **/
static void cop_disp_exec(os_queue_elem_t *msg)
{
	cop_ep_t *ep;
	int busy_rd, busy_wr, no_c_inp, no_c_out;

	/* Initialize the return values. */
	busy_rd = 1;
	busy_wr = 1;
	
	/* Initialize the wait condition for cable endpoint operations. */
	no_c_inp = 0;
	no_c_out = 0;

	/* Get the pointer to the endpoint state. */
	ep = &cop_data.n_disp;

	/* The display thread analyzes or generates data from or to the
	 * controller with the non blocking syncronous copy read or write
	 * operation. */
	while (busy_rd || busy_wr) {
		/* Read the input from the controller. */
		busy_rd = cop_read(ep, &no_c_inp);
		
		/* Generate output for the controller. */
		busy_wr = cop_write(ep, &no_c_out);
		
		/* Avoid an endles loop, if no I/O data are available. */
		if (ep->use_wait && no_c_inp && no_c_out)
			os_c_wait(ep->wait_id);
	}
	
	TRACE(("%s nops: [e:%s, s:done]\n", P, ep->name));
}

/**
 * cop_wait() - the main process is waiting for the end of the data transfer.
 *
 * Return:	None.
 **/
static void cop_wait(void)
{
	cop_ep_t *c_batt, *c_disp, *n_batt, *n_disp;
	int done, wr_done, rd_done;
	
	/* Get the pointer to all endpoint states. */
	c_batt = &cop_data.c_batt;
	c_disp = &cop_data.c_disp;
	n_batt = &cop_data.n_batt;
	n_disp = &cop_data.n_disp;

	/* Initialize the control status. */
	done = 0;
	
	/* Test the status of the data transfer threads. */
	while(! done) {
		/* Suspend the main process. */
		os_sem_wait(&cop_data.suspend);
		
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
 * cop_ep_cleanup() - release the resources of a cable endpooint.
 *
 * @ep:  pointer to the endpoint state.
 *
 * Return:	None.
 **/
static void cop_ep_cleanup(cop_ep_t *ep)
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
 * cop_cleanup() - release the platform for the control technology.
 *
 * Return:	None.
 **/
static void cop_cleanup(void)
{
	struct cop_data_s *c;
	
	/* Get the pointer to the cop state. */
	c = &cop_data;
	
	/* Battery. */
	if (! c->distributed || c->n_batt.alone)
		cop_ep_cleanup(&c->n_batt);

	/* Display. */
	if (! c->distributed || c->n_disp.alone)
		cop_ep_cleanup(&c->n_disp);

	/* Controller. */
	if (! c->distributed || c->c_batt.alone || c->c_disp.alone) {
		/* Remove the controller threads */
		os_thread_destroy(c->c_thr);

		/* Release the controller resources. */
		cop_ep_cleanup(&c->c_batt);
		cop_ep_cleanup(&c->c_disp);

		/* Release the wait condition of the controller. */
		os_c_wait_release(c->c_wait_id);
	}
	
	/* Release the control semaphore for the main process. */
	os_sem_delete(&c->suspend);

	/* Release the OS resources. */
	os_exit();
}

/**
 * cop_ep_init() - initialize a cable endpoint.
 *
 * @ep:      pointer to the endpoint state.
 * @ep_n:    trace name of the cable endpoint.
 * @dev_n:   device name of the cable endpoint.
 * @wait:    if 1, initialize the wait condition.
 * @cycles:  number of the output cycles.
 *
 * Return:	None.
 **/
static void cop_ep_init(cop_ep_t *ep, char *ep_n, char *thr_n, char *dev_n,
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
 * cop_display_init() - install the display resources.
 *
 * @c: pointer to the cop state.
 *
 * Return:	None.
 **/
static void cop_display_init(struct cop_data_s *c)
{
	os_queue_elem_t msg;
	cop_ep_t *disp;

	/* Get the pointer to the battery state. */
	disp = &c->n_disp;
	
	/* Test the program configuration. */
	if (c->distributed && ! disp->alone) {
		/* Resume the main process. */
		cop_resume(&disp->rd_done);
		cop_resume(&disp->wr_done);
		return;
	}
	
	/* Initialize the battery endpoints. */
	cop_ep_init(disp, "n_disp", "display", "/display", 1);
	
	/* Start the display. */
	os_memset(&msg, 0, sizeof(msg));	
	msg.cb = cop_disp_exec;
	OS_SEND(disp->thread, &msg, sizeof(msg));	
}

/**
 * cop_battery_init() - install the battery resources.
 *
 * @c: pointer to the cop state.
 *
 * Return:	None.
 **/
static void cop_battery_init(struct cop_data_s *c)
{
	os_queue_elem_t msg;
	cop_ep_t *batt;
	
	/* Get the pointer to the battery state. */
	batt = &c->n_batt;
	
	/* Test the program configuration. */
	if (c->distributed && ! batt->alone) {
		/* Resume the main process. */
		cop_resume(&batt->rd_done);
		cop_resume(&batt->wr_done);
		return;
	}
	
	/* Initialize the battery endpoints. */
	cop_ep_init(batt, "n_batt", "battery", "/battery", 1);

	/* Start the battery. */
	os_memset(&msg, 0, sizeof(msg));	
	msg.cb = cop_batt_exec;
	OS_SEND(batt->thread, &msg, sizeof(msg));	
}

/**
 * cop_controller_init() - install the controller resources.
 *
 * @c: pointer to the cop state.
 *
 * Return:	None.
 **/
static void cop_controller_init(struct cop_data_s *c)
{
	os_queue_elem_t msg;
	cop_ep_t *batt, *disp;
	int wait_list[2];

	/* Get the pointer to the controller state. */
	batt = &c->c_batt;
	disp = &c->c_disp;

	/* Test the program configuration. */
	if (c->distributed && ! batt->alone && ! disp->alone) {
		/* Resume the main process. */
		cop_resume(&batt->rd_done);
		cop_resume(&batt->wr_done);
		cop_resume(&disp->rd_done);
		cop_resume(&disp->wr_done);
		return;
	}
	
	/* Install the controller thread. */
	c->c_thr = os_thread_create("controller", PRIO, Q_SIZE);
	
	/* Initialize the cable controller endpoints. */
	cop_ep_init(batt, "c_batt", NULL, "/ctrl_batt", 0);
	cop_ep_init(disp, "c_disp", NULL, "/ctrl_disp", 0);

	/* Avoid an endlos loop, if no controller I/O data are available. */
	wait_list[0] = batt->dev_id;
	wait_list[1] = disp->dev_id;
	c->c_wait_id = os_c_wait_init(wait_list, 2);

	/* Start the controller. */
	os_memset(&msg, 0, sizeof(msg));	
	msg.cb = cop_ctrl_exec;
	OS_SEND(c->c_thr, &msg, sizeof(msg));	
}

/**
 * cop_os_init() - initialize the operating system.
 *
 * @c: pointer to the cop state.
 *
 * Return:	None.
 **/
static void cop_os_init(struct cop_data_s *c)
{
	/* Test the program configuration. */
	if (! c->distributed || c->c_batt.alone || c->c_disp.alone)
		os_init(1);
	else
		os_init(0);
}

/**
 * cop_init() - install the platform for the control technology.
 *
 * Return:	None.
 **/
static void cop_init(void)
{
	struct cop_data_s *c;
	
	/* Get the pointer to the cop state. */
	c = &cop_data;

	/* Initialize the operating system. */
	cop_os_init(c);

	/* Deactivate the OS trace. */
	os_trace_button(0);

	/* Control the lifetime of the loop test. */
	os_sem_init(&c->suspend, 0);

	/* Install the controller resources. */
	cop_controller_init(c);

	/* Install the battery resources. */
	cop_battery_init(c);

	/* Install the display resources. */
	cop_display_init(c);
	
	/* The main process shall be suspended, as long as the transfer is in
	 * progess. */
	cop_wait();
}

/**
 * cop_str_to_int() - convert a string to integer.
 *
 * @string:   pointer to the gotten string.
 * @integer:  pointer to the expected integer.
 *
 * Return:	if applicaton error, print help and terminate the program.
 **/
static void cop_str_to_int(char *string, int *integer)
{
	/* Entry condition. */
	if (string == NULL) {
		cop_usage();
		exit(1);
	}

	/* Convert the string to integer in the decimal system. */
	*integer = strtol(string, NULL, 10);
}

/**
 * cop_standalone_conf() - configure cop as stand-alone programm for the van
 * triad: controller-battery-display.
 *
 * @c:       pointer to the cop state.
 * @string:  pointer to the gotten string.
 *
 * Return:	None.
 **/
static void cop_standalone_conf(struct cop_data_s *c, char *string)
{
	int len;
	
	/* Entry condition. */
	if (string == NULL) {
		cop_usage();
		exit(1);
	}

	/* Formal test of the -s option argument. */
	len = os_strlen(string);
	if (len != 1) {
		cop_usage();
		exit(1);
	}

	/* Parse the -s argument. */
	switch (*string) {
	case 'b':
		/* Start cop as battery program. */
		c->distributed  = 1;
		c->n_batt.alone = 1;
		break;
	case 'c':
		/* Start cop as controller program. */
		c->distributed  = 1;
		c->c_batt.alone = 1;
		c->c_disp.alone = 1;
		break;
	case 'd':
		/* Start cop as display program. */
		c->distributed  = 1;
		c->n_disp.alone = 1;
		break;
	default:
		cop_usage();
		exit(1);
		break;
	}
}

/**
 * cop_usage() - provide information aboute the cop configuration.
 *
 * Return:	None.
 **/
static void cop_usage(void)
{
	struct cop_data_s *c;
	cop_ep_t *cb, *cd, *nb, *nd;

	/* Get the pointer to the cop state. */
	c = &cop_data;

	/* Get the pointer to the endpoints. */
	cb = &c->c_batt;
	cd = &c->c_disp;
	nb = &c->n_batt;
	nd = &c->n_disp;

	printf("cop - control technology platform\n");
	printf("  -h      show this usage\n");
	printf("  -t      print the cop trace information\n");
	
	printf("Test cycle settings:\n");
	printf("  -dcc n  display generator cycles towards controller\n");
	printf("  -cbc n  controller generator cycles towards battery\n");
	printf("  -bcc n  battery generator cycles towards controller\n");
	printf("  -cdc n  controller generator cycles towards display\n");
	
	printf("Setting of the wait conditions:\n");
	printf("  -dw     display cycles with wait condition\n");
	printf("  -cw     controller cycles with wait condition\n");
	printf("  -bw     battery cycles with wait condition\n");

	printf("\nProgramm configuration:\n");
	printf("  -s x    execute cop as stand-alone program: substitue x with:\n");
	printf("          c  controller\n");
	printf("          b  battery\n");
	printf("          d  display\n");
	
	printf("\nDefault settings:\n");
	printf("  Distributed:              %s\n", c->distributed ? "yes" : "no");
	printf("  Cop trace:                %s\n", c->my_trace ? "on" : "off");
	printf("  Display cycles:           %d\n", nd->cycles);
	printf("  Ctrl->battery cycles:     %d\n", cb->cycles);
	printf("  Battery cycles:           %d\n", nb->cycles);
	printf("  Ctrl->display cycles:     %d\n", cd->cycles);
	printf("  Display wait condition:   %s\n", nd->use_wait ? "on" : "off");
	printf("  Ctrl wait conditions:     %s\n", cb->use_wait ? "on" : "off");
	printf("  Battery wait condition:   %s\n", nb->use_wait ? "on" : "off");
	printf("  Stand-alone disp program: %s\n", nd->alone ? "yes" : "no");
	printf("  Stand-alone ctrl program: %s\n", cd->alone ? "yes" : "no");
	printf("  Stand-alone batt program: %s\n", nb->alone ? "yes" : "no");
}

/*============================================================================
  EXPORTED FUNCTIONS
  ============================================================================*/
/**
 * main() - start function of the control technology platform.
 *
 * @argc:  argument counter.
 * @argv:  list of the arguments.
 *
 * Return:	0 or force a software trap.
 **/
int main(int argc, char *argv[])
{
	struct cop_data_s *c;
	cop_ep_t *cb, *cd, *nb, *nd;
	int opt;

	/* An array describing valid long options for getopt_long_only().  */
	const struct option opts[] = {
		{ "help",   0, NULL, 'h' },
		{ "trace",  0, NULL, 't' },
		
		/* Test cycle settings: */
		{ "dcc",    1, NULL, 3 },
		{ "cbc",    1, NULL, 4 },
		{ "bcc",    1, NULL, 5 },
		{ "cdc",    1, NULL, 6 },

		/* Setting of the wait conditions: */
		{ "dw",     0, NULL, 7 },
		{ "cw",     0, NULL, 8 },
		{ "bw",     0, NULL, 9 },

		/* Required at end of array: */
		{ NULL,       0, NULL, 0   }
	};
		
	/* Get the pointer to the cop state. */
	c = &cop_data;

	/* Get the pointer to the endpoints. */
	cb = &c->c_batt;
	cd = &c->c_disp;
	nb = &c->n_batt;
	nd = &c->n_disp;
	
	/* Analyze the paramer, by which the cop has been invoked. */
	while ((opt = getopt_long_only(argc, argv, "hs:t", opts, NULL)) != -1) {
		/* Analyze the current string arguments. */
		switch (opt) {
		case 'h':
			/* Support the user. */
			cop_usage();
			exit(0);
			break;
		case 's':
			/* Configure cop as stand-alone programm for the van
			 * triad: controller-battery-display. */
			cop_standalone_conf(c, optarg);
			break;
		case 't':
			/* Switch on the cop trace. */
			c->my_trace = 1;
			break;
		case 3:
			/* Test the wire between display and controller. */
			cop_str_to_int(optarg, &nd->cycles);
			break;
		case 4:
			/* Test the wire between controller and battery. */
			cop_str_to_int(optarg, &cb->cycles);
			break;
		case 5:
			/* Test the wire between battery and controller. */
			cop_str_to_int(optarg, &nb->cycles);
			break;
		case 6:
			/* Test the wire between controller and display. */
			cop_str_to_int(optarg, &cd->cycles);
			break;
		case 7:
			/* Switch on the display wait condition. */
			nd->use_wait = 1;
			break;
		case 8:
			/* Switch on the controller wait conditions. */
			cb->use_wait = 1;
			cd->use_wait = 1;
			break;
		case 9:
			/* Switch on the battery wait condition. */
			nb->use_wait = 1;
			break;
		default:
			cop_usage();
			exit(1);
			break;
		}
	}

	/* Install the platform for the control technology. */
	cop_init();

	/* Release the platform of the control technology. */
	cop_cleanup();

	printf("\nTest settings:\n");
	printf("  Distributed:              %s\n", c->distributed ? "yes" : "no");
	printf("  Cop trace:                %s\n", c->my_trace ? "on" : "off");
	printf("  Display cycles:           %d\n", nd->cycles);
	printf("  Ctrl->battery cycles:     %d\n", cb->cycles);
	printf("  Battery cycles:           %d\n", nb->cycles);
	printf("  Ctrl->display cycles:     %d\n", cd->cycles);
	printf("  Display wait condition:   %s\n", nd->use_wait ? "on" : "off");
	printf("  Ctrl wait conditions:     %s\n", cb->use_wait ? "on" : "off");
	printf("  Battery wait condition:   %s\n", nb->use_wait ? "on" : "off");
	printf("  Stand-alone disp program: %s\n", nd->alone ? "yes" : "no");
	printf("  Stand-alone ctrl program: %s\n", cd->alone ? "yes" : "no");
	printf("  Stand-alone batt program: %s\n", nb->alone ? "yes" : "no");

	return (0);
}
