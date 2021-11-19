// SPDX-License-Identifier: GPL-2.0

/*
 * Pthread interfaces.
 *
 * Copyright (C) 2021 Gerald Schueller <gerald.schueller@web.de>
 */

/*============================================================================
  IMPORTED INCLUDE REFERENCES
  ============================================================================*/
#include <stdio.h>   /* Standard C library: NULL. */
#include <string.h>  /* String operations. */
#include <pthread.h> /* POSIX thread. */
#include "trap.h"    /* Exception handling: TRAP */
#include "os.h"      /* Operating system: os_sem_create() */

/*============================================================================
  EXPORTED INCLUDE REFERENCES
  ============================================================================*/
/*============================================================================
  LOCAL NAME CONSTANTS DEFINITIONS
  ============================================================================*/

/* Number of the allowed threads. */
#define OS_THREAD_NUMBER  2

/*============================================================================
  MACROS
  ============================================================================*/
/*============================================================================
  LOCAL TYPE DEFINITIONS
  ============================================================================*/

/**
 * os_thread_entry_t - entry of the thread table.
 *
 * @thread:     addres of the thread description.
 * @id:         number of the table entry.
 * @is_in_use:  1, if the table entry is used.
 **/
typedef struct {
	os_thread_t  *thread;
	int           id;
	int           is_in_use;	
} os_thread_entry_t;

/*============================================================================
  LOCAL DATA
  ============================================================================*/

/**
 * os_thread_table - list of the installed thread.
 *
 * @list:     list of the installed thread.
 * @count:    number of the installed threads.
 * @protect:  protect the access to the thread list.
 **/
static struct os_thread_table_s {
	os_thread_entry_t  list[OS_THREAD_NUMBER];
	unsigned int       count;
	pthread_mutex_t    protect;
} os_thread_table;

/*============================================================================
  LOCAL FUNCTION PROTOTYPES
  ============================================================================*/
/*============================================================================
  LOCAL FUNCTIONS
  ============================================================================*/

/**
 * os_sync_complete() - finalize the synchronization between the parent and child
 * thread.
 *
 * @sync:       address of the pthread synchronization conditions.
 *
 * Return:	None.
 **/
static void os_sync_complete(os_sync_t *sync)
{
	int ret;

	/* Enter the critical section. */
	ret = pthread_mutex_lock(&sync->mutex);
	TRAP_IF(ret != 0);

	/* Update the synchronization status. */
	sync->done = 1;

	/* Resume the suspended thread in os_sync_poll(). */
	ret = pthread_cond_signal(&sync->cond);
	TRAP_IF(ret != 0);

	/* Leave the critical section. */
	ret = pthread_mutex_unlock(&sync->mutex);
	TRAP_IF(ret != 0);
}

/**
 * os_sync_wait() - synchronize the parent and child thread.
 *
 * @sync:       address of the pthread synchronization conditions.
 *
 * Return:	None.
 **/
static void os_sync_wait(os_sync_t *sync)
{
	int ret;

	/* Enter the critical section. */
	ret = pthread_mutex_lock(&sync->mutex);
	TRAP_IF(ret != 0);

	/* Wait for the call of os_sync_end(). */
	while (! sync->done) {
		/* Release the mutex atomically and block the calling thread on cond. */
		ret = pthread_cond_wait(&sync->cond, &sync->mutex);
		TRAP_IF(ret != 0);
	}

	/* Leave the critical section. */
	ret = pthread_mutex_unlock(&sync->mutex);
	TRAP_IF(ret != 0);
}

/**
 * os_sync_init() - initialize the pthread wait conditions.
 *
 * @sync:       address of the pthread synchronization conditions.
 *
 * Return:	None.
 **/
static void os_sync_init(os_sync_t *sync)
{
	int ret = -1;

	/* Entry condition. */
	TRAP_IF(sync == NULL);
	
	/* Initialize the mutex. */
	ret = pthread_mutex_init(&sync->mutex, NULL);
	TRAP_IF(ret != 0);

	/* Initialize the condition variable. */
	ret = pthread_cond_init(&sync->cond, NULL);
	TRAP_IF(ret != 0);

	/* Reset the synchronization status. */
	sync->done = 0;
}

/**
 * os_thread_id_alloc() - allocate a thread table entry.
 *
 * @cond:       address of the pthread wait conditions.
 *
 * Return:	None.
 **/
static void os_thread_id_alloc(os_thread_t *thread)
{
	struct os_thread_table_s *table;
	os_thread_entry_t *list;
	int i;

	/* Get the address of the thread table. */
	table = &os_thread_table;
	
	/* Get the address of the first table entry. */
	list = table->list;

	/* Enter the critical section. */
	os_cs_enter (&table->protect);
	
	/* Search for a free table entry. */
	for (i = 0; i < OS_THREAD_NUMBER && list->is_in_use; i++, list++)
		;

	/* Test the table state. */
	TRAP_IF(i >= OS_THREAD_NUMBER);

	/* Allocate a table entry. */
	list->thread    = thread;
	list->id        = i;
	list->is_in_use = 1;
	
	table->count++;

	/* Leave the critical section. */
	os_cs_leave (&table->protect);
}

/**
 * os_thread_prio() - calculate the thread priority.
 *
 * @prio:  thread priority.
 *
 * Return:	the updated thread priority.
 **/
static os_thread_prio_t os_thread_prio(os_thread_prio_t prio)
{
	/* Test the thread priority. */
	switch(prio) {
	case OS_THREAD_PRIO_HARDRT:
	case OS_THREAD_PRIO_SOFTRT:
	case OS_THREAD_PRIO_BACKG:
	case OS_THREAD_PRIO_FOREG:
		return prio;
	default:
		return OS_THREAD_PRIO_DEFAULT;
	}
}


/*============================================================================
  EXPORTED FUNCTIONS
  ============================================================================*/

/**
 * os_thread_create() - create a thread.
 *
 * @thread:  address of the pthread data.
 * @name:    name of the thread.
 * @prio:    thread priority.
 *
 * Return:	None.
 **/
void os_thread_create(os_thread_t *thread, const char *name,
		      os_thread_prio_t prio)
{
	int ret;
	struct sched_param p;
	int c_type, orig_c;

	/* Entry condition. */
	TRAP_IF(thread == NULL || name == NULL);

	/* Allocate a thread table entry. */
	os_thread_id_alloc(thread);
	
	/* Calculate the thread priority. */
	prio = os_thread_prio(prio);
	
	/* Initialize the thread attributes with default values. */
	ret = pthread_attr_init(&thread->attr);
	TRAP_IF(ret != 0);

	/* Set the scheduling policy attribute. */
	ret = pthread_attr_setschedpolicy(&thread->attr, SCHED_RR);
	TRAP_IF(ret != 0);

	/* Define the thread priority. */
	p.sched_priority = prio;
	ret = pthread_attr_setschedparam(&thread->attr, &p);
	TRAP_IF(ret != 0);

	/* Save the thread name. */
	/* XXX */
	strncpy(thread->name, name , OS_MAX_STRING_LEN);
	thread->name[OS_MAX_STRING_LEN] = '\0';

	/* Complete the thread state. */
	thread->state  = OS_THREAD_SUSPENDED;
	thread->prio   = prio;

	/* Initialize the synchronization elements. */
	os_sync_init(&thread->suspend_c);
	os_sync_init(&thread->suspend_t);

	/* Set the cancelability type. */
	c_type = PTHREAD_CANCEL_ENABLE;
	ret = pthread_setcancelstate(c_type, &orig_c);
	TRAP_IF(ret != 0);

#if 0 /* XXX */
	/* Create the pthread. */
	ret = pthread_create(&thread->p_id, NULL, os_thread_start_cb, thrread);
	TRAP_IF(ret != 0);
	
	/* Suspend the caller and wait for the thread start. */
	os_sync_wait(&thread->suspend_c);
#endif

	/* Update the thread state. */
	thread->state = OS_THREAD_SUSPENDED;
}

/**
 * os_thread_start() - start the thread.
 *
 * @thread:  address of the pthread data.
 *
 * Return:	None.
 **/
void os_thread_start(os_thread_t *thread)
{
	/* Entry condition. */
	TRAP_IF(thread == NULL);
}

/**
 * os_thread_init() - initialize the thread table.
 *
 * Return:	None.
 **/
void os_thread_init(void)
{
	/* Create the mutex for the critical section. */
	os_cs_init(&os_thread_table.protect);
}
