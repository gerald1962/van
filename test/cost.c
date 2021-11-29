// SPDX-License-Identifier: GPL-2.0

/*
 * Coverage of the VAN operating system tests.
 *
 * Copyright (C) 2021 Gerald Schueller <gerald.schueller@web.de>
 */
/*============================================================================
  IMPORTED INCLUDE REFERENCES
  ============================================================================*/
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
 * test_stat_t - state of cost.
 *
 * label:      Identifikation of the current test case.
 * @test_n:    Number of the executed test cases.
 * @error_n:   Number of the test case errors.
 * @suspend:   Control semaphore for the main process.
 * @msg_info:  Contents of the message buffer.
 **/
typedef struct {
	char  *label;
	int    test_n;
	int    error_n;
	sem_t  suspend;
	char  *msg_info;

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

/*============================================================================
  LOCAL FUNCTION PROTOTYPES
  ============================================================================*/

static int test_case_shutdown(void);
static int test_case_queue_limit(void);
static int test_case_malloc_limit(void);
static int test_case_queue(void);
static int test_case_thread(void);
static int test_case_string(void);
static int test_case_mem(void);
static int test_case_sync(void);
static int test_case_trap(void);
static int test_case_boot(void);

/*============================================================================
  LOCAL DATA
  ============================================================================*/

/* State of the cost program. */
static test_stat_t test_stat;

/* List of the boot test cases. */
static test_elem_t test_system[] = {
	{ TEST_ADD(test_case_boot), 0 },
	{ TEST_ADD(test_case_trap), 0 },
	{ TEST_ADD(test_case_sync), 0 },
	{ TEST_ADD(test_case_mem), 0 },
	{ TEST_ADD(test_case_string), 0 },
	{ TEST_ADD(test_case_thread), 0 },
	{ TEST_ADD(test_case_queue), 0 },
	{ TEST_ADD(test_case_malloc_limit), 0 },
	{ TEST_ADD(test_case_queue_limit), 0 },
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
	os_statistics_t expected = { 2, 2, 0, 2306, 2306, 0 };
	int stat;
	
	/* Verify the OS state. */
	stat = test_os_stat(&expected);
	
	/* Release all OS resources. */
	os_exit();
	
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
 * test_case_thread() - test the thread handling.
 *
 * Return:	the execution state..
 **/
static int test_case_thread(void)
{
	os_statistics_t expected = { 2, 2, 0, 1, 1, 0 };
	void *p;
	int stat;

	/* Create and start the test thread. */
	p = os_thread_create("test", OS_THREAD_PRIO_FOREG, 256);

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

	/* Backup of the VAN OS status. */
	//  TS_ENTER();
	
	/* Get the address of the cost state. */
	s = &test_stat;
	
	/* Run thru the list of the test cases. */
	for (; elem->routine; elem++) {
		/* Save the label of the current test and update the test counter. */
		s->label = elem->label;
		s->test_n++;

		printf("%s: CALL [%s, %d]\n", label, elem->label, elem->place);

		/* Execute the test case. */
		ret = elem->routine();
		
		// check_status();

		TEST_ASSERT_EQ(elem->expected, ret);
		
		/* Evalute the test result. */
		if (ret == elem->expected)
			continue;

		printf("%s: FAILURE in %s: expected %d, but gotten %d\n",
		       label, s->label,  elem->expected, ret);
        }
	
	/* Match the backup of the VAN OS status with the current status. */
	//  TS_LEAVE();
	
	printf("%s: DONE\n", label);
}

/**
 * test_run() - execute the list of the test cases.
 *
 * Return:	None.
 **/
static void test_run(void)
{
	/* Execute the list of the test cases. */
	test_set_process(TEST_ADD(test_system));
}

/*============================================================================
  EXPORTED FUNCTIONS
  ============================================================================*/

/**
 * main() - start function of the VAN code coverage.
 *
 * Return:	0 or force a software trap.
 **/
int main(void)
{
	test_stat_t *s;

	/* Get the address of the cost status. */
	s = &test_stat;
	
	/* Execute all test cases. */
	test_run();

	printf("\nTEST: %d failures in %d test cases.\n", s->error_n, s->test_n);

	return (0);
}
