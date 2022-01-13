// SPDX-License-Identifier: GPL-2.0

/*
 * Coverage of the VAN operating system tests:
 * van OS test environment.
 *
 * Copyright (C) 2022 Gerald Schueller <gerald.schueller@web.de>
 */
/*============================================================================
  IMPORTED INCLUDE REFERENCES
  ============================================================================*/
#include "os.h"    /* Van Operating system: os_clock_init(). */
#include "vote.h"  /* Van OS test environment. */
#include <time.h>  /* Get time in seconds: time(). */

/*============================================================================
  EXPORTED INCLUDE REFERENCES
  ============================================================================*/
/*============================================================================
  LOCAL NAME CONSTANTS DEFINITIONS
  ============================================================================*/
#define PRIO  OS_THREAD_PRIO_FOREG  /* Thread foreground priority. */
#define Q_S   1                     /* Size of the thread input queue. */

/*============================================================================
  MACROS
  ============================================================================*/
/*============================================================================
  LOCAL TYPE DEFINITIONS
  ============================================================================*/
/*============================================================================
  LOCAL FUNCTION PROTOTYPES
  ============================================================================*/
static int clk_start(void);
static int clk_1st_clock(void);
static int clk_all_clocks(void);
static int clk_stop(void);

/*============================================================================
  LOCAL DATA
  ============================================================================*/
/* Define vot_p for TEST_ASSERT_EQ. */
static test_stat_t *vote_p;

/* List of the of the clock test cases. */
static test_elem_t clk_system[] = {
	{ TEST_ADD(clk_start), 0 },
	{ TEST_ADD(clk_1st_clock), 0 },
	{ TEST_ADD(clk_all_clocks), 0 },
	{ TEST_ADD(clk_stop), 0 },
	{ NULL, NULL, 0 }
};

/**
 * clk - clock test control. 
 *
 * done:     test termination condition in clk_all_clocks().
 * suspend:  test control in clk_all_clocks().
 **/
static struct {
	atomic_int done[OS_CLOCK_LIMIT];
	sem_t suspend;
} clk;

/*============================================================================
  LOCAL FUNCTIONS
  ============================================================================*/
/**
 * clk_thr_exec() - execute clock operations in the van OS thread context.
 *
 * @msg:  pointer to the generic input message.
 *
 * Return:	None.
 **/
static void clk_thr_exec(os_queue_elem_t *msg)
{
	long exec_time, limit;
	int interval, t_id, i;

	/* Define the clock interval in millisecons. */
	interval = 25;
	
	/* Create the interval timer. */
	t_id = os_clock_init("vote_clk_x", interval);

	/* Start the periodic timer. */
	os_clock_start(t_id);

	/* Take the second value of the current time as random start value for the
	 * random number generator. */
	srand(time(NULL));

	/* Calculate the max. execution time with the intended violation of the
	 * adjusted clock. */
	limit = interval / 2;

	/* Test the periodic timer. */
	for(i = 0; i < 7; i++) {
		/* Simulate the current execute time. */
		exec_time = random() % limit;
		if (exec_time < 1)
			exec_time = 1;
		
		/* Sleep n milliseconds. */
		os_clock_msleep(exec_time);
		
		/* Wait for the timer expiration. */
		os_clock_barrier(t_id);		
	}

	/* Stop the periodic timer. */
	os_clock_stop(t_id);

	/* Print the test cycle information. */
	os_clock_trace(t_id, OS_CT_LAST);

	/* Destroy the  interval timer. */
	os_clock_delete(t_id);

	/* Update the test status. */
	atomic_store(&clk.done[t_id], 1);
	os_sem_release(&clk.suspend);
}

/**
 * clk_all_clocks() - test all clocks with one shot.
 *
 * Return:	the test status.
 **/
static int clk_all_clocks(void)
{
	os_statistics_t expected = { 4, 4, 0, 2345, 2345, 0 };
	os_queue_elem_t msg;
	void *thr[OS_CLOCK_LIMIT];
	char n[OS_MAX_NAME_LEN];
	int i, stat, busy;

	/* Initialize the test control. */
	os_sem_init(&clk.suspend, 0);
	
	/* Hire test threads for all clocks. */
	for (i = 0; i < OS_CLOCK_LIMIT; i++) {		
		/* Build a unique thread name because, perhaps we have to 
		 * troubleshoot. */
		snprintf(n, OS_MAX_NAME_LEN, "vote_clk_%d", i);
		
		/* Demand the test thread. */
		thr[i] = os_thread_create(n, PRIO, Q_S);
	}

	/* Give the threads a job. */
	for (i = 0; i < OS_CLOCK_LIMIT; i++) {
		/* Every thread shall run with its own clock, i.e. it is busy
		 * itself with clock operations. */
		os_memset(&msg, 0, sizeof(msg));
		msg.param = thr[i];
		msg.cb    = clk_thr_exec;
		OS_SEND(thr[i], &msg, sizeof(msg));	
	}
	
	/* Wait for the end of the test threads. */
	for (busy = 1; busy; ) {
		/* Wait for the end of the test threads. */
		os_sem_wait(&clk.suspend);
		
		/* Test the run state of the thread. */
		for(i = 0;  i < OS_CLOCK_LIMIT; i++) {
			stat = atomic_load(&clk.done[i]);
			busy = busy && stat;
		}		
	}
	
	/* Release  the test control. */
	os_sem_delete(&clk.suspend);

	/* Release the test threads. */
	for (i = 0 ; i < OS_CLOCK_LIMIT; i++) {
		/* Lock, power down and remove the thread. */
		os_thread_destroy(thr[i]);
	}

	/* Verify the OS state. */
	stat = test_os_stat(&expected);

	return stat;
}

/**
 * clk_1st_clock() - test the first clock with one shot.
 *
 * Return:	the test status.
 **/
static int clk_1st_clock(void)
{
	os_statistics_t expected = { 4, 4, 0, 2341, 2341, 0 };
	int t_id, stat;
	
	/* Create the interval timer. */
	t_id = os_clock_init("vote_clk_1", 25);

	/* Start the periodic timer. */
	os_clock_start(t_id);
	
	/* Sleep n milliseconds. */
	os_clock_msleep(10);

	/* Wait for the timer expiration. */
	os_clock_barrier(t_id);
		
	/* Stop the periodic timer. */
	os_clock_stop(t_id);

	/* Print the test cycle information. */
	os_clock_trace(t_id, OS_CT_LAST);

	/* Destroy the  interval timer. */
	os_clock_delete(t_id);
	
	/* Verify the OS state. */
	stat = test_os_stat(&expected);

	return stat;
}

/**
 * clk_stop() - xxx
 *
 * Return:	the test status.
 **/
static int clk_stop(void)
{
	os_statistics_t expected = { 4, 4, 0, 2345, 2345, 0 };
	int stat;

	/* Verify the OS state. */
	stat = test_os_stat(&expected);

	return stat;
}

/**
 * clk_start() - xxx
 *
 * Return:	the test status.
 **/
static int clk_start(void)
{
	os_statistics_t expected = { 4, 4, 0, 2341, 2341, 0 };
	int stat;

	/* Verify the OS state. */
	stat = test_os_stat(&expected);

	return stat;
}

/*============================================================================
  EXPORTED FUNCTIONS
  ============================================================================*/
/**
 * clk_init() - define vot_p for TEST_ASSERT_EQ.
 *
 * @stat:  pointer to the test configuration.
 *
 * Return:	None.
 **/
void clk_init(test_stat_t *stat)
{
	vote_p = stat;
}

/**
 * tri_run() - test the clock operations.
 *
 * Return:	None.
 **/
void clk_run(void)
{
	/* Execute the list of the test cases. */
	test_set_process(TEST_ADD(clk_system));
}
