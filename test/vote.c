// SPDX-License-Identifier: GPL-2.0

/*
 * Coverage of the VAN operating system tests.
 *
 * Copyright (C) 2021 Gerald Schueller <gerald.schueller@web.de>
 */
/*============================================================================
  IMPORTED INCLUDE REFERENCES
  ============================================================================*/
#include <stdlib.h>  /* Standard lib: exit(). */
#include "os.h"      /* Operating system: os_sem_create(). */

/*============================================================================
  EXPORTED INCLUDE REFERENCES
  ============================================================================*/
/*============================================================================
  LOCAL NAME CONSTANTS DEFINITIONS
  ============================================================================*/
/* Size of the message buffer. */
#define TEST_LEN  32

/*============================================================================
  MACROS
  ============================================================================*/

/* Test two values for likeness. */
#define TEST_ASSERT_EQ(arg1_, arg2_) \
do { \
	if ((arg1_) != (arg2_)) { \
		printf("%s: MISMATCH at %s,%d: %d == %d\n", test_stat.label, \
		       __FILE__, __LINE__, (arg1_), (arg2_)); \
		test_stat.error_n++; \
	} \
} while (0)

/* Extend the test sest. */
#define TEST_ADD(label_) #label_, label_

/*============================================================================
  LOCAL TYPE DEFINITIONS
  ============================================================================*/

/**
 * test_stat_t - state of vote.
 *
 * label:      identifikation of the current test case.
 * @test_n:    number of the executed test cases.
 * @error_n:   number of the test case errors.
 * @suspend:   control semaphore for the main process.
 * @msg_info:  contents of the message buffer.
 * @thread:    thread list for the multi thread test.
 **/
typedef struct {
	char  *label;
	int    test_n;
	int    error_n;
	sem_t  suspend;
	char  *msg_info;
	void  *thread[OS_THREAD_LIMIT];

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

/**
 * test_msg_t - test message.
 *
 * @OS_QUEUE_MSG_HEAD:  generic message header.
 * @buf:                message information.
 **/
typedef struct {
        OS_QUEUE_MSG_HEAD;
        char buf[TEST_LEN];
} test_msg_t;

/**
 * test_mt_msg_t - message for the mult thread test.
 *
 * @OS_QUEUE_MSG_HEAD:  generic message header.
 * @buf:                message information.
 * @his_idx             index of the sender thread.
 * @my_idx              index of the receiver thread.
 **/
typedef struct {
        OS_QUEUE_MSG_HEAD;
        char buf[TEST_LEN];
	int  his_idx;
	int  my_idx;
} test_mt_msg_t;

/*============================================================================
  LOCAL FUNCTION PROTOTYPES
  ============================================================================*/

static void test_mt_msg_send(int his_idx, int my_idx);

static int test_case_shutdown(void);
static int test_case_multi_thread(void);
static int test_case_queue_limit(void);
static int test_case_malloc_limit(void);
static int test_case_queue(void);
static int test_case_thread_limit(void);
static int test_case_thread(void);
static int test_case_string(void);
static int test_case_mem(void);
static int test_case_sync(void);
static int test_case_trap(void);
static int test_case_boot(void);

/*============================================================================
  LOCAL DATA
  ============================================================================*/

/* State of the vote program. */
static test_stat_t test_stat;

/* List of the boot test cases. */
static test_elem_t test_system[] = {
	{ TEST_ADD(test_case_boot), 0 },
	{ TEST_ADD(test_case_trap), 0 },
	{ TEST_ADD(test_case_sync), 0 },
	{ TEST_ADD(test_case_mem), 0 },
	{ TEST_ADD(test_case_string), 0 },
	{ TEST_ADD(test_case_thread), 0 },
	{ TEST_ADD(test_case_thread_limit), 0 },
	{ TEST_ADD(test_case_queue), 0 },
	{ TEST_ADD(test_case_malloc_limit), 0 },
	{ TEST_ADD(test_case_queue_limit), 0 },
	{ TEST_ADD(test_case_multi_thread), 0 },
	{ TEST_ADD(test_case_shutdown), 0 },
	{ NULL, NULL, 0 }
};

/*============================================================================
  LOCAL FUNCTIONS
  ============================================================================*/

/**
 * test_os_stat() - compare the current with expected OS state.
 *
 * @expected:  expected OS state.
 *
 * Return:	return 0, if the states match.
 **/
static int test_os_stat(os_statistics_t *expected)
{
	os_statistics_t stat;
	int ret;
	
	/* Get the state of the OS. */
	os_memset(&stat, 0, sizeof(stat));
	os_statistics(&stat);

	/* Compare the current with expected OS state. */
	ret = os_memcmp(&stat, expected, sizeof(os_statistics_t));

	return ret;
}

/**
 * test_mt_msg_exec() - process the multi thread message in the test thread context.
 *
 * @m:  pointer to the received message.
 *
 * Return:	None.
 **/
static void test_mt_msg_exec(os_queue_elem_t *m)
{
        test_mt_msg_t *msg;
	void *my_p, *his_p;
	char *my_name, *his_name;
	int my_idx, his_idx, ret;
	
	/* Decode the test message. */
        msg = (test_mt_msg_t *) m;

	/* Copy the thread index verify it. */
	my_idx  = msg->my_idx;
	his_idx = msg->his_idx;
	OS_TRAP_IF(my_idx < 0 || my_idx >= OS_THREAD_LIMIT ||
		   his_idx < 0 || his_idx >= OS_THREAD_LIMIT);
	
	/* Test the message buffer. */
	ret = os_strcmp(msg->buf, test_stat.msg_info);
	OS_TRAP_IF(ret != 0);

	/* Map the index to the thread address. */
	my_p  = test_stat.thread[my_idx];
	his_p = test_stat.thread[his_idx];

	/* Request the thread name. */
	my_name  = os_thread_name(my_p);
	his_name = os_thread_name(his_p);

	printf("[%d, %s]: received mt from [%d, %s]\n",
	       my_idx, my_name, his_idx, his_name);
	
	/* Test the index and resume the main process. */
	if ((my_idx + 1) >= OS_THREAD_LIMIT) {
		os_sem_release(&test_stat.suspend);
		return;
	}

	/* Send the message to the next thread. */
	test_mt_msg_send(my_idx, my_idx + 1);
}

/**
 * test_mt_msg_send() - send the multi thread message to the next thread.
 *
 * @his_idx:  index of the sender thread.
 * @my_idx:   index of the receiver thread.
 *
 * Return:	None.
 **/
static void test_mt_msg_send(int his_idx, int my_idx)
{
        test_mt_msg_t msg;
	void *p;

	/* Entry condition. */
	OS_TRAP_IF(my_idx < 0 || my_idx >= OS_THREAD_LIMIT ||
		   his_idx < 0 || his_idx >= OS_THREAD_LIMIT);

	/* Map the index to the thread address. */
	p = test_stat.thread[my_idx];
	
	/* Define the test message. */
	os_memset(&msg, 0, sizeof(msg));
	msg.param   = p;
	msg.cb      = test_mt_msg_exec;
	msg.his_idx = his_idx;
	msg.my_idx  = my_idx;
	os_strcpy(msg.buf, TEST_LEN, test_stat.msg_info);

	/* Send the message to the test thread. */
	OS_SEND(p, &msg, sizeof(msg));
}

/**
 * test_msg_exec() - process the message in the test thread context.
 *
 * @m:  pointer to the received message.
 *
 * Return:	None.
 **/
static void test_msg_exec(os_queue_elem_t *m)
{
        test_msg_t *msg;
	int ret;
	
	/* Decode the test message. */
        msg = (test_msg_t *) m;

	/* Test the message buffer. */
	ret = os_strcmp(msg->buf, test_stat.msg_info);
	OS_TRAP_IF(ret != 0);

	/* Resume the main process. */
	os_sem_release(&test_stat.suspend);
}

/**
 * test_msg_send() - send the message to the test thread.
 *
 * @server:  address of test thread.
 *
 * Return:	None.
 **/
static void test_msg_send(void *thread)
{
        test_msg_t msg;

	/* Define the test message. */
	os_memset(&msg, 0, sizeof(msg));
	msg.param = thread;
	msg.cb    = test_msg_exec;
	os_strcpy(msg.buf, TEST_LEN, test_stat.msg_info);

	/* Send the message to the test thread. */
	OS_SEND(thread, &msg, sizeof(msg));
}

/**
 * test_case_shutdown() - switch off the VAN OS.
 *
 * Return:	the execution state..
 **/
static int test_case_shutdown(void)
{
	os_statistics_t expected = { 2, 2, 0, 2311, 2311, 0 };
	int stat;
	
	/* Verify the OS state. */
	stat = test_os_stat(&expected);
	
	/* Release all OS resources. */
	os_exit();
	
	return stat;
}

/**
 * test_case_multi_thread() - send a message from one thread to the other.
 *
 * Return:	the execution state..
 **/
static int test_case_multi_thread(void)
{
	os_statistics_t expected = { 2, 2, 0, 2311, 2311, 0 };
	char name[OS_MAX_NAME_LEN];
	void **p;
	int i, stat;

	/* Define the message information. */
	test_stat.msg_info = "ping";

	/* Get the start adderss of the global thread list. */
	p = test_stat.thread;
	
	/* Create the control semaphore for the main process. */
	os_sem_init(&test_stat.suspend, 0);

	/* Install all available threads. */
	for (i = 0; i < OS_THREAD_LIMIT; i++) {
		/* Build the thread name. */
		snprintf(name, OS_MAX_NAME_LEN, "test_%d", i + 1);
		
		/* Create and start the test thread. */
		p[i] = os_thread_create(name, OS_THREAD_PRIO_FOREG, 256);
	}

	/* Send the first multi thread message. */
	test_mt_msg_send(0, 0);
	
	/* Suspend the main process. */
	os_sem_wait(&test_stat.suspend);

	/* Kill all installed threads. */
	for (i = 0; i < OS_THREAD_LIMIT; i++) {
		/* Kill the test thread. */
		os_thread_destroy(p[i]);
	}

	/* Release the control semaphore for the main process. */
	os_sem_delete(&test_stat.suspend);

	/* Reset the global thread list. */
	os_memset(p, 0, sizeof(void *) * OS_THREAD_LIMIT);
	
	/* Verify the OS state. */
	stat = test_os_stat(&expected);
	
	return stat;
}

/**
 * test_case_queue_limit() - fill the thread input queue up to the limit.
 *
 * Return:	the execution state..
 **/
static int test_case_queue_limit(void)
{
	os_statistics_t expected = { 2, 2, 0, 2306, 2306, 0 };
	void *p;
	int i, stat;

	/* Define the message information. */
	test_stat.msg_info = "ping";
	
	/* Create the control semaphore for the main process. */
	os_sem_init(&test_stat.suspend, 0);

	/* Create and start the test thread. */
	p = os_thread_create("test", OS_THREAD_PRIO_FOREG, 256);

	/* Send the messagae to the test thread and verify the cleanup of the
	 *  message queue in os_exit. */
	for (i = 0; i < 256; i++)
		test_msg_send(p);

	/* Suspend the main process. */
	os_sem_wait(&test_stat.suspend);

	/* Kill the test thread. */
	os_thread_destroy(p);

	/* Release the control semaphore for the main process. */
	os_sem_delete(&test_stat.suspend);

	/* Verify the OS state. */
	stat = test_os_stat(&expected);
	
	return stat;
}

/**
 * test_case_malloc_limit() - loop over malloc and free.
 *
 * Return:	the execution state..
 **/
static int test_case_malloc_limit(void)
{
	os_statistics_t expected = { 2, 2, 0, 2050, 2050, 0 };
	int i, stat;
	void *p;

	/* malloc and free test */
	for (i = 0; i < (2 * OS_MALLOC_LIMIT); i++) {
		p = OS_MALLOC(1);
		OS_FREE(p);
		p = OS_MALLOC(1);
		OS_FREE(p);
	}

	/* Verify the OS state. */
	stat = test_os_stat(&expected);
	
	return stat;
}

/**
 * test_case_queue() - test the thread input queue.
 *
 * Return:	the execution state..
 **/
static int test_case_queue(void)
{
	os_statistics_t expected = { 2, 2, 0, 2, 2, 0 };
	void *p;
	int stat;

	/* Define the message information. */
	test_stat.msg_info = "ping";
	
	/* Create the control semaphore for the main process. */
	os_sem_init(&test_stat.suspend, 0);

	/* Create and start the test thread. */
	p = os_thread_create("test", OS_THREAD_PRIO_FOREG, 256);

	/* Start the interworking with the test thread. */
	test_msg_send(p);

	/* Suspend the main process. */
	os_sem_wait(&test_stat.suspend);

	/* Kill the test thread. */
	os_thread_destroy(p);

	/* Release the control semaphore for the main process. */
	os_sem_delete(&test_stat.suspend);

	/* Verify the OS state. */
	stat = test_os_stat(&expected);
	
	return stat;
}

/**
 * test_case_thread_limit() - install and kill all availabe threads.
 *
 * Return:	the execution state..
 **/
static int test_case_thread_limit(void)
{
	os_statistics_t expected = { 2, 2, 0, 1, 1, 0 };
	char name[OS_MAX_NAME_LEN];
	void *p[OS_THREAD_LIMIT];
	int i, j, stat;

	j = OS_THREAD_LIMIT;
	
	/* Install all available threads. */
	for (i = 0; i < j; i++) {
		/* Build the thread name. */
		snprintf(name, OS_MAX_NAME_LEN, "test_%d", i + 1);
		
		/* Create and start the test thread. */
		p[i] = os_thread_create(name, OS_THREAD_PRIO_FOREG, 256);
	}

	/* Kill all installed threads. */
	for (j = j - 1; j >= 0; j--) {
		/* Kill the test thread. */
		os_thread_destroy(p[j]);
	}
	
	/* Verify the OS state. */
	stat = test_os_stat(&expected);
	
	return stat;
}

/**
 * test_case_thread() - test the thread handling.
 *
 * Return:	the execution state..
 **/
static int test_case_thread(void)
{
	os_statistics_t expected = { 2, 2, 0, 1, 1, 0 };
	void *p;
	char *name;
	int stat;

	/* Create and start the test thread. */
	p = os_thread_create("test", OS_THREAD_PRIO_FOREG, 256);

	/* Get the name of the thread. */
	name = os_thread_name(p);
	printf("Thread name: %s\n", name);

	/* Kill the test thread. */
	os_thread_destroy(p);

	/* Test a thread with an invalid priority. */
	p = os_thread_create("test", 11, 256);
	os_thread_destroy(p);

	/* Verify the OS state. */
	stat = test_os_stat(&expected);
	
	return stat;
}

/**
 * test_case_string() - string and mem test like strcmp or memcmp.
 *
 * Return:	the execution state..
 **/
static int test_case_string(void)
{
	os_statistics_t expected = { 2, 2, 0, 1, 1, 0 };
	char    s1[TEST_LEN], *s2 = "*coverage*";
	int n, stat;

	/* Fill memory with a contant byte. */
	n = os_strlen(s2);
	os_memset(s1, 0, n);

	/* Copy memory area. */
	os_memcpy(s1, TEST_LEN, s2, n);

	/* Compare memory areas. */
	stat = os_memcmp(s1, s2, n);
	TEST_ASSERT_EQ(0, stat);

	/* Determine the length of a fixed-size string. */
	stat = os_strnlen(s1, TEST_LEN);
	TEST_ASSERT_EQ(n, stat);

	/* Copy a string. */
	os_memset(s1, 0, n);
	os_strcpy(s1, TEST_LEN, s2);

	/* Compare two strings. */
	stat = os_strcmp(s1, s2);
	TEST_ASSERT_EQ(0, stat);
	
	/* Verify the OS state. */
	stat = test_os_stat(&expected);
	
	return stat;
}

/**
 * test_case_mem() - os_malloc and os_free test.
 *
 * Return:	the execution state..
 **/
static int test_case_mem(void)
{
	os_statistics_t expected = { 2, 2, 0, 1, 1, 0 };
	void *p;
	int stat;

	/* Dynamic memory. */
	p = OS_MALLOC(1);
	OS_FREE(p);
	
	/* Verify the OS state. */
	stat = test_os_stat(&expected);
	
	return stat;
}

/**
 * test_case_sync() - test the thread synchronization procedures.
 *
 * Return:	the execution state..
 **/
static int test_case_sync(void)
{	
	os_statistics_t expected = { 2, 2, 0, 0, 0, 0 };
	sem_t           sem;
	pthread_mutex_t mutex;
	spinlock_t      spinlock;
	int stat;

	/* Semaphores. */
	os_sem_init(&sem, 1);
	os_sem_wait(&sem);
	os_sem_release(&sem);
	os_sem_delete(&sem);

	/* Critical section. */
	os_cs_init(&mutex);
	os_cs_enter(&mutex);
	os_cs_leave(&mutex);
	os_cs_destroy(&mutex);

	/* Interrupts. */
	os_spin_init(&spinlock);
	os_spin_lock(&spinlock);
	os_spin_unlock(&spinlock);
	os_spin_destroy(&spinlock);
	
	/* Verify the OS state. */
	stat = test_os_stat(&expected);
	
	return stat;
}

/**
 * test_case_trap() - test the trap handling.
 *
 * Return:	the execution state..
 **/
static int test_case_trap(void)
{
	os_statistics_t expected = { 2, 2, 0, 0, 0, 0 };
	int stat;

	/* Use the test agreement. */
	os_trap(__FILE__, "*coverage*", __LINE__);

	/* Verify the OS state. */
	stat = test_os_stat(&expected);
	
	return stat;
}

/**
 * test_case_boot() - test the boot phase of the VAN system.
 *
 * Return:	the execution state..
 **/
static int test_case_boot(void)
{
	os_statistics_t expected = { 2, 2, 0, 0, 0, 0 };
	int stat;
	
	/* Initialize all OS layers. */
	os_init();

	/* Switch off the OS trace. */
	os_trace_button(0);

	/* Verify the OS state. */
	stat = test_os_stat(&expected);
	
	return stat;
}

/**
 * test_set_process() - execute the list of the test cases.
 *
 * @label: name of the test set.
 * @p:     start of the test case list.
 *
 * Return:	None.
 **/
static void test_set_process (char *label, test_elem_t *elem)
{
	test_stat_t *s;
	int ret;

	printf("%s: SELECT\n", label);

	/* Get the address of the vote state. */
	s = &test_stat;
	
	/* Run thru the list of the test cases. */
	for (; elem->routine; elem++) {
		/* Save the label of the current test and update the test counter. */
		s->label = elem->label;
		s->test_n++;

		printf("%s: CALL [%d, %s, %d]\n", label, s->test_n, elem->label,
		       elem->place);

		/* Execute the test case. */
		ret = elem->routine();
		
		/* Verify the test status. */
		TEST_ASSERT_EQ(elem->expected, ret);
		
		/* Evalute the test result. */
		if (ret == elem->expected)
			continue;

		printf("%s: FAILURE in %s: expected %d, but gotten %d\n",
		       label, s->label,  elem->expected, ret);
        }
	
	printf("%s: DONE\n", label);
}

/**
 * test_run() - execute the specific list of test cases.
 *
 * Return:	None.
 **/
static void test_run(void)
{
	/* Execute the list of the test cases. */
	test_set_process(TEST_ADD(test_system));
}

static void test_usage(void)
{
	printf("VOTE - VAN OS Test Environment \n");
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
	
	/* Initialize the OS. */
	test_case_boot();

	/* Switch on the OS trace. */
	os_trace_button(1);

	/* Execute the test case. */
	printf("CALL [%d, %s, %d]\n", n, elem->label, elem->place);
	elem->routine();

	/* Switch off the OS. */
	test_case_shutdown();
}

/*============================================================================
  EXPORTED FUNCTIONS
  ============================================================================*/

/**
 * main() - start function of the VAN code coverage.
 *
 * Return:	0 or force a software trap.
 **/
int main(int argc, char* argv[])
{
	test_stat_t *s;
	int n;

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
	printf("\nTEST: %d failures in %d test cases.\n", s->error_n, s->test_n);

	return (0);
}
