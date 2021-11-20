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

/* Prompt of zinc. */
#define P  "Z>"

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
 * zinc_cs - State of the battery program
 *
 * @suspend:  suspend the start thread as long as the battery exists.
 * @bat:      reference to the bat thread.
 **/
static struct zinc_cs_s {
	sem_t   suspend;
	void    *bat;
} zinc_cs;


/*============================================================================
  LOCAL FUNCTION PROTOTYPES
  ============================================================================*/
/*============================================================================
  LOCAL FUNCTIONS
  ============================================================================*/

/**
 * zinc_cleanup() - free the resources of the battery management.
 *
 * Return:	None.
 **/
static void zinc_cleanup(void)
{
	printf("%s [main,cleanup]\n", P);
}


/**
 * zinc_wait() - suspend the main thread and wait for the exit message.
 *
 * Return:	None.
 **/
static void zinc_wait(void)
{
	printf("%s [main,suspend]\n", P);
	
	/* Suspend the main process. */
	os_sem_wait(&zinc_cs.suspend);
	
	printf("%s [main,resume]\n", P);
}

/**
 * zinc_exit_exec() - execute the exit message in the bat context.
 *
 * @msg:  reference to the generic input message.
 *
 * Return:	None.
 **/
static void zinc_exit_exec(os_queue_elem_t *msg)
{
	struct zinc_cs_s  *zinc;
	
	printf("%s [bat,exit]\n", P);

	/* Decode the reference to the zinc state. */
	zinc = msg->param;

	/* Resume the main thread. */
	os_sem_release(&zinc->suspend);
}

/**
 * zinc_exit_send() - send the exit message to bat thread.
 *
 * Return:	None.
 **/
static void zinc_exit_send(void)
{
	os_queue_elem_t msg;
	
	/* Send a test message. */
	os_memset(&msg, 0, sizeof(msg));
	msg.param = &zinc_cs;
	msg.cb    = zinc_exit_exec;
	os_queue_send(zinc_cs.bat, &msg, sizeof(msg));
}

/**
 * zinc_init() - iniialize the battery management.
 *
 * Return:	None.
 **/
static void zinc_init(void)
{
	printf("%s [main,init]\n", P);

	/* Initialize the operating system. */
	os_init();

	/* Control the lifetime of the battery. */
	os_sem_init(&zinc_cs.suspend, 0);

	/* Create the bat thread. */
	zinc_cs.bat = os_thread_create("bat", OS_THREAD_PRIO_FOREG, 2);

	/* Start the bat thread. */
	os_thread_start(zinc_cs.bat);

	/* Send the exit message to the bat thread. */
	zinc_exit_send();
	
	/* Wait for the exit message. */
	zinc_wait();	
}


/*============================================================================
  EXPORTED FUNCTIONS
  ============================================================================*/

/**
 * main() - start function of the zinc copper battery.
 *
 * Return:	0 or force a software trap.
 **/
int main(void)
{
	/* Iniialize the battery management. */
	zinc_init();

	/* Free all resources. */
	zinc_cleanup();
	
	return (0);
}
