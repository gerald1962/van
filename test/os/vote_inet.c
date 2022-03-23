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
static int inet_start(void);
static int inet_stop(void);

/*============================================================================
  LOCAL DATA
  ============================================================================*/
/* Define vot_p for TEST_ASSERT_EQ. */
static test_stat_t *vote_p;

/* List of the of the message queue test cases. */
static test_elem_t inet_system[] = {
	{ TEST_ADD(inet_start), 0 },
	{ TEST_ADD(inet_stop), 0 },	
	{ NULL, NULL, 0 }
};

/*============================================================================
  LOCAL FUNCTIONS
  ============================================================================*/
/**
 * inet_stop() - end of the inet interface test.
 *
 * Return:	the test status.
 **/
static int inet_stop(void)
{
	os_statistics_t expected = { 6, 4, 0, 2382, 2382, 0 };
	int stat;

	/* Verify the OS state. */
	stat = test_os_stat(&expected);

	return stat;
}

/**
 * inet_start() - start of the inet interface test.
 *
 * Return:	the test status.
 **/
static int inet_start(void)
{
	os_statistics_t expected = { 6, 4, 0, 2382, 2382, 0 };
	int stat;

	/* Verify the OS state. */
	stat = test_os_stat(&expected);

	return stat;
}

/*============================================================================
  EXPORTED FUNCTIONS
  ============================================================================*/
/**
 * inet_init() - define vot_p for TEST_ASSERT_EQ.
 *
 * @stat:  pointer to the test configuration.
 *
 * Return:	None.
 **/
void inet_init(test_stat_t *stat)
{
	vote_p = stat;
}

/**
 * inet_run() - test the inet operations.
 *
 * Return:	None.
 **/
void inet_run(void)
{
	/* Execute the list of the test cases. */
	test_set_process(TEST_ADD(inet_system));
}
