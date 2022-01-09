// SPDX-License-Identifier: GPL-2.0

/*
 * Operating system interfaces.
 *
 * Copyright (C) 2022 Gerald Schueller <gerald.schueller@web.de>
 */

/*============================================================================
  IMPORTED INCLUDE REFERENCES
  ============================================================================*/
#include <pthread.h>     /* POSIX thread. */
#include <signal.h>      /* for struct sigevent and SIGEV_THREAD */
#include "os.h"          /* Operating system: os_timer_init() */
#include "os_private.h"  /* Local interfaces of the OS: os_tm_init() */

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
/**
 * tm_t - state of a periodic timer.
 *
 * @mutex:         protect the critical section in the timer operations.
 * @id:            timer id.
 * @name:          timer name.
 * @assigned:      if 1, the element has been reserved.
 * @interval:      repeating interval in milliseconds.
 * @suspend:       suspend the wait caller.
 * @attr:          thread attibutes objct.
 * @suspended:     if 1, the caller has been suspend in os_timer_barrier().
 * @sys_ov_count:  the overrun count of this timer caused by system load.
 * @exp_ov_count:  timer expiration counter.
 **/
typedef struct {
	pthread_mutex_t  mutex;
	
	struct tm_elem_s {
		int      id;
		char     name[OS_MAX_NAME_LEN + 1];
		int      assigned;
		int      interval;
		timer_t  timer_id;
		sem_t    suspend;
		pthread_attr_t  attr;
		atomic_int      suspended;
		atomic_int      sys_ov_count;
		atomic_int      exp_ov_count;
	} elem[OS_TIMER_LIMIT];
} tm_t;

/*============================================================================
  LOCAL DATA
  ============================================================================*/
/* State of the periodic timer list. */
static tm_t tm;

/*============================================================================
  LOCAL FUNCTION PROTOTYPES
  ============================================================================*/
/*============================================================================
  LOCAL FUNCTIONS
  ============================================================================*/
/**
 * tm_handler() -   this function will be invoked upon timer expiration as if it
 * were the start function of a new thread. 
 *
 * sv:  pointer to the assigned timer.
 *
 * Return:	None.
 **/
static void tm_handler(union sigval sv)
{
	struct tm_elem_s *t;
	int rv, suspended;

	/* Get the pointer to the timer descripton. */
	t = sv.sival_ptr;

	/* Entry condition. */
	OS_TRAP_IF(! t->assigned);

	/* Get the system overrun count for a POSIX per-process timer. */
	rv = timer_getoverrun(t->timer_id);
	OS_TRAP_IF(rv == -1);

	/* Save the overrun count caused by system load. */
	atomic_store(&t->sys_ov_count, rv);
	
	printf("%s: n:%s [i=%d, o=%d]\n", F, t->name, t->id, rv);

	/* Test the state of the timer owner. */
	suspended = atomic_load(&t->suspended);
	if (! suspended) {
		/* Update the timer expiration counter. */
		atomic_fetch_add(&t->exp_ov_count, 1);
		return;
	}

	/* Resume the suspended caller. */
	os_sem_release(&t->suspend);
}

/**
 * tm_create() - install a periodic timer.
 *
 * @t:  pointer to the timer descripton.
 *
 * Return:	None.
 **/
static void tm_create(struct tm_elem_s *t)
{
	struct sched_param parm;
	struct sigevent sev;
	pthread_attr_t attr;
	int rv;

	/* Initialize the thread attributes object. */
	rv = pthread_attr_init(&attr);
	OS_TRAP_IF(rv != 0);

	/* Set the value of the scheduling priority to real time. */
	parm.sched_priority = 255;
	rv = pthread_attr_setschedparam(&attr, &parm);
	OS_TRAP_IF(rv != 0);

	/* Set the notification method as SIGEV_THREAD: upon timer expiration,
	 * tm_handler(), will be invoked as if it were the start function of a
	 * new thread. */
	os_memset(&sev, 0, sizeof(struct sigevent));
	sev.sigev_notify = SIGEV_THREAD;
	sev.sigev_notify_function = tm_handler;
	sev.sigev_value.sival_int = 20;
	sev.sigev_value.sival_ptr = &t;
	sev.sigev_notify_attributes = &attr;

	/* Create a new timer. */
	rv = timer_create(CLOCK_REALTIME, &sev, &t->timer_id);
	OS_TRAP_IF(rv != 0);
}

/*============================================================================
  EXPORTED FUNCTIONS
  ============================================================================*/
/**
 * os_timer_barrier() - ordering timer constraint on controller operations 
 * before and after the barrier instructions as agreed in os_timer_init().
 *
 * @id:  assigned timer id by os_timer_init().
 * @bs:  current barrier state.
 *
 * Return:	0, if the interval has not been met.
 **/
int os_timer_barrier(int id, os_tm_barrier_t *bs)
{
	struct tm_elem_s *elem;
	int rv;
	
	/* Enter the critical section. */
	os_cs_enter(&tm.mutex);
	
	/* Entry condition. */
	OS_TRAP_IF(id < 0 || id >= OS_TIMER_LIMIT || bs == NULL);

	/* Get the pointer to the timer element. */
	elem = &tm.elem[id];
	OS_TRAP_IF(! elem->assigned);

	/* XXX */

	/* Leave the critical section. */
	os_cs_leave(&tm.mutex);
	
	return 0;
}

/**
 * os_timer_stop() - stop a periodic timer.
 *
 * @id:  assigned timer id by os_timer_init().
 *
 * Return:	None.
 **/
void os_timer_stop(int id)
{
        struct itimerspec trigger;
	struct tm_elem_s *elem;
	int rv;
	
	/* Enter the critical section. */
	os_cs_enter(&tm.mutex);
	
	/* Entry condition. */
	OS_TRAP_IF(id < 0 || id >= OS_TIMER_LIMIT);

	/* Get the pointer to the timer element. */
	elem = &tm.elem[id];
	OS_TRAP_IF(! elem->assigned);

	/* Initialize the timer trigger. */
	os_memset(&trigger, 0, sizeof(struct itimerspec));

	/* If the timer was already armed, then the previous settings are
	 * overwritten. If the newvalues specifies a zero value (i.e., both
	 * subfields are zero, then the timer is disarmed. */

        /* Disarm the timer. No flags are set and no old_value will be
	 * retrieved. */
        rv = timer_settime(elem->timer_id, 0, &trigger, NULL);
	OS_TRAP_IF(rv != 0);

	/* Leave the critical section. */
	os_cs_leave(&tm.mutex);
}

/**
 * os_timer_start() - start a periodic timer.
 *
 * @id:  assigned timer id by os_timer_init().
 *
 * Return:	None.
 **/
void os_timer_start(int id)
{
        struct itimerspec trigger;
	struct tm_elem_s *elem;
	int rv;
	
	/* Enter the critical section. */
	os_cs_enter(&tm.mutex);
	
	/* Entry condition. */
	OS_TRAP_IF(id < 0 || id >= OS_TIMER_LIMIT);

	/* Get the pointer to the timer element. */
	elem = &tm.elem[id];
	OS_TRAP_IF(! elem->assigned);

	/* Initialize the timer trigger. */
	os_memset(&trigger, 0, sizeof(struct itimerspec));

	/* Defines the input values for timer_settime(). The key input values is
	 * the interval with the base value 1 ms: 10000000. 
	 * The substructures it_interval of the itimerspec structure allows a
	 * time value to be specified in seconds and nanoseconds. */
	trigger.it_interval.tv_nsec = 10000000 * elem->interval;

        /* Arm the timer. No flags are set and no old_value will be
	 * retrieved. */
        rv = timer_settime(elem->timer_id, 0, &trigger, NULL);
	OS_TRAP_IF(rv != 0);

	/* Leave the critical section. */
	os_cs_leave(&tm.mutex);
}

/**
 * os_timer_delete() - remove a periodic timer.
 *
 * @id:  assigned timer id by os_timer_init().
 *
 * Return:	None.
 **/
void os_timer_delete(int id)
{
	struct tm_elem_s *elem;
	int rv;
	
	/* Enter the critical section. */
	os_cs_enter(&tm.mutex);
	
	/* Entry condition. */
	OS_TRAP_IF(id < 0 || id >= OS_TIMER_LIMIT);

	/* Get the pointer to the timer element. */
	elem = &tm.elem[id];
	OS_TRAP_IF(! elem->assigned);
	
	/* Free the timer element. */
	elem->assigned = 0;

	/* Release the thread attibutes object. */
	rv = pthread_attr_destroy(&elem->attr);
	OS_TRAP_IF(rv != 0);

	/* Disarm and delete the timer. */
	rv = timer_delete(elem->timer_id);
	OS_TRAP_IF(rv != 0);
	
	/* Leave the critical section. */
	os_cs_leave(&tm.mutex);
}

/**
 * os_timer_init() - create a periodic timer with a repeating interval of i
 * milliseconds.
 *
 * @name:      timer name.
 * @interval:  repeating interval in milliseconds.
 *
 * Return:	the timer id.
 **/
int os_timer_init(const char *name, int interval)
{
	struct tm_elem_s *elem;
	int i, tm_id, len;
	
	/* Entry condition. */
	OS_TRAP_IF(name == NULL || interval < 1);
	
	/* Enter the critical section. */
	os_cs_enter(&tm.mutex);
	
	/* Search for a free wait element. */
	for (i = 0, elem = tm.elem; i < OS_TIMER_LIMIT; i++, elem++) {
		if (! elem->assigned)
			break;
	}

	/* Test the search result. */
	OS_TRAP_IF(i >= OS_TIMER_LIMIT);
	tm_id = i;
	elem->assigned = 1;
	
	/* Save the timer name. */
	len = os_strlen(name);
	OS_TRAP_IF(len >= OS_MAX_NAME_LEN);
	os_memset(elem->name, 0, OS_MAX_NAME_LEN);
	os_strcpy(elem->name, OS_MAX_NAME_LEN, name);

	/* Install a periodic timer. */
	tm_create(elem);
	
	/* Leave the critical section. */
	os_cs_leave(&tm.mutex);

	return tm_id;
}

/**
 * os_tm_exit() - release the timer resources.
 *
 * Return:	None.
 **/
void os_tm_exit(void)
{
	struct tm_elem_s *elem;
	int i;

	/* Enter the critical section. */
	os_cs_enter(&tm.mutex);
	
	/* Test and release the timer resources. */
	for (i = 0, elem = tm.elem; i < OS_TIMER_LIMIT; i++) {
		/* Test the timer state. */
		OS_TRAP_IF(elem->assigned);
		os_sem_delete(&elem->suspend);
	}

	/* Leave the critical section. */
	os_cs_leave(&tm.mutex);
	
	/* Delete the mutex for the critical sections. */
	os_cs_destroy(&tm.mutex);
}

/**
 * os_tm_init() - initialize the timer resources.
 *
 * Return:	None.
 **/
void os_tm_init(void)
{
	struct tm_elem_s *elem;
	int i;
	
	/* Create the mutex for the critical section in the timer operations. */
	os_cs_init(&tm.mutex);

	/* Define the timer id. */
	for (i = 0, elem = tm.elem; i < OS_TIMER_LIMIT; i++, elem++) {
		elem->id = i;
		os_sem_init(&elem->suspend, 0);
	}
}
