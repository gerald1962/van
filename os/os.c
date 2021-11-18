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
#include "os.h"      /* Operating system: os_sem_create() */

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
 * os_sem_create() - initialize the semaphore.
 *
 * @sem:         address of the semaphore.
 * @init_value:  inital value of the semaphore counter.
 *
 * Return:	None.
 **/
void os_sem_create(sem_t *sem, unsigned int init_value)
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
 * os_spinlock_init() - initialize the spinlock.
 *
 * @sem:         address of the spinlock.
 *
 * Return:	None.
 **/
void os_spinlock_init(spinlock_t *spinlock)
{
	int ret;
	
	/* Entry condition. */
	TRAP_IF(spinlock == NULL);

	ret = pthread_mutex_init(spinlock, NULL);

	/* Final condition. */
	TRAP_IF(ret != 0);
}


/**
 * os_spinlock_obtain() - aquire an atomic lock.
 *
 * @sem:         address of the spinlock.
 *
 * Return:	None.
 **/
void os_spinlock_obtain(spinlock_t *spinlock)
{
	int ret;
	
	/* Entry condition. */
	TRAP_IF(spinlock == NULL);

	ret = pthread_mutex_lock(spinlock);

	/* Final condition. */
	TRAP_IF(ret != 0);
}


/**
 * os_spinlock_release() - unlock the spinlock.
 *
 * @sem:         address of the spinlock.
 *
 * Return:	None.
 **/
void os_spinlock_release(spinlock_t *spinlock)
{
	int ret;
	
	/* Entry condition. */
	TRAP_IF(spinlock == NULL);

	ret = pthread_mutex_unlock(spinlock);

	/* Final condition. */
	TRAP_IF(ret != 0);
}


/**
 * os_spinlock_delete() - destroy the spinlock.
 *
 * @sem:         address of the spinlock.
 *
 * Return:	None.
 **/
void os_spinlock_delete(spinlock_t *spinlock)
{
	int ret;
	
	/* Entry condition. */
	TRAP_IF(spinlock == NULL);

	ret = pthread_mutex_destroy(spinlock);

	/* Final condition. */
	TRAP_IF(ret != 0);
}
