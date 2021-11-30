// SPDX-License-Identifier: GPL-2.0

/*
 * XXX
 *
 * Copyright (C) 2021 Gerald Schueller <gerald.schueller@web.de>
 */

/*============================================================================
  IMPORTED INCLUDE REFERENCES
  ============================================================================*/
#include "os.h"      /* Operating system: os_sem_create(). */

/*============================================================================
  EXPORTED INCLUDE REFERENCES
  ============================================================================*/
/*============================================================================
  LOCAL NAME CONSTANTS DEFINITIONS
  ============================================================================*/

/* Prompt of the research programme. */
#define P  "S:"

/*============================================================================
  MACROS
  ============================================================================*/
/*============================================================================
  LOCAL TYPE DEFINITIONS
  ============================================================================*/
/*============================================================================
  LOCAL DATA
  ============================================================================*/

/** 
 * shm_stat - state of the research project.
 *
 * @suspend:  suspend the main process while the test is running.
 * @con:      reference to the controller thread.
 **/
static struct shm_stat_s {
	sem_t   suspend;
	void    *con;
} shm_stat;


/*============================================================================
  LOCAL FUNCTION PROTOTYPES
  ============================================================================*/
/*============================================================================
  LOCAL FUNCTIONS
  ============================================================================*/

/**
 * shm_cleanup() - free the resources of the research programme.
 *
 * Return:	None.
 **/
static void shm_cleanup(void)
{
	void *con;
	
	printf("%s [p=main,s=exit,o=cleanup]\n", P);

	/* Destroy the controller thread. */
	con = shm_stat.con;
	shm_stat.con = NULL;
	os_thread_destroy(con);
	os_sem_delete(&shm_stat.suspend);

	/* Release the OS resources. */
	os_exit();
}


/**
 * shm_wait() - suspend the main process and wait for the exit message.
 *
 * Return:	None.
 **/
static void shm_wait(void)
{
	printf("%s [p=main,s=done,o=suspend]\n", P);
	
	/* Suspend the main process. */
	os_sem_wait(&shm_stat.suspend);
	
	printf("%s [p=main,s=exit,o=resume]\n", P);
}

/**
 * shm_exit_exec() - execute the exit message in the controller context.
 *
 * @msg:  reference to the generic input message.
 *
 * Return:	None.
 **/
static void shm_exit_exec(os_queue_elem_t *msg)
{
	printf("%s [t=con,s=jobs,m=exit]\n", P);

	/* Entry condition. */
	OS_TRAP_IF(msg == NULL || msg->param != shm_stat.con);

	/* Resume the main thread. */
	os_sem_release(&shm_stat.suspend);
}

/**
 * shm_exit_send() - send the exit message to controller thread.
 *
 * Return:	None.
 **/
static void shm_exit_send(void)
{
	os_queue_elem_t msg;
	
	/* Send a test message. */
	os_memset(&msg, 0, sizeof(msg));
	msg.param = shm_stat.con;
	msg.cb    = shm_exit_exec;
	os_queue_send(shm_stat.con, &msg, sizeof(msg));
}

/**
 * shm_init() - iniialize the research programme.
 *
 * Return:	None.
 **/
static void shm_init(void)
{
	printf("%s [p=main,s=boot,o=init]\n", P);

	/* Initialize the operating system. */
	os_init();

	/* Control the lifetime of the research programme. */
	os_sem_init(&shm_stat.suspend, 0);

	/* Create and start the controller thread. */
	shm_stat.con = os_thread_create("con", OS_THREAD_PRIO_FOREG, 2);

	/* Send the exit message to the controller thread. */
	shm_exit_send();
	
	/* Wait for the exit message. */
	shm_wait();	
}


/*============================================================================
  EXPORTED FUNCTIONS
  ============================================================================*/

/**
 * main() - start function of the research programme.
 *
 * Return:	0 or force a software trap.
 **/
int main(void)
{
	/* Iniialize the research progamme. */
	shm_init();

	/* Free all resources. */
	shm_cleanup();
	
	return (0);
}
