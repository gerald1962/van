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
#include "os.h"    /* Operating system: os_sem_create(). */
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

static int test_case_shutdown(void);
static int test_case_boot(void);

/*============================================================================
  LOCAL DATA
  ============================================================================*/

/* State of the vote program. */
static test_stat_t test_stat;

/* Local pointer for TEST_ASSERT_EQ. */
static test_stat_t *vote_p;

/* List of the boot test cases. */
static test_elem_t boot_system[] = {
	{ TEST_ADD(test_case_boot), 0 },
	{ NULL, NULL, 0 }
};

/* List of the shutdown test cases. */
static test_elem_t shutdown_system[] = {
	{ TEST_ADD(test_case_shutdown), 0 },
	{ NULL, NULL, 0 }
};

/*============================================================================
  LOCAL FUNCTIONS
  ============================================================================*/
/**
 * test_case_shutdown() - switch off the VAN OS.
 *
 * Return:	the execution state..
 **/
static int test_case_shutdown(void)
{
	os_statistics_t expected = { 6, 4, 0, 2350, 2350, 0 };
	int stat;
	
	/* Verify the OS state. */
	stat = test_os_stat(&expected);
	
	/* Release all OS resources. */
	os_exit();
	
	return stat;
}

/**
 * test_case_boot() - test the boot phase of the VAN system.
 *
 * Return:	the execution state..
 **/
static int test_case_boot(void)
{
	os_statistics_t expected = { 6, 4, 0, 0, 0, 0 };
	int stat;
	
	/* Initialize all OS layers. */
	os_init(1);

	/* Switch off the OS trace. */
	os_trace_button(0);

	/* Verify the OS state. */
	stat = test_os_stat(&expected);
	
	return stat;
}

/**
 * vote_run() - run through all test system.
 *
 * Return:	None.
 **/
static void vote_run(void)
{
	/* Execute the list of the test cases. */
	test_set_process(TEST_ADD(boot_system));

	/* Basic van OS interfaces under test. */
	but_run();
	
	/* Test the controller interworking with the battery endpoind. */
	cob_run();
	
	/* Test the controller-battery-display cabling. */
	tri_run();

	/* Test the van OS clock system. */
	clk_run();

	test_set_process(TEST_ADD(shutdown_system));
}

/*============================================================================
  EXPORTED FUNCTIONS
  ============================================================================*/
/**
 * test_os_stat() - compare the current with expected OS state.
 *
 * @expected:  expected OS state.
 *
 * Return:	return 0, if the states match.
 **/
int test_os_stat(os_statistics_t *expected)
{
	os_statistics_t stat;
	int ret;
	
	/* Get the state of the OS. */
	os_memset(&stat, 0, sizeof(stat));
	os_statistics(&stat);

	/* Compare the current with expected OS state. */
	ret = os_memcmp(&stat, expected, sizeof(os_statistics_t));

	/* Test the result. */
	if (ret != 0) {
		printf("vote: expected: [cs=%d, sem=%d, spin=%d, malloc=%d, free=%d, thread=%d]\n",
		       expected->cs_count,
		       expected->sem_count,
		       expected->spin_count,
		       expected->malloc_c,
		       expected->free_c,
		       expected->thread_c);
		printf("vote: gotten:   [cs=%d, sem=%d, spin=%d, malloc=%d, free=%d, thread=%d]\n",
		       stat.cs_count,
		       stat.sem_count,
		       stat.spin_count,
		       stat.malloc_c,
		       stat.free_c,
		       stat.thread_c);
	}
	return ret;
}

/**
 * test_set_process() - execute the list of the test cases.
 *
 * @label: name of the test set.
 * @p:     start of the test case list.
 *
 * Return:	None.
 **/
void test_set_process(char *label, test_elem_t *elem)
{
	test_stat_t *s;
	int ret;

	printf("%s %s: SELECT\n", P, label);

	/* Get the address of the vote state. */
	s = &test_stat;
	
	/* Run thru the list of the test cases. */
	for (; elem->routine; elem++) {
		/* Save the label of the current test and update the test counter. */
		s->label = elem->label;
		s->test_n++;

		printf("%s %s: CALL [%d, %s, %d]\n", P, label, s->test_n,
		       elem->label, elem->place);

		/* Execute the test case. */
		ret = elem->routine();
		
		/* Verify the test status. */
		TEST_ASSERT_EQ(elem->expected, ret);
		
		/* Evalute the test result. */
		if (ret == elem->expected)
			continue;

		printf("%s %s: FAILURE in %s: expected %d, but gotten %d\n",
		       P, label, s->label,  elem->expected, ret);
        }
	
	printf("%s %s: DONE\n", P, label);
}

/**
 * main() - start function of the VAN code coverage.
 *
 * Return:	0 or force a software trap.
 **/
int main(void)
{
	test_stat_t *s;

	/* Initialize the local pointer for TEST_ASSERT_EQ */
	vote_p = &test_stat;
	
	/* Initialize the basic test system. */
	but_init(&test_stat);
	
	/* Initialize controller-battery test system. */
	cob_init(&test_stat);

	/* Initialize the controller-battery-display cabling. */
	tri_init(&test_stat);

	/* Initialize the van OS clock system. */
	clk_init(&test_stat);

	/* Run through all test system. */
	vote_run();
	
	/* Display the test status. */
	s = &test_stat;
	printf("\n%s TEST: %d failures in %d test cases.\n",
	       P, s->error_n, s->test_n);

	return (0);
}
