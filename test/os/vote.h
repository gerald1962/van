/* SPDX-License-Identifier: GPL-2.0 */

/*
 * Coverage of the VAN operating system tests:
 * list of all test entry points.
 *
 * Copyright (C) 2022 Gerald Schueller <gerald.schueller@web.de>
 */

#ifndef __vote_h__
#define __vote_h__

/*============================================================================
  IMPORTED INCLUDE REFERENCES
  ============================================================================*/
#include <time.h>    /* Get time in seconds: time(). */
#include "os.h"      /* Van Operating system: os_clock_init(). */

/*============================================================================
  NAME CONSTANTS DEFINITIONS
  ============================================================================*/
/* Prompt label. */
#define P "V)"

/*============================================================================
  MACROS
  ============================================================================*/
/* Extend the test sest. */
#define TEST_ADD(label_) #label_, label_

/* Test two values for likeness. */
#define TEST_ASSERT_EQ(arg1_, arg2_) \
do { \
	if ((arg1_) != (arg2_)) { \
		printf("%s %s: MISMATCH at %s,%d: %d == %d\n", P, vote_p->label, \
		       __FILE__, __LINE__, (arg1_), (arg2_)); \
		vote_p->error_n++; \
	} \
} while (0)

/*============================================================================
  TYPE DEFINITIONS
  ============================================================================*/
/**
 * test_stat_t - state of vote.
 *
 * label:      identifikation of the current test case.
 * @test_n:    number of the executed test cases.
 * @error_n:   number of the test case errors.
 **/
typedef struct {
	char  *label;
	int    test_n;
	int    error_n;
} test_stat_t;
	
/**
 * test_elem_t - test set element.
 *
 * @label:     identification of the test.
 * @routine:   test routine.
 * @place:     call location.
 * @expected:  verification value.
 **/
typedef struct {
	char *label;
	int   (*routine) (void);
	int   place;
	int   expected;
} test_elem_t;

/*============================================================================
  GLOBAL DATA
  ============================================================================*/
/*============================================================================
  EXPORTED FUNCTIONS
  ============================================================================*/
/* Test control functions. */
int test_os_stat(os_statistics_t *expected);
void test_set_process(char *label, test_elem_t *elem);

/* Basic van OS interfacees under test. */
void but_init(test_stat_t *stat);
void but_run(void);

/* Test the controller interworking with the battery endpoint. */
void cob_init(test_stat_t *stat);
void cob_run(void);

/* Test the controller-battery-display cabling. */
void tri_init(test_stat_t *stat);
void tri_run(void);

/* Test the van OS clock operations. */
void clk_init(test_stat_t *stat);
void clk_run(void);

/* Test the Tcl/Tk driver. */
void tic_init(test_stat_t *stat);
void tic_run(void);

/* Test the message queue interfaces. */
void mq_init(test_stat_t *stat);
void mq_run(void);

/* Test the inte interfaces. */
void inet_init(test_stat_t *stat);
void inet_run(void);

#endif /* __vote_h__ */
