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

typedef struct {
	int             p;
	pthread_cond_t  cond;
	pthread_mutex_t mutex;
} os_cond_t;


/*============================================================================
  LOCAL DATA
  ============================================================================*/

/**
 * os_thread_table - list of the installed threads.
 *
 * @thread:     addres of the thread description.
 * @id:         number of the table entry.
 * @is_in_use:  1, if the table entry is used.
 **/
static struct os_thread_table_s {
	os_thread_t  *thread;
	int           id;
	int           is_in_use;	
} os_thread_table[OS_THREAD_NUMBER];


/*============================================================================
  LOCAL FUNCTION PROTOTYPES
  ============================================================================*/

/**
 * os_thread_id_alloc() - allocate a thread table entry.
 *
 * @cond:       address of the pthread wait conditions.
 *
 * Return:	None.
 **/
static void os_thread_id_alloc(os_thread_t *thread)
{
	struct os_thread_table_s *p;
	int i;

	/* Get the address of the first table entry. */
	p = os_thread_table;
	
	/* Search for a free table entry. */
	for (i = 0; i < OS_THREAD_NUMBER; i++, p++) {
		/* Test the entry state. */
		if (p->is_in_use)
			continue;

		/* Allocate the table entry. */
		p->thread    = thread;
		p->id        = i;
		p->is_in_use = 1;
		
		return;
	}

	/* All table entries have been allocated. */
	TRAP();
}


/*============================================================================
  LOCAL FUNCTIONS
  ============================================================================*/

/**
 * os_cond_init() - initialize the pthread wait conditions.
 *
 * @cond:       address of the pthread wait conditions.
 *
 * Return:	None.
 **/
static void os_cond_init(os_cond_t  *cond)
{
	int ret = -1;

	/* Entry condition. */
	TRAP_IF(cond == NULL);
	
	/* Initialize the mutex. */
	ret = pthread_mutex_init(&cond->mutex, NULL);
	TRAP_IF(ret != 0);

	/* Initialize the condition variable. */
	ret = pthread_cond_init(&cond->cond, NULL);
	TRAP_IF(ret != 0);

	cond->p = 0;
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

/**
 * os_thread_create() - create a thread.
 *
 * @thread:  address of the pthread data.
 * @name:    name of the thread.
 * @prio:    thread priority.
 *
 * Return:	None.
 **/
static void os_thread_create(os_thread_t *thread, const char *name,
			     os_thread_prio_t prio)
{
	int ret;
	struct sched_param p;
	int c_state;

	/* Allocate a thread table entry. */
	os_thread_id_alloc(thread);
	
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
	strncpy(thread->name, name , OS_MAX_STRING_LEN);
	thread->name[OS_MAX_STRING_LEN] = '\0';

	/* Complete the thread state. */
	thread->prio  = prio;
	thread->state = OS_THREAD_SUSPENDED;

	/* XXX */

	/* Infor    ret = pthread_setcancelstate(cstate, &old_cstate);
	   if (ret != 0)
	   {
	   FCT_RETURN(UTA_FAILURE);
	   } /* no else */
	
m the OS, what you can do with the comming thread. */
	
}


/*============================================================================
  EXPORTED FUNCTIONS
  ============================================================================*/

/**
 * os_thread_start() - create and start a thread.
 *
 * @thread:  address of the pthread data.
 * @name:    name of the thread.
 * @prio:    thread priority.
 *
 * Return:	None.
 **/
void os_thread_start(os_thread_t *thread, const char *name,
		     os_thread_prio_t prio)
{
	/* Entry condition. */
	TRAP_IF(thread == NULL || name == NULL);

	/* Calculate the thread priority. */
	prio = os_thread_prio(prio);
	
	/* Create a thread. */
	os_thread_create(thread, name, prio);
}
