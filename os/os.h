/* SPDX-License-Identifier: GPL-2.0 */
#ifndef __os_h__
#define __os_h__

/*============================================================================
  IMPORTED INCLUDE REFERENCES
  ============================================================================*/

#include <semaphore.h>

/*============================================================================
  NAME CONSTANTS DEFINITIONS
  ============================================================================*/

/* Maximum length of contant OS strings. */
#define OS_MAX_STRING_LEN  8

/*============================================================================
  MACROS
  ============================================================================*/
/*============================================================================
  TYPE DEFINITIONS
  ============================================================================*/

/**
 * os_thread_prio_t - supported thread priorities.
 *
 * @OS_THREAD_PRIO_HARDRT:   hard realtime priority.
 * @OS_THREAD_PRIO_SOFTRT:   soft realtime priority.
 * @OS_THREAD_PRIO_BACKG:    background priority for applications.
 * @OS_THREAD_PRIO_FOREG:    foreground priority for applications.
 **/
typedef enum {
	OS_THREAD_PRIO_HARDRT  = 99,
	OS_THREAD_PRIO_SOFTRT  = 50,
	OS_THREAD_PRIO_BACKG   = 40,
	OS_THREAD_PRIO_FOREG   = 35,
	OS_THREAD_PRIO_DEFAULT = 5
} os_thread_prio_t;

/* States of a pthread. */
typedef enum {
	OS_THREAD_SUSPENDED,
	OS_THREAD_RUNNING,
	OS_THREAD_TERMINATED,
	OS_THREAD_FINISHED,
	OS_THREAD_DELETED,
	OS_THREAD_INVALID
} os_thread_state_t;

typedef pthread_mutex_t spinlock_t;

/**
 * os_thread_t - data for all thread interfaces. The caller must not modify any
 * data.
 *
 * @id:     number of the thread table entry.
 * @name:   name of the thread.
 * @attr:   thread attribute like SCHED_RR.
 * @prio:   thread priority.
 * @state:  current state of the pthread.
 **/
typedef struct {
	int                id;
	char               name[OS_MAX_STRING_LEN];
	pthread_attr_t     attr;
	os_thread_prio_t   prio;
	os_thread_state_t  state;
} os_thread_t;

/*============================================================================
  GLOBAL DATA
  ============================================================================*/
/*============================================================================
  EXPORTED FUNCTIONS
  ============================================================================*/

void os_sem_create(sem_t *sem, unsigned int init_value);
void os_sem_wait(sem_t *sem);
void os_sem_delete(sem_t *sem);

void os_spinlock_init(spinlock_t *spinlock);
void os_spinlock_obtain(spinlock_t *spinlock);
void os_spinlock_release(spinlock_t *spinlock);
void os_spinlock_delete(spinlock_t *spinlock);

void os_thread_start(os_thread_t *thread, const char *name, os_thread_prio_t prio);

#endif /* __os_h__ */
