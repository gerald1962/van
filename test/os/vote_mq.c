// SPDX-License-Identifier: GPL-2.0

/*
 * Coverage of the VAN operating system tests:
 * test of the message queue interfaces.
 *
 * Copyright (C) 2022 Gerald Schueller <gerald.schueller@web.de>
 */

/*============================================================================
  IMPORTED INCLUDE REFERENCES
  ============================================================================*/
#include "vote.h"  /* Van OS test environment. */

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
static int mq_start(void);
static int mq_init_loop(void);
static int mq_io(void);
static int mq_overflow(void);
static int mq_stop(void);

/*============================================================================
  LOCAL DATA
  ============================================================================*/
/* Define vot_p for TEST_ASSERT_EQ. */
static test_stat_t *vote_p;

/* List of the of the message queue test cases. */
static test_elem_t mq_system[] = {
	{ TEST_ADD(mq_start), 0 },
	{ TEST_ADD(mq_init_loop), 0 },	
	{ TEST_ADD(mq_io), 0 },	
	{ TEST_ADD(mq_overflow), 0 },	
	{ TEST_ADD(mq_stop), 0 },	
	{ NULL, NULL, 0 }
};

/*============================================================================
  LOCAL FUNCTIONS
  ============================================================================*/
/**
 * mq_overflow() - test the overflow of the message queue.
 *
 * Return:	the test status.
 **/
static int mq_overflow(void)
{
	os_statistics_t expected = { 6, 4, 0, 2382, 2382, 0 };
	int stat;
	
	/* XXX */
	
	/* Verify the OS state. */
	stat = test_os_stat(&expected);

	return stat;
}

/**
 * mq_io() - verify the message queue output.
 *
 * Return:	the test status.
 **/
static int mq_io(void)
{
	os_statistics_t expected = { 6, 4, 0, 2382, 2382, 0 };
	int stat;
	
	/* XXX */
	
	/* Verify the OS state. */
	stat = test_os_stat(&expected);

	return stat;
}

/**
 * mq_init_loop() - test the allocation and release of the message queue
 *                  objects with a random size.
 *
 * Return:	the test status.
 **/
static int mq_init_loop(void)
{	
	os_statistics_t expected = { 6, 4, 0, 2382, 2382, 0 };
	void  *q;
	long limit, size;
	int i, stat;

	/* Take the second value of the current time as random start value for the
	 * random number generator. */
	srand(time(NULL));

	/* Calculate the limit of the message buffer size; 1 Mb. */
	limit = 2 << 20;

	/* Loop thru the allocation and release of the message queue objects. */
	for (i = 0; i < 11; i++) {
		/* Consider the upper and lower bound of the buffer size. */
		size = random() % limit;
		if (size < 1)
			size = 1;

		/* Create a message queue object. */
		q = os_mq_init(size);
		
		/* Free a message queue object. */		
		os_mq_delete(q);
	}
	
	/* Verify the OS state. */
	stat = test_os_stat(&expected);

	return stat;
}

/**
 * mq_stop() - end of the message interface test.
 *
 * Return:	the test status.
 **/
static int mq_stop(void)
{
	os_statistics_t expected = { 6, 4, 0, 2382, 2382, 0 };
	int stat;

	/* Verify the OS state. */
	stat = test_os_stat(&expected);

	return stat;
}

/**
 * mq_start() - start of the message interface test.
 *
 * Return:	the test status.
 **/
static int mq_start(void)
{
	os_statistics_t expected = { 6, 4, 0, 2360, 2360, 0 };
	int stat;

	/* Verify the OS state. */
	stat = test_os_stat(&expected);

	return stat;
}

/*============================================================================
  EXPORTED FUNCTIONS
  ============================================================================*/
/**
 * mq_init() - define vot_p for TEST_ASSERT_EQ.
 *
 * @stat:  pointer to the test configuration.
 *
 * Return:	None.
 **/
void mq_init(test_stat_t *stat)
{
	vote_p = stat;
}

/**
 * mq_run() - test the clock operations.
 *
 * Return:	None.
 **/
void mq_run(void)
{
	/* Execute the list of the test cases. */
	test_set_process(TEST_ADD(mq_system));
}
