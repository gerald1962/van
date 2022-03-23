// SPDX-License-Identifier: GPL-2.0

/*
 * tick - Tcl/Tk kit.
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
/*============================================================================
  MACROS
  ============================================================================*/
/*============================================================================
  LOCAL TYPE DEFINITIONS
  ============================================================================*/
/*============================================================================
  LOCAL FUNCTION PROTOTYPES
  ============================================================================*/
static int tic_start(void);
static int tic_controller(void);
static int tic_display(void);
static int tic_stop(void);

/*============================================================================
  LOCAL DATA
  ============================================================================*/
/* Define vot_p for TEST_ASSERT_EQ. */
static test_stat_t *vote_p;

/**
 * tri_data - state of the control technology platform. 
 *
 * @suspend:  suspend the main process while the test is running.
 * @thr:      pointer to the controller test thread.
 * @id:       id of the controller display end point.
 **/
static struct tri_data_s {
	sem_t   suspend;
	void   *thr;
	int     id;
} tic;

/* List of the of the Tcl/Tk driver test cases. */
static test_elem_t tic_system[] = {
	{ TEST_ADD(tic_start), 0 },
	{ TEST_ADD(tic_controller), 0 },
	{ TEST_ADD(tic_display), 0 },
	{ TEST_ADD(tic_stop), 0 },
	{ NULL, NULL, 0 }
};

/*============================================================================
  LOCAL FUNCTIONS
  ============================================================================*/
/**
 * tic_ctrl_read() - read a display message.
 *
 * @count:  current counter value.
 * @limit:  limit of the send counter.
 *
 * Return:	new counter value.
 **/
static int tic_ctrl_read(int count, int limit)
{
	char buf[OS_BUF_SIZE];
	int rv, n;
	
	/* Read the input from the display. */
	rv = os_c_read(tic.id, buf, OS_BUF_SIZE);

	/* Test the reply from the display. */
	if (rv < 1)
		return count;

	/* Test the generator cycle counter. */
	OS_TRAP_IF(count > limit);

	/* Terminate the message with EOS. */
	buf[rv] = '\0';
			
	/* Test the end condition of the test. */
	if (os_strcmp(buf, "DONE") == 0)
		return count + 1;
			
	/* Convert and test the received counter. */
	n = os_strtol_b10(buf, rv);
	OS_TRAP_IF(n != count);

	return count + 1;
}

/**
 * tic_ctrl_write() - send a message to the display.
 *
 * @count:  current counter value.
 * @limit:  limit of the send counter.
 *
 * Return:	new counter value.
 **/
static int tic_ctrl_write(int count, int limit)
{
	char buf[OS_BUF_SIZE];
	int len, rv;
	
	/* Test the generator cycle counter. */
	if (count < limit) {
		/* Create the digit sequence. */
		len = snprintf(buf, OS_BUF_SIZE, "%d", count);
	}
	else if (count == limit) {
		/* Create the final response. */
		len = snprintf(buf, OS_BUF_SIZE, "DONE");
	}
	else {
		return count;
	}

	/* Include end of string. */
	len++;

	/* Start an attempt of transmission. */
	rv = os_c_write(tic.id, buf, len);

	/* Test the output of the send operation. */
	if (rv < 1)
		return count;

	/* Test the send success. */
	OS_TRAP_IF(rv != len);
			
	/* Increment the generator counter. */
	return count + 1;
}

/**
 * tic_ctrl_exec() - the controller data data with the non blocking synchronous
 * operations with the display.
 *
 * @msg:  pointer to the generic input message.
 *
 * Return:	None.
 **/
static void tic_ctrl_exec(os_queue_elem_t *msg)
{
	int t_id, wr_c, rd_c, l, n;
	
	/* Create the interval timer with a 2 millisecon period. */
	t_id = os_clock_init("ctrl", 2);

	/* Start the periodic timer. */
	os_clock_start(t_id);
	
	/* The control thread analyzes or generates data from or to the
	 * display. */
	for (wr_c = 0, rd_c = 0, l = 42; wr_c <= l || rd_c <= l;) {
		/* Send a message to the display. */
		wr_c = tic_ctrl_write(wr_c, l);

		/* Read a display message. */
		rd_c = tic_ctrl_read(rd_c, l);
		
		/* Wait for the controller clock tick. */
		n = os_clock_barrier(t_id);
	}

	/* Test the buffer status of the output wire. */
	n = os_c_writable(tic.id);
	OS_TRAP_IF(n < 1);

	
	/* Test the the progress of the going out message. */
	n = os_c_sync(tic.id);
	OS_TRAP_IF(n > 0);
	
	/* Stop the periodic timer. */
	os_clock_stop(t_id);
	
	/* Destroy the  interval timer. */
	os_clock_delete(t_id);

	/* Resume the main process. */
	os_sem_release(&tic.suspend);
}

/**
 * tic_display() - start the controller
 *
 * Return:	the test status.
 **/
static int tic_display(void)
{
	os_statistics_t expected = { 10, 11, 2, 2360, 2358, 2 };
	Tcl_Interp  *interp;
	const char *result;
	int rv, code, stat;

	/* Create a Tcl command interpreter. */
	interp = Tcl_CreateInterp();

	/* Create a van OS cabel with a display. */
	rv = Van_Init(interp);
	OS_TRAP_IF(rv != TCL_OK);

	/* Read and evaluate the Tcl script. */
	code = Tcl_EvalFile(interp, "vote_tick.tcl");
	result = Tcl_GetStringResult(interp);

	/* Aanalyze the result of the scipt execution. */
	if (code != TCL_OK) {
		printf("Error was: %s\n", result);
		OS_TRAP_IF(0);
	}
	printf("Result was: %s\n", result);
	
	/* Delete a Tcl command interpreter. */
	Tcl_DeleteInterp(interp);

	/* Wait for the controller. */
	os_sem_wait(&tic.suspend);
	
	/* Verify the OS state. */
	stat = test_os_stat(&expected);
		
	return stat;
}

/**
 * tic_controller() - start the controller
 *
 * Return:	the test status.
 **/
static int tic_controller(void)
{
	os_statistics_t expected = { 10, 11, 2, 2353, 2350, 2 };
	os_queue_elem_t msg;
	int stat;

	/* Start the controller. */
	os_memset(&msg, 0, sizeof(msg));	
	msg.cb = tic_ctrl_exec;
	OS_SEND(tic.thr, &msg, sizeof(msg));	
	
	/* Verify the OS state. */
	stat = test_os_stat(&expected);

	return stat;
}

/**
 * tic_stop() - release the controller test resources.
 *
 * Return:	the test status.
 **/
static int tic_stop(void)
{
	os_statistics_t expected = { 6, 4, 0, 2360, 2360, 0 };
	int stat;

	/* Remove the controller device. */
	os_c_close(tic.id);
		
	/* Remove the controller test thread. */
	os_thread_destroy(tic.thr);

	/* Remove the control semaphore. */
	os_sem_delete(&tic.suspend);
	
	/* Verify the OS state. */
	stat = test_os_stat(&expected);

	return stat;
}

/**
 * tic_start() - install the controller test resources.
 *
 * Return:	the test status.
 **/
static int tic_start(void)
{
	os_statistics_t expected = { 10, 11, 2, 2352, 2350, 2 };
	int stat;

	/* Create the control semaphore for the main process. */
	os_sem_init(&tic.suspend, 0);
	
	/* Create the controller test thread. */
	tic.thr = os_thread_create("controller", OS_THREAD_PRIO_FOREG, 4);

	/* Install the controller display end point. */
	tic.id = os_c_open("/van/ctrl_disp", O_NBLOCK);
	
	/* Verify the OS state. */
	stat = test_os_stat(&expected);
	
	return stat;
}

/*============================================================================
  EXPORTED FUNCTIONS
  ============================================================================*/
/**
 * tic_init() - define vot_p for TEST_ASSERT_EQ.
 *
 * @stat:  pointer to the test configuration.
 *
 * Return:	None.
 **/
void tic_init(test_stat_t *stat)
{
	vote_p = stat;
}

/**
 * tic_run() - load the test set to verify the Tcl/Tk driver.
 *
 * Return:	None.
 **/
void tic_run(void)
{
	/* Execute the list of the test cases. */
	test_set_process(TEST_ADD(tic_system));
}
