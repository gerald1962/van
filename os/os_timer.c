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
#include <sys/time.h>    /* Get / set time: gettimeofday(). */
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
 *
 * @s_start:       system start time of the timer.
 * @c_start:       current start time of the periodic timer.
 * @l_elapsed:     execution time of the last cycle. 
 * @min:           minimum of the processing time.
 * @max:           maximum of the processing time.
 * @cycles:        timer expiration counter.
 * @sys_ov_count:  overrun count caused through system load.
 * @exp_ov_count:  overrun count caused through your himself.
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
		
		struct timeval  s_start;
		struct timeval  c_start;
		struct timeval  l_elapsed;
		struct timeval  min;
		struct timeval  max;
		long long       cycles;
		int             sys_ov_count;
		int             exp_ov_count;
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
 * sv:  pointer to the affected timer.
 *
 * Return:	None.
 **/
static void tm_handler(union sigval sv)
{
	struct tm_elem_s *t;
	struct timeval tv;
	int rv, suspended;
	
	/* Get the pointer to the timer descripton. */
	t = sv.sival_ptr;

	/* Entry condition. */
	OS_TRAP_IF(t == NULL || ! t->assigned);

	/* Update the expiration counter. */
	t->cycles++;
	
	/* Get the system overrun count for a POSIX per-process timer. */
	rv = timer_getoverrun(t->timer_id);
	OS_TRAP_IF(rv == -1);

	/* Save the overrun count caused by system load. */
	t->sys_ov_count = rv;
	
	/* Test the state of the timer owner. */
	suspended = atomic_exchange(&t->suspended, 0);
	if (! suspended) {
		/* Update the timer expiration counter. */
		t->exp_ov_count++;

		/* XXX Trace. */
		printf("%s: n:%s [i=%d, s-o=%d, e-o=%d]\n", F, t->name,
		       t->id, rv, t->exp_ov_count);
		
		return;
	}

	/* Get the current time. */
	rv = gettimeofday(&tv, NULL);
	OS_TRAP_IF(rv != 0);

	/* Calculate the runtime of the transition. */
	timersub (&tv, &t->c_start, &t->l_elapsed);

	/* XXX Trace. */
	printf("%s: n:%s [i=%d, s-o=%d, e-o=%d]\n", F, t->name,
	       t->id, rv, t->exp_ov_count);

	/* Calculate the minimum of the processing time. */
	if (! timercmp(&t->min, &tv, >))
		t->min = tv;

	/* Calculate the maximum of the processing time. */
	if (! timercmp(&t->max, &tv, <))
		t->max = tv;
	
	/* Update the start time of the transition. */
	t->c_start = tv;

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
 * os_timer_get() - provide all timer information.
 *
 * @id:  assigned timer id by os_timer_init().
 * @ts:  pointer to the timer state.
 * 
 * Return:	None.
 **/
void os_timer_get(int id, os_tm_state_t *ts)
{
	struct tm_elem_s *t;
	
	/* Enter the critical section. */
	os_cs_enter(&tm.mutex);
	
	/* Entry condition. */
	OS_TRAP_IF(id < 0 || id >= OS_TIMER_LIMIT || ts == NULL);

	/* Get the pointer to the timer element. */
	t = &tm.elem[id];
	OS_TRAP_IF(! t->assigned);

	/* XXX Copy all available timer information. */

	/* Update the barrier state. */
	ts->sys_ov_count = t->sys_ov_count;
	ts->exp_ov_count = t->exp_ov_count;
	
	/* Leave the critical section. */
	os_cs_leave(&tm.mutex);
}

/**
 * os_timer_barrier() - ordering timer constraint on controller operations 
 * before and after the barrier instructions as agreed in os_timer_init().
 *
 * @id:  assigned timer id by os_timer_init().
 *
 * Return:	-1, if the interval has not been met, otherwise 0.
 **/
int os_timer_barrier(int id)
{
	struct tm_elem_s *t;
	
	/* Enter the critical section. */
	os_cs_enter(&tm.mutex);
	
	/* Entry condition. */
	OS_TRAP_IF(id < 0 || id >= OS_TIMER_LIMIT);

	/* Get the pointer to the timer element. */
	t = &tm.elem[id];
	OS_TRAP_IF(! t->assigned);

	/* Indicate the entering of the barrier. */
	atomic_store(&t->suspended, 1);
	
	/* Suspend the timer owner. */
	os_sem_wait (&t->suspend);
	
	/* Leave the critical section. */
	os_cs_leave(&tm.mutex);

	/* Calculate the return value. */
	if (t->sys_ov_count || t->exp_ov_count)
		return -1;

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
	struct tm_elem_s *t;
	int rv;
	
	/* Enter the critical section. */
	os_cs_enter(&tm.mutex);
	
	/* Entry condition. */
	OS_TRAP_IF(id < 0 || id >= OS_TIMER_LIMIT);

	/* Get the pointer to the timer element. */
	t = &tm.elem[id];
	OS_TRAP_IF(! t->assigned);

	/* Initialize the timer trigger. */
	os_memset(&trigger, 0, sizeof(struct itimerspec));

	/* If the timer was already armed, then the previous settings are
	 * overwritten. If the new values specifies a zero value (i.e., both
	 * subfields are zero, then the timer is disarmed. */

        /* Disarm the timer. No flags are set and no old values will be
	 * retrieved. */
        rv = timer_settime(t->timer_id, 0, &trigger, NULL);
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
	struct tm_elem_s *t;
	int rv;
	
	/* Enter the critical section. */
	os_cs_enter(&tm.mutex);
	
	/* Entry condition. */
	OS_TRAP_IF(id < 0 || id >= OS_TIMER_LIMIT);

	/* Get the pointer to the timer element. */
	t = &tm.elem[id];
	OS_TRAP_IF(! t->assigned);

	/* Initialize the timer trigger. */
	os_memset(&trigger, 0, sizeof(struct itimerspec));

	/* Defines the input values for timer_settime(). The key input values is
	 * the interval with the base value 1 ms: 10000000. 
	 * The substructures it_interval of the itimerspec structure allows a
	 * time value to be specified in seconds and nanoseconds. */
	trigger.it_interval.tv_nsec = 10000000 * t->interval;

	/* Save the current start time. */
	rv = gettimeofday(&t->c_start, NULL);
	OS_TRAP_IF(rv != 0);
	
        /* Arm the timer. No flags are set and no old_value will be
	 * retrieved. */
        rv = timer_settime(t->timer_id, 0, &trigger, NULL);
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
	struct tm_elem_s *t;
	int rv;
	
	/* Enter the critical section. */
	os_cs_enter(&tm.mutex);
	
	/* Entry condition. */
	OS_TRAP_IF(id < 0 || id >= OS_TIMER_LIMIT);

	/* Get the pointer to the timer element. */
	t = &tm.elem[id];
	OS_TRAP_IF(! t->assigned);
	
	/* Release the thread attibutes object with the realtime setting. */
	rv = pthread_attr_destroy(&t->attr);
	OS_TRAP_IF(rv != 0);

	/* Disarm and delete the timer. */
	rv = timer_delete(t->timer_id);
	OS_TRAP_IF(rv != 0);
	
	/* Release this timer. */
	t->assigned = 0;

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
	struct tm_elem_s *t;
	int i, tm_id, len, rv;
	
	/* Entry condition. */
	OS_TRAP_IF(name == NULL || interval < 1);
	
	/* Enter the critical section. */
	os_cs_enter(&tm.mutex);
	
	/* Search for a free wait element. */
	for (i = 0, t = tm.elem; i < OS_TIMER_LIMIT; i++, t++) {
		if (! t->assigned)
			break;
	}

	/* Test the search result. */
	OS_TRAP_IF(i >= OS_TIMER_LIMIT);

	/* Reset the timer state. */
	os_memset(t, 0, sizeof(struct tm_elem_s));

	/* This timer is yours. */
	tm_id = i;
	t->assigned = 1;
	
	/* Save the timer name. */
	len = os_strlen(name);
	OS_TRAP_IF(len >= OS_MAX_NAME_LEN);
	os_memset(t->name, 0, OS_MAX_NAME_LEN);
	os_strcpy(t->name, OS_MAX_NAME_LEN, name);

	/* Install a periodic timer. */
	tm_create(t);
	
	/* Remember the birthdate. */
	rv = gettimeofday(&t->s_start, NULL);
	OS_TRAP_IF(rv != 0);

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
	struct tm_elem_s *t;
	int i;

	/* Enter the critical section. */
	os_cs_enter(&tm.mutex);
	
	/* Test and release the timer resources. */
	for (i = 0, t = tm.elem; i < OS_TIMER_LIMIT; i++, t++) {
		/* Test the timer state. */
		OS_TRAP_IF(t->assigned);
		os_sem_delete(&t->suspend);
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
	struct tm_elem_s *t;
	int i;
	
	/* Create the mutex for the critical section in the timer operations. */
	os_cs_init(&tm.mutex);

	/* Define the timer id. */
	for (i = 0, t = tm.elem; i < OS_TIMER_LIMIT; i++, t++) {
		t->id = i;
		os_sem_init(&t->suspend, 0);
	}
}
