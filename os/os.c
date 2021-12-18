// SPDX-License-Identifier: GPL-2.0

/*
 * Operatin system interfaces.
 *
 * Copyright (C) 2021 Gerald Schueller <gerald.schueller@web.de>
 */

/*============================================================================
  IMPORTED INCLUDE REFERENCES
  ============================================================================*/
#include <pthread.h>     /* POSIX thread. */

/*============================================================================
  EXPORTED INCLUDE REFERENCES
  ============================================================================*/
#include "os.h"          /* Operating system: os_sem_init() */
#include "os_private.h"  /* Local interfaces of the OS: os_trap_init() */

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

/* OS configuration. */
static os_conf_t os_conf;

/**
 * os_stat - overall state of the OS.
 *
 * @is_init:     1, if the OS has been initialized.
 * @cs_count:    number of created mutexes.
 * @sem_count:   number of created semaphores.
 * @spin_count:  number of created spin locks.
 **/
static struct os_stat_s {
	atomic_int is_init;
	atomic_int cs_count;
	atomic_int sem_count;
	atomic_int spin_count;
} os_stat;

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
	OS_TRAP_IF(mutex == NULL);
	
	ret = pthread_mutexattr_init(&attr);
	OS_TRAP_IF(ret != 0);
	
	ret = pthread_mutexattr_settype(&attr, PTHREAD_MUTEX_RECURSIVE);
	OS_TRAP_IF(ret != 0);
	
	ret = pthread_mutex_init(mutex, &attr);
	OS_TRAP_IF(ret != 0);
	
	ret = pthread_mutexattr_destroy(&attr);
	OS_TRAP_IF(ret != 0);

	atomic_fetch_add(&os_stat.cs_count, 1);
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
	OS_TRAP_IF(mutex == NULL);
	
	ret = pthread_mutex_lock(mutex);
	
	/* Final condition. */
	OS_TRAP_IF(ret != 0);
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
	OS_TRAP_IF(mutex == NULL);
	
	ret = pthread_mutex_unlock(mutex);
	
	/* Final condition. */
	OS_TRAP_IF(ret != 0);
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
	OS_TRAP_IF(mutex == NULL);
	
	ret = pthread_mutex_destroy(mutex);
	
	/* Final condition. */
	OS_TRAP_IF(ret != 0);
	
	atomic_fetch_sub(&os_stat.cs_count, 1);
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
	OS_TRAP_IF(sem == NULL);

	ret = sem_init(sem, 0, init_value);

	/* Final condition. */
	OS_TRAP_IF(ret != 0);

	atomic_fetch_add(&os_stat.sem_count, 1);	
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
	OS_TRAP_IF(sem == NULL);

	ret = sem_wait(sem);

	/* Final condition. */
	OS_TRAP_IF(ret != 0);
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
	OS_TRAP_IF(sem == NULL);

	ret = sem_post(sem);

	/* Final condition. */
	OS_TRAP_IF(ret != 0);
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
	OS_TRAP_IF (sem == NULL);

	ret = sem_destroy (sem);

	/* Final condition. */
	OS_TRAP_IF (ret != 0);
	
	atomic_fetch_sub(&os_stat.sem_count, 1);	
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
	OS_TRAP_IF(spinlock == NULL);

#if defined(USE_PTHREAD_SPIN)
	ret = pthread_spin_init(spinlock, PTHREAD_PROCESS_SHARED);
#else
	ret = pthread_mutex_init(spinlock, NULL);
#endif
	/* Final condition. */
	OS_TRAP_IF(ret != 0);
	
	atomic_fetch_add(&os_stat.spin_count, 1);
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
	OS_TRAP_IF(spinlock == NULL);

#if defined(USE_PTHREAD_SPIN)
	ret = pthread_spin_lock(spinlock);
#else
	ret = pthread_mutex_lock(spinlock);
#endif
	/* Final condition. */
	OS_TRAP_IF(ret != 0);
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
	OS_TRAP_IF(spinlock == NULL);
	
#if defined(USE_PTHREAD_SPIN)
	ret = pthread_spin_unlock(spinlock);
#else
	ret = pthread_mutex_unlock(spinlock);
#endif
	/* Final condition. */
	OS_TRAP_IF(ret != 0);
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
	OS_TRAP_IF(spinlock == NULL);
	
#if defined(USE_PTHREAD_SPIN)
	ret = pthread_spin_destroy(spinlock);
#else
	ret = pthread_mutex_destroy(spinlock);
#endif
	/* Final condition. */
	OS_TRAP_IF(ret != 0);
	
	atomic_fetch_sub(&os_stat.spin_count, 1);
}

/**
 * os_statistics() - provide data on the OS state.
 *
 * @stat:  address of the status information.
 *
 * Return:	None.
 **/
void os_statistics(os_statistics_t *stat)
{
	int is_init;
	
	/* Test the OS state. */
	is_init = atomic_load(&os_stat.is_init);
	OS_TRAP_IF(! is_init);
	
	/* Entry condition. */
	OS_TRAP_IF(stat == NULL);
	
	/* Get information about the thread state. */
	os_thread_statistics(stat);
	
	/* Get information about the memory state. */
	os_mem_statistics(stat);

	/* Complete the data gathering. */
	stat->cs_count   = os_stat.cs_count;
	stat->sem_count  = os_stat.cs_count;
	stat->spin_count = os_stat.spin_count;
}

/**
 * os_trace_button() - change the trace configuration.
 *
 * @n:  if n eq. 0, trace off, else trace on.
 *
 * Return:	None.
 **/
void os_trace_button(int n)
{
	os_conf.trace_stat = n;
}

/**
 * os_init() - initialize the operation system.
 *
 * Return:	None.
 **/
void os_init(void)
{
	int is_init;
	
	/* Test the OS state. */
	is_init = atomic_load(&os_stat.is_init);
	atomic_store(&os_stat.is_init, 1);
	OS_TRAP_IF(is_init);

	/* Switch on the trace. */
	os_conf.trace_stat = 1;
	
	/* Install a signal handler to generate a core dump, if the test
         * programm has been terminated with Ctrl-C. */
        os_trap_init(&os_conf);

	/* Initialize the os_malloc list. */
	os_mem_init();
	
	/* Initialize the thread table. */
	os_thread_init(&os_conf);

	/* Initialize the shared memory devices. */
	os_shm_init(&os_conf);
}

/**
 * os_exit() - release the OS resources.
 *
 * Return:	None.
 **/
void os_exit(void)
{
	struct os_stat_s *p;
	int is_init;

	/* Test the OS state. */
	p = &os_stat;	
	is_init = atomic_load(&p->is_init);
	OS_TRAP_IF(! is_init);

	os_shm_exit();
	os_thread_exit();
	os_mem_exit();

	/* Test the OS state. */
	OS_TRAP_IF(p->cs_count != 0 || p->sem_count != 0 ||  p->spin_count != 0);

	/* Change the OS state. */
	atomic_store(&p->is_init, 0);
}
