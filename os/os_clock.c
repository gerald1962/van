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
#include <errno.h>       /* Linux error codes: EINTR. */
#include "os.h"          /* Operating system: os_timer_init() */
#include "os_private.h"  /* Local interfaces of the OS: os_tm_init() */

/*============================================================================
  EXPORTED INCLUDE REFERENCES
  ============================================================================*/
/*============================================================================
  LOCAL NAME CONSTANTS DEFINITIONS
  ============================================================================*/
/* Muliplier for conversion from nanoseconds to millisecondss. */
#define CLOCK_NSEC_M  1000000

/*============================================================================
  MACROS
  ============================================================================*/
/*============================================================================
  LOCAL TYPE DEFINITIONS
  ============================================================================*/
/**
 * tm_t - state of a periodic timer.
 *
 * @mutex:       protect the critical section in the timer operations.
 * @id:          timer id.
 * @name:        timer name.
 * @assigned:    if 1, the element has been reserved.
 * @interval:    repeating interval in milliseconds.
 * @suspend:     suspend the wait caller.
 * @suspended:   if 1, the caller has been suspend in os_timer_barrier().
 *
 * @s_start:     system start time.
 * @s_end:       system end time.
 * @c_start:     current start time of the transition.
 * @busy:        execution time of the last cycle.
 * @min:         minimum of the processing time.
 * @max:         maximum of the processing time.
 * @cycles:      transition counter.
 * @k_ov_count:  Linux kernel overrun counter.
 * @u_ov_count:  User overrun counter.
 **/
typedef struct {
	pthread_mutex_t  mutex;
	
	struct tm_elem_s {
		int         id;
		char        name[OS_MAX_NAME_LEN + 1];
		int         assigned;
		int         interval;
		timer_t     timer_id;
		sem_t       suspend;
		atomic_int  suspended;
		
		struct timeval  s_start;
		struct timeval  s_end;
		struct timeval  c_start;
		struct timeval  busy;
		struct timeval  min;
		struct timeval  max;
		int             cycles;
		int             k_ov_count;
		int             u_ov_count;
	} elem[OS_CLOCK_LIMIT];
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
	int suspended;
	
	/* Get the pointer to the timer descripton. */
	t = sv.sival_ptr;

	/* Entry condition. */
	OS_TRAP_IF(t == NULL || ! t->assigned);

	/* Test the presence of the timer user. */
	suspended = atomic_exchange(&t->suspended, 0);
	if (suspended) {
		/* The timer owner has not yet reached the target time. */
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
	struct sigevent sev;
	int rv;

	/* Set the notification method as SIGEV_THREAD: upon timer expiration,
	 * tm_handler(), will be invoked as if it were the start function of a
	 * new thread. */
	os_memset(&sev, 0, sizeof(struct sigevent));
	sev.sigev_notify = SIGEV_THREAD;
	sev.sigev_notify_function = tm_handler;
	sev.sigev_value.sival_ptr = t;

	/* Create a new timer. */
	rv = timer_create(CLOCK_REALTIME, &sev, &t->timer_id);
	OS_TRAP_IF(rv != 0);
}

/**
 * tm_ms() - calculate millliseconds.
 *
 * @tv:  pointer to the time description.
 * 
 * Return:	milliseconds.
 **/
static long long tm_ms(struct timeval *t)
{
	long long ms;

	/* Calculate milliseconds */
	ms = t->tv_sec * 1000LL + t->tv_usec / 1000;

	return ms;
}

/*============================================================================
  EXPORTED FUNCTIONS
  ============================================================================*/
/**
 * os_clock_trace() - print timer information.
 *
 * @id:    assigned timer id by os_clock_init().
 * @mode:  configuration of the trace output.
 * 
 * Return:	None.
 **/
void os_clock_trace(int id, int mode)
{
	struct tm_elem_s *t;
	struct timeval res;
	struct tm *l;
	char *n;
	
	/* Enter the critical section. */
	os_cs_enter(&tm.mutex);
	
	/* Entry condition. */
	OS_TRAP_IF(id < 0 || id >= OS_CLOCK_LIMIT);

	/* Get the pointer to the timer description. */
	t = &tm.elem[id];
	OS_TRAP_IF(! t->assigned);

	/* Get the pointer to the timer name. */
	n = t->name;

	/* Print the system start and end time. */

	/* System start time. */
	if(mode == OS_CT_FIRST || mode == OS_CT_LAST) {
		l = localtime(&t->s_start.tv_sec);
		printf("%s: proc-start: %d:%0d:%0d.%ld\n", n, 
		       l->tm_hour, l->tm_min, l->tm_sec, t->s_start.tv_usec);
	}
	
	/* System end time. */
	if(mode == OS_CT_LAST) {
		l = localtime(&t->s_end.tv_sec);
		printf("%s: proc-end:   %d:%0d:%0d.%ld\n", n, 
		       l->tm_hour, l->tm_min, l->tm_sec, t->s_end.tv_usec);
		
		/* Calculate the runtime of the cycles. */
		timersub (&t->s_end, &t->s_start, &res);
		printf("%s: runtime:    %lld ms\n", n, tm_ms(&res));
	}
	
	/* Print timer information. */
	printf("%s: timer id:   %d\n",      n, t->id);
	printf("%s: cycles:     %d\n",      n, t->cycles);
	printf("%s: interval:   %d ms\n",   n, t->interval);
	printf("%s: busy:       %lld ms\n", n, tm_ms(&t->busy));
	printf("%s: min:        %lld ms\n", n, tm_ms(&t->min));
	printf("%s: max:        %lld ms\n", n, tm_ms(&t->max));
	printf("%s: kernel-ov:  %d\n",      n, t->k_ov_count);
	printf("%s: user-ov:    %d\n\n",    n, t->u_ov_count);
	
	/* Leave the critical section. */
	os_cs_leave(&tm.mutex);
}

/**
 * os_clock_msleep() - sleep for the requested number of milliseconds.
 *
 * @msec:  millisecond value.
 *
 * Return:	None.
 **/
void os_clock_msleep(long msec)
{
	struct timespec ts;
	int rv;

	/* Entry condition. */
	if (msec < 1)
		return;

	/* Map the millisecond value. */
	ts.tv_sec = msec / 1000;
	ts.tv_nsec = (msec % 1000) * CLOCK_NSEC_M;

	do {
		/* Execute the high-resoluton sleep. */
		rv = nanosleep(&ts, &ts);
	} while (rv && errno == EINTR);
}

/**
 * os_clock_barrier() - ordering timer constraint on controller operations 
 * before and after the barrier instructions as agreed in os_clock_init().
 *
 * @id:  assigned timer id by os_clock_init().
 *
 * Return:	-1, if the interval has not been met, otherwise 0.
 **/
int os_clock_barrier(int id)
{
        struct itimerspec in;
	struct tm_elem_s *t;
	struct timeval now, res;
	long long busy;
	int rv, overrun;

	/* Enter the critical section. */
	os_cs_enter(&tm.mutex);
	
	/* Entry condition. */
	OS_TRAP_IF(id < 0 || id >= OS_CLOCK_LIMIT);

	/* Extended entry condition. */
	t = &tm.elem[id];
	OS_TRAP_IF(! t->assigned);

	/* Update the calculation counter. */
	t->cycles++;
	
	/* Get the system overrun count for a POSIX per-process timer. */
	rv = timer_getoverrun(t->timer_id);
	OS_TRAP_IF(rv == -1);

	/* Save the overrun count caused by system load. */
	t->k_ov_count = rv;
	
	/* Get the current time. */
	rv = gettimeofday(&now, NULL);
	OS_TRAP_IF(rv != 0);

	/* Calculate the runtime of the transition. */
	timersub (&now, &t->c_start, &res);
	t->busy = res;

	/* Calculate the overrun value. */
	busy = tm_ms(&t->busy);
	overrun =  (busy > t->interval);
	
	/* Calculate the minimum of the processing time. */
	if (timercmp(&t->min, &res, >))
		t->min = res;

	/* Calculate the maximum of the processing time. */
	if (timercmp(&t->max, &res, <))
		t->max = res;
	
	/* Test the overrun condition. */
	if (overrun) {
		/* Update the overrun counter. */
		t->u_ov_count++;

		/* Restart the clock timer. */
		
		/* Stop the timer. */
		os_memset(&in, 0, sizeof(struct itimerspec));
		rv = timer_settime(t->timer_id, 0, &in, NULL);
		OS_TRAP_IF(rv != 0);

		/* Start the timer again. */
		in.it_value.tv_nsec    = CLOCK_NSEC_M * t->interval;
		in.it_interval.tv_nsec = CLOCK_NSEC_M * t->interval;
		rv = timer_settime(t->timer_id, 0, &in, NULL);
		OS_TRAP_IF(rv != 0);
	}
	else {
		/* Indicate the entering of the barrier. */
		atomic_store(&t->suspended, 1);
	
		/* Suspend the timer owner. */
		os_sem_wait (&t->suspend);
	}

	/* Update the current start time of the transition. */
	rv = gettimeofday(&t->c_start, NULL);
	OS_TRAP_IF(rv != 0);

	/* Leave the critical section. */
	os_cs_leave(&tm.mutex);

	/* Calculate the return value. */
	return (overrun ? -1 : 0);
}

/**
 * os_clock_stop() - stop a periodic timer.
 *
 * @id:  assigned timer id by os_clock_init().
 *
 * Return:	None.
 **/
void os_clock_stop(int id)
{
        struct itimerspec in;
	struct tm_elem_s *t;
	int rv;
	
	/* Enter the critical section. */
	os_cs_enter(&tm.mutex);
	
	/* Entry condition. */
	OS_TRAP_IF(id < 0 || id >= OS_CLOCK_LIMIT);

	/* Get the pointer to the timer description. */
	t = &tm.elem[id];
	OS_TRAP_IF(! t->assigned);

	/* Remember the system end time. */
	rv = gettimeofday(&t->s_end, NULL);
	OS_TRAP_IF(rv != 0);

	/* Initialize the timer trigger. */
	os_memset(&in, 0, sizeof(struct itimerspec));

	/* If the timer was already armed, then the previous settings are
	 * overwritten. If the new values specifies a zero value (i.e., both
	 * subfields are zero, then the timer is disarmed. */

        /* Disarm the timer. No flags are set and no old values will be
	 * retrieved. */
        rv = timer_settime(t->timer_id, 0, &in, NULL);
	OS_TRAP_IF(rv != 0);

	/* Leave the critical section. */
	os_cs_leave(&tm.mutex);
}

/**
 * os_clock_start() - start a periodic timer.
 *
 * @id:  assigned timer id by os_clock_init().
 *
 * Return:	None.
 **/
void os_clock_start(int id)
{
        struct itimerspec in;
	struct tm_elem_s *t;
	int rv;
	
	/* Enter the critical section. */
	os_cs_enter(&tm.mutex);
	
	/* Entry condition. */
	OS_TRAP_IF(id < 0 || id >= OS_CLOCK_LIMIT);

	/* Get the pointer to the timer description. */
	t = &tm.elem[id];
	OS_TRAP_IF(! t->assigned);

	/* Initialize the timer trigger. */
	os_memset(&in, 0, sizeof(struct itimerspec));
	
        /* Timer expiration will occur withing n millisecons after being armed
         * by timer_settime(). */
        in.it_value.tv_nsec = CLOCK_NSEC_M * t->interval;
	
	/* Defines the input values for timer_settime(). The key input values is
	 * the interval with the base value 1 ms: CLOCK_NSEC_M0. 
	 * The substructures it_interval of the itimerspec structure allows a
	 * time value to be specified in seconds and nanoseconds. */
	in.it_interval.tv_nsec = CLOCK_NSEC_M * t->interval;

	/* Initialize the timer start. */
	rv = gettimeofday(&t->c_start, NULL);
	OS_TRAP_IF(rv != 0);
	t->min = t->c_start;
	
        /* Arm the timer. No flags are set and no old_value will be
	 * retrieved. */
        rv = timer_settime(t->timer_id, 0, &in, NULL);
	OS_TRAP_IF(rv != 0);

	/* Leave the critical section. */
	os_cs_leave(&tm.mutex);
}

/**
 * os_clock_delete() - remove a periodic timer.
 *
 * @id:  assigned timer id by os_clock_init().
 *
 * Return:	None.
 **/
void os_clock_delete(int id)
{
	struct tm_elem_s *t;
	int rv;
	
	/* Enter the critical section. */
	os_cs_enter(&tm.mutex);
	
	/* Entry condition. */
	OS_TRAP_IF(id < 0 || id >= OS_CLOCK_LIMIT);

	/* Get the pointer to the timer description. */
	t = &tm.elem[id];
	OS_TRAP_IF(! t->assigned);
	
	/* Disarm and delete the timer. */
	rv = timer_delete(t->timer_id);
	OS_TRAP_IF(rv != 0);

	/* Release the control semaphore. */
	os_sem_delete(&t->suspend);
	
	/* Release this timer. */
	t->assigned = 0;

	/* Leave the critical section. */
	os_cs_leave(&tm.mutex);
}

/**
 * os_clock_init() - create a periodic timer with a repeating interval of i
 * milliseconds.
 *
 * @name:      timer name.
 * @interval:  repeating interval in milliseconds.
 *
 * Return:	the timer id.
 **/
int os_clock_init(const char *name, int interval)
{
	struct tm_elem_s *t;
	int i, len, rv;
	
	/* Entry condition. */
	OS_TRAP_IF(name == NULL || interval < 1);
	
	/* Enter the critical section. */
	os_cs_enter(&tm.mutex);
	
	/* Search for a free wait element. */
	for (i = 0, t = tm.elem; i < OS_CLOCK_LIMIT; i++, t++) {
		if (! t->assigned)
			break;
	}

	/* Test the search result. */
	OS_TRAP_IF(i >= OS_CLOCK_LIMIT);

	/* This timer is yours. */
	os_memset(t, 0, sizeof(struct tm_elem_s));

	/* Update the timer state. */
	t->id = i;
	t->assigned = 1;
	t->interval = interval;
	os_sem_init(&t->suspend, 0);
	
	/* Save the timer name. */
	len = os_strlen(name);
	OS_TRAP_IF(len >= OS_MAX_NAME_LEN);
	os_memset(t->name, 0, OS_MAX_NAME_LEN);
	os_strcpy(t->name, OS_MAX_NAME_LEN, name);

	/* Install a periodic timer. */
	tm_create(t);
	
	/* Remember the system start time. */
	rv = gettimeofday(&t->s_start, NULL);
	OS_TRAP_IF(rv != 0);

	/* Leave the critical section. */
	os_cs_leave(&tm.mutex);

	return t->id;
}

/**
 * os_clock_exit() - release the timer resources.
 *
 * Return:	None.
 **/
void os_clock_exit_(void)
{
	struct tm_elem_s *t;
	int i;

	/* Enter the critical section. */
	os_cs_enter(&tm.mutex);
	
	/* Test the state of all timer. */
	for (i = 0, t = tm.elem; i < OS_CLOCK_LIMIT; i++, t++) {
		/* Test the timer state. */
		OS_TRAP_IF(t->assigned);
	}

	/* Leave the critical section. */
	os_cs_leave(&tm.mutex);
	
	/* Delete the mutex for the critical sections. */
	os_cs_destroy(&tm.mutex);
}

/**
 * os_clock_init() - initialize the timer resources.
 *
 * Return:	None.
 **/
void os_clock_init_(void)
{
	/* Create the mutex for the critical section in the timer operations. */
	os_cs_init(&tm.mutex);
}
