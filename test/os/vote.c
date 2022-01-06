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
	os_statistics_t expected = { 3, 3, 0, 2330, 2330, 0 };
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
	os_statistics_t expected = { 3, 3, 0, 0, 0, 0 };
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
 * test_run() - execute the specific list of test cases.
 *
 * Return:	None.
 **/
static void test_run(void)
{
	/* Execute the list of the test cases. */
	test_set_process(TEST_ADD(boot_system));

	/* Basic van OS interfaces under test. */
	but_run();
	
	/* Test the controller interworking with the battery endpoind. */
	cob_run();
	
	test_set_process(TEST_ADD(shutdown_system));
}

/**
 * test_usage() - provide information aboute the vote configuration.
 *
 * Return:	None.
 **/
static void test_usage(void)
{
	printf("VOTE - VAN OS Test Environment\n");
	printf("  no arg - execute the complete test set with n cases\n");
	printf("  n      - start the nth test case, except n<=1 or n>=limit \n");
	printf("  other  - print the usage information.\n");
	exit (1);
}

/**
 * test_single_case() - execute a single test case.
 *
 * Return:	None.
 **/
static void test_single_case(int n)
{
	/* XXX */
#if 0
	test_elem_t *elem;
	int i;

	/* Test the limits of the case list: boot process shall be done once. */
	if (n < 1)
		test_usage();
		
	/* Get the address of the first test case. */
	elem = test_system;

	/* Count the number of the  test cases. */
	for (i = 0; elem->routine; i++, elem++) 
		;

	/* Test the search result. */
	if (n <= 1 || n >= i) {
		printf("vote: invalid test case number %d.\n", n);
		test_usage();
	}

	/* Get the address of the first test case. */
	elem = test_system;

	/* Search for the test case. */
	for (i = 1; i < n && elem->routine; i++, elem++) 
		;

	/* Initialize the test counter. */
	test_stat.test_n = 1;
	
	printf("%s SELECT\n", P);

	/* Initialize the OS. */
	test_case_boot();

	/* Switch on the OS trace. */
	os_trace_button(1);

	/* Execute the test case. */
	printf("%s CALL [%d, %s, %d]\n", P, n, elem->label, elem->place);
	elem->routine();

	/* Switch off the OS. */
	test_case_shutdown();
	
	printf("%s DONE\n", P);
#endif
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
int main(int argc, char *argv[])
{
	test_stat_t *s;
	int n;

	/* Initialize the local pointer for TEST_ASSERT_EQ */
	vote_p = &test_stat;
	
	/* Initialize the basic test system. */
	but_init(&test_stat);
	
	/* Initialize controller-battery test system. */
	cob_init(&test_stat);
	
	/* Test the argument counter. */
	switch(argc) {
	case 1:
		/* Execute all test cases. */
		test_run();
		break;
	case 2:
		/* Decode the test number and execute the nth test case. */
		n = atoi(argv[1]);
		test_single_case(n);
		break;
	default:
		test_usage();
		break;
	}
	
	/* Display the test status. */
	s = &test_stat;
	printf("\n%s TEST: %d failures in %d test cases.\n",
	       P, s->error_n, s->test_n);

	return (0);
}
