// SPDX-License-Identifier: GPL-2.0

/*
 * Baic van OS interfaces under test.
 *
 * Copyright (C) 2021 Gerald Schueller <gerald.schueller@web.de>
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
/* Size of the message buffer. */
#define BUT_LEN  32

/*============================================================================
  MACROS
  ============================================================================*/
/*============================================================================
  LOCAL TYPE DEFINITIONS
  ============================================================================*/
/**
 * but_stat_t - state of but system.
 *
 * @msg_info:  contents of the message buffer.
 * @thread:    thread list for the multi thread test.
 * @suspend:   control semaphore for the main process.
 **/
typedef struct {
	char  *msg_info;
	void  *thread[OS_THREAD_LIMIT];
	sem_t  suspend;
} but_stat_t;

/**
 * but_msg_t - test message.
 *
 * @OS_QUEUE_MSG_HEAD:  generic message header.
 * @buf:                message information.
 **/
typedef struct {
        OS_QUEUE_MSG_HEAD;
        char buf[BUT_LEN];
} but_msg_t;

/**
 * but_mt_msg_t - message for the mult thread test.
 *
 * @OS_QUEUE_MSG_HEAD:  generic message header.
 * @buf:                message information.
 * @his_idx             index of the sender thread.
 * @my_idx              index of the receiver thread.
 **/
typedef struct {
        OS_QUEUE_MSG_HEAD;
        char buf[BUT_LEN];
	int  his_idx;
	int  my_idx;
} but_mt_msg_t;

/*============================================================================
  LOCAL FUNCTION PROTOTYPES
  ============================================================================*/
static void but_mt_msg_send(int his_idx, int my_idx);

static int but_multi_thread(void);
static int but_queue_limit(void);
static int but_malloc_limit(void);
static int but_queue(void);
static int but_thread_limit(void);
static int but_thread(void);
static int but_string(void);
static int but_mem(void);
static int but_sync(void);
static int but_trap(void);

/*============================================================================
  LOCAL DATA
  ============================================================================*/
/* Define vot_p for TEST_ASSERT_EQ. */
static test_stat_t *vote_p;

/* State of the but system. */
static but_stat_t but_stat;

/* List of the of basic van OS test cases. */
static test_elem_t but_system[] = {
	{ TEST_ADD(but_trap), 0 },
	{ TEST_ADD(but_sync), 0 },
	{ TEST_ADD(but_mem), 0 },
	{ TEST_ADD(but_string), 0 },
	{ TEST_ADD(but_thread), 0 },
	{ TEST_ADD(but_thread_limit), 0 },
	{ TEST_ADD(but_queue), 0 },
	{ TEST_ADD(but_malloc_limit), 0 },
	{ TEST_ADD(but_queue_limit), 0 },
	{ TEST_ADD(but_multi_thread), 0 },
	{ NULL, NULL, 0 }
};

/*============================================================================
  LOCAL FUNCTIONS
  ============================================================================*/
/**
 * but_mt_msg_exec() - process the multi thread message in the test thread context.
 *
 * @m:  pointer to the received message.
 *
 * Return:	None.
 **/
static void but_mt_msg_exec(os_queue_elem_t *m)
{
        but_mt_msg_t *msg;
	void *my_p, *his_p;
	char *my_name, *his_name;
	int my_idx, his_idx, ret;
	
	/* Decode the test message. */
        msg = (but_mt_msg_t *) m;

	/* Copy the thread index verify it. */
	my_idx  = msg->my_idx;
	his_idx = msg->his_idx;
	OS_TRAP_IF(my_idx < 0 || my_idx >= OS_THREAD_LIMIT ||
		   his_idx < 0 || his_idx >= OS_THREAD_LIMIT);
	
	/* Test the message buffer. */
	ret = os_strcmp(msg->buf, but_stat.msg_info);
	OS_TRAP_IF(ret != 0);

	/* Map the index to the thread address. */
	my_p  = but_stat.thread[my_idx];
	his_p = but_stat.thread[his_idx];

	/* Request the thread name. */
	my_name  = os_thread_name(my_p);
	his_name = os_thread_name(his_p);

	printf("%s [%d, %s]: received mt from [%d, %s]\n", P,
	       my_idx, my_name, his_idx, his_name);
	
	/* Test the index and resume the main process. */
	if ((my_idx + 1) >= OS_THREAD_LIMIT) {
		os_sem_release(&but_stat.suspend);
		return;
	}

	/* Send the message to the next thread. */
	but_mt_msg_send(my_idx, my_idx + 1);
}

/**
 * but_mt_msg_send() - send the multi thread message to the next thread.
 *
 * @his_idx:  index of the sender thread.
 * @my_idx:   index of the receiver thread.
 *
 * Return:	None.
 **/
static void but_mt_msg_send(int his_idx, int my_idx)
{
        but_mt_msg_t msg;
	void *p;

	/* Entry condition. */
	OS_TRAP_IF(my_idx < 0 || my_idx >= OS_THREAD_LIMIT ||
		   his_idx < 0 || his_idx >= OS_THREAD_LIMIT);

	/* Map the index to the thread address. */
	p = but_stat.thread[my_idx];
	
	/* Define the test message. */
	os_memset(&msg, 0, sizeof(msg));
	msg.param   = p;
	msg.cb      = but_mt_msg_exec;
	msg.his_idx = his_idx;
	msg.my_idx  = my_idx;
	os_strcpy(msg.buf, BUT_LEN, but_stat.msg_info);

	/* Send the message to the test thread. */
	OS_SEND(p, &msg, sizeof(msg));
}

/**
 * but_msg_exec() - process the message in the test thread context.
 *
 * @m:  pointer to the received message.
 *
 * Return:	None.
 **/
static void but_msg_exec(os_queue_elem_t *m)
{
        but_msg_t *msg;
	int ret;
	
	/* Decode the test message. */
        msg = (but_msg_t *) m;

	/* Test the message buffer. */
	ret = os_strcmp(msg->buf, but_stat.msg_info);
	OS_TRAP_IF(ret != 0);

	/* Resume the main process. */
	os_sem_release(&but_stat.suspend);
}

/**
 * but_msg_send() - send the message to the test thread.
 *
 * @server:  address of test thread.
 *
 * Return:	None.
 **/
static void but_msg_send(void *thread)
{
        but_msg_t msg;

	/* Define the test message. */
	os_memset(&msg, 0, sizeof(msg));
	msg.param = thread;
	msg.cb    = but_msg_exec;
	os_strcpy(msg.buf, BUT_LEN, but_stat.msg_info);

	/* Send the message to the test thread. */
	OS_SEND(thread, &msg, sizeof(msg));
}

/**
 * but_multi_thread() - send a message from one thread to the other.
 *
 * Return:	the execution state..
 **/
static int but_multi_thread(void)
{
	os_statistics_t expected = { 4, 4, 0, 2322, 2322, 0 };
	char name[OS_MAX_NAME_LEN];
	void **p;
	int i, stat;

	/* Define the message information. */
	but_stat.msg_info = "ping";

	/* Get the start adderss of the global thread list. */
	p = but_stat.thread;
	
	/* Create the control semaphore for the main process. */
	os_sem_init(&but_stat.suspend, 0);

	/* Install all available threads. */
	for (i = 0; i < OS_THREAD_LIMIT; i++) {
		/* Build the thread name. */
		snprintf(name, OS_MAX_NAME_LEN, "test_%d", i + 1);
		
		/* Create and start the test thread. */
		p[i] = os_thread_create(name, OS_THREAD_PRIO_FOREG, 256);
	}

	/* Send the first multi thread message. */
	but_mt_msg_send(0, 0);
	
	/* Suspend the main process. */
	os_sem_wait(&but_stat.suspend);

	/* Kill all installed threads. */
	for (i = 0; i < OS_THREAD_LIMIT; i++) {
		/* Kill the test thread. */
		os_thread_destroy(p[i]);
	}

	/* Release the control semaphore for the main process. */
	os_sem_delete(&but_stat.suspend);

	/* Reset the global thread list. */
	os_memset(p, 0, sizeof(void *) * OS_THREAD_LIMIT);
	
	/* Verify the OS state. */
	stat = test_os_stat(&expected);
	
	return stat;
}

/**
 * but_queue_limit() - fill the thread input queue up to the limit.
 *
 * Return:	the execution state..
 **/
static int but_queue_limit(void)
{
	os_statistics_t expected = { 4, 4, 0, 2306, 2306, 0 };
	void *p;
	int i, stat;

	/* Define the message information. */
	but_stat.msg_info = "ping";
	
	/* Create the control semaphore for the main process. */
	os_sem_init(&but_stat.suspend, 0);

	/* Create and start the test thread. */
	p = os_thread_create("test", OS_THREAD_PRIO_FOREG, 256);

	/* Send the messagae to the test thread and verify the cleanup of the
	 *  message queue in os_exit. */
	for (i = 0; i < 256; i++)
		but_msg_send(p);

	/* Suspend the main process. */
	os_sem_wait(&but_stat.suspend);

	/* Kill the test thread. */
	os_thread_destroy(p);

	/* Release the control semaphore for the main process. */
	os_sem_delete(&but_stat.suspend);

	/* Verify the OS state. */
	stat = test_os_stat(&expected);
	
	return stat;
}

/**
 * but_malloc_limit() - loop over malloc and free.
 *
 * Return:	the execution state..
 **/
static int but_malloc_limit(void)
{
	os_statistics_t expected = { 4, 4, 0, 2050, 2050, 0 };
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
 * but_queue() - test the thread input queue.
 *
 * Return:	the execution state..
 **/
static int but_queue(void)
{
	os_statistics_t expected = { 4, 4, 0, 2, 2, 0 };
	void *p;
	int stat;

	/* Define the message information. */
	but_stat.msg_info = "ping";
	
	/* Create the control semaphore for the main process. */
	os_sem_init(&but_stat.suspend, 0);

	/* Create and start the test thread. */
	p = os_thread_create("test", OS_THREAD_PRIO_FOREG, 256);

	/* Start the interworking with the test thread. */
	but_msg_send(p);

	/* Suspend the main process. */
	os_sem_wait(&but_stat.suspend);

	/* Kill the test thread. */
	os_thread_destroy(p);

	/* Release the control semaphore for the main process. */
	os_sem_delete(&but_stat.suspend);

	/* Verify the OS state. */
	stat = test_os_stat(&expected);
	
	return stat;
}

/**
 * but_thread_limit() - install and kill all availabe threads.
 *
 * Return:	the execution state..
 **/
static int but_thread_limit(void)
{
	os_statistics_t expected = { 4, 4, 0, 1, 1, 0 };
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
 * but_thread() - test the thread handling.
 *
 * Return:	the execution state..
 **/
static int but_thread(void)
{
	os_statistics_t expected = { 4, 4, 0, 1, 1, 0 };
	void *p;
	char *name;
	int stat;

	/* Create and start the test thread. */
	p = os_thread_create("test", OS_THREAD_PRIO_FOREG, 256);

	/* Get the name of the thread. */
	name = os_thread_name(p);
	printf("%s Thread name: %s\n", P, name);

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
 * but_string() - string and mem test like strcmp or memcmp.
 *
 * Return:	the execution state..
 **/
static int but_string(void)
{
	os_statistics_t expected = { 4, 4, 0, 1, 1, 0 };
	char    s1[BUT_LEN], *s2 = "*coverage*";
	int n, stat;

	/* Fill memory with a contant byte. */
	n = os_strlen(s2);
	os_memset(s1, 0, n);

	/* Copy memory area. */
	os_memcpy(s1, BUT_LEN, s2, n);

	/* Compare memory areas. */
	stat = os_memcmp(s1, s2, n);
	TEST_ASSERT_EQ(0, stat);

	/* Determine the length of a fixed-size string. */
	stat = os_strnlen(s1, BUT_LEN);
	TEST_ASSERT_EQ(n, stat);

	/* Copy a string. */
	os_memset(s1, 0, n);
	os_strcpy(s1, BUT_LEN, s2);

	/* Compare two strings. */
	stat = os_strcmp(s1, s2);
	TEST_ASSERT_EQ(0, stat);
	
	stat = os_strncmp(s1, s2, n);
	TEST_ASSERT_EQ(0, stat);

	/* Verify the OS state. */
	stat = test_os_stat(&expected);
	
	return stat;
}

/**
 * but_mem() - os_malloc and os_free test.
 *
 * Return:	the execution state..
 **/
static int but_mem(void)
{
	os_statistics_t expected = { 4, 4, 0, 1, 1, 0 };
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
 * but_sync() - test the thread synchronization procedures.
 *
 * Return:	the execution state..
 **/
static int but_sync(void)
{	
	os_statistics_t expected = { 4, 4, 0, 0, 0, 0 };
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
 * but_trap() - test the trap handling.
 *
 * Return:	the execution state..
 **/
static int but_trap(void)
{
	os_statistics_t expected = { 4, 4, 0, 0, 0, 0 };
	int stat;

	/* Use the test agreement. */
	os_trap(__FILE__, "*coverage*", __LINE__);

	/* Verify the OS state. */
	stat = test_os_stat(&expected);
	
	return stat;
}

/*============================================================================
  EXPORTED FUNCTIONS
  ============================================================================*/
/**
 * but_init() - define vot_p for TEST_ASSERT_EQ.
 *
 * Return:	None.
 **/
void but_init(test_stat_t *stat)
{
	vote_p = stat;
}

/**
 * but_run() - execute the basic van OS interfaces.
 *
 * Return:	None.
 **/
void but_run(void)
{
	/* Execute the list of the test cases. */
	test_set_process(TEST_ADD(but_system));
}
