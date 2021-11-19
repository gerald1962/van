// SPDX-License-Identifier: GPL-2.0

/*
 * Operatin system interfaces.
 *
 * Copyright (C) 2021 Gerald Schueller <gerald.schueller@web.de>
 */

/*============================================================================
  IMPORTED INCLUDE REFERENCES
  ============================================================================*/
#include <stdio.h>   /* Standard C library: NULL. */
#include <pthread.h> /* POSIX thread. */
#include "trap.h"    /* Exception handling: TRAP */

/*============================================================================
  EXPORTED INCLUDE REFERENCES
  ============================================================================*/
#include "os.h"      /* Operating system: os_sem_init() */

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
  LOCAL DATA
  ============================================================================*/
/*============================================================================
  LOCAL FUNCTION PROTOTYPES
  ============================================================================*/
/*============================================================================
  LOCAL FUNCTIONS
  ============================================================================*/
/*============================================================================
  EXPORTED FUNCTIONS
  ============================================================================*/

/**
 * os_cs_init() - initialize the mutex.
 *
 * @mutex:  address of the mutex.
 *
 * Return:	None.
 **/
void os_cs_init(pthread_mutex_t *mutex)
{
	pthread_mutexattr_t attr;
	int ret;
	
	/* Entry condition. */
	TRAP_IF(mutex == NULL);
	
	ret = pthread_mutexattr_init(&attr);
	TRAP_IF(ret != 0);
	
	ret = pthread_mutexattr_settype(&attr, PTHREAD_MUTEX_RECURSIVE);
	TRAP_IF(ret != 0);
	
	ret = pthread_mutex_init(mutex, &attr);
	TRAP_IF(ret != 0);
	
	ret = pthread_mutexattr_destroy(&attr);
	TRAP_IF(ret != 0);
}


/**
 * os_cs_enter() - enter the critical section.
 *
 * @mutex:  address of the mutex.
 *
 * Return:	None.
 **/
void os_cs_enter(pthread_mutex_t *mutex)
{
	int ret;
	
	/* Entry condition. */
	TRAP_IF(mutex == NULL);
	
	ret = pthread_mutex_lock(mutex);
	
	/* Final condition. */
	TRAP_IF(ret != 0);
}

/**
 * os_cs_leave() - leave the critical section.
 *
 * @mutex:  address of the mutex.
 *
 * Return:	None.
 **/
void os_cs_leave(pthread_mutex_t *mutex)
{
	int ret;
	
	/* Entry condition. */
	TRAP_IF(mutex == NULL);
	
	ret = pthread_mutex_unlock(mutex);
	
	/* Final condition. */
	TRAP_IF(ret != 0);
}

/**
 * os_cs_destroy() - delete the mutex.
 *
 * @mutex:  address of the mutex.
 *
 * Return:	None.
 **/
void os_cs_destroy(pthread_mutex_t *mutex)
{
	int ret;
	
	/* Entry condition. */
	TRAP_IF(mutex == NULL);
	
	ret = pthread_mutex_destroy(mutex);
	
	/* Final condition. */
	TRAP_IF(ret != 0);
}

/**
 * os_sem_init() - initialize the semaphore.
 *
 * @sem:         address of the semaphore.
 * @init_value:  inital value of the semaphore counter.
 *
 * Return:	None.
 **/
void os_sem_init(sem_t *sem, unsigned int init_value)
{
	int ret;
	
	/* Entry condition. */
	TRAP_IF(sem == NULL);

	ret = sem_init(sem, 0, init_value);

	/* Final condition. */
	TRAP_IF(ret != 0);
}

/**
 * os_sem_wait() - suspend the current thread.
 *
 * @sem:         address of the semaphore.
 *
 * Return:	None.
 **/
void os_sem_wait(sem_t *sem)
{
	int ret;
	
	/* Entry condition. */
	TRAP_IF(sem == NULL);

	ret = sem_wait(sem);

	/* Final condition. */
	TRAP_IF(ret != 0);
}

/**
 * os_sem_release() - release the current thread.
 *
 * @sem:         address of the semaphore.
 *
 * Return:	None.
 **/
void os_sem_release(sem_t *sem)
{
	int ret;
	
	/* Entry condition. */
	TRAP_IF(sem == NULL);

	ret = sem_post(sem);

	/* Final condition. */
	TRAP_IF(ret != 0);
}

/**
 * os_sem_delete() - destroy the semaphore.
 *
 * @sem:         address of the semaphore.
 *
 * Return:	None.
 **/
void os_sem_delete(sem_t *sem)
{
	int ret;
	
	/* Entry condition. */
	TRAP_IF (sem == NULL);

	ret = sem_destroy (sem);

	/* Final condition. */
	TRAP_IF (ret != 0);
}


/**
 * os_spin_init() - initialize the spinlock.
 *
 * @sem:         address of the spinlock.
 *
 * Return:	None.
 **/
void os_spin_init(spinlock_t *spinlock)
{
	int ret;
	
	/* Entry condition. */
	TRAP_IF(spinlock == NULL);

	ret = pthread_mutex_init(spinlock, NULL);

	/* Final condition. */
	TRAP_IF(ret != 0);
}


/**
 * os_spin_lock() - aquire an atomic lock.
 *
 * @sem:         address of the spinlock.
 *
 * Return:	None.
 **/
void os_spin_lock(spinlock_t *spinlock)
{
	int ret;
	
	/* Entry condition. */
	TRAP_IF(spinlock == NULL);

	ret = pthread_mutex_lock(spinlock);

	/* Final condition. */
	TRAP_IF(ret != 0);
}

/**
 * os_spin_unlock() - unlock the spinlock.
 *
 * @sem:         address of the spinlock.
 *
 * Return:	None.
 **/
void os_spin_unlock(spinlock_t *spinlock)
{
	int ret;
	
	/* Entry condition. */
	TRAP_IF(spinlock == NULL);

	ret = pthread_mutex_unlock(spinlock);

	/* Final condition. */
	TRAP_IF(ret != 0);
}

/**
 * os_spin_destroy() - destroy the spinlock.
 *
 * @sem:         address of the spinlock.
 *
 * Return:	None.
 **/
void os_spin_destroy(spinlock_t *spinlock)
{
	int ret;
	
	/* Entry condition. */
	TRAP_IF(spinlock == NULL);

	ret = pthread_mutex_destroy(spinlock);

	/* Final condition. */
	TRAP_IF(ret != 0);
}

/**
 * os_init() - initialize the operation system.
 *
 * Return:	None.
 **/
void os_init(void)
{
	/* Install a signal handler to generate a core dump, if the test
         * programm has been terminated with Ctrl-C. */
        trap_signal_catch();

	/* Initialize the thread table. */
	os_thread_init();
}
