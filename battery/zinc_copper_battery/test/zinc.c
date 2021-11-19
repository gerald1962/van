// SPDX-License-Identifier: GPL-2.0

/*
 * XXX
 *
 * Copyright (C) 2021 Gerald Schueller <gerald.schueller@web.de>
 */

/*============================================================================
  IMPORTED INCLUDE REFERENCES
  ============================================================================*/
#include <stdio.h>   /* Standard C library: printf(). */
#include "trap.h"    /* Exception handling: TRAP */
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
 * @suspend:   suspend the start thread as long as the battery exists.
 * @spinlock:  spinlock test.
 * thread:     thread data.
 **/
static struct {
	sem_t        suspend;
#if 1
	spinlock_t   spinlock;
#endif
	os_thread_t  thread;
} zinc_cs;


/*============================================================================
  LOCAL FUNCTION PROTOTYPES
  ============================================================================*/
/*============================================================================
  LOCAL FUNCTIONS
  ============================================================================*/

/**
 * zinc_boot() - iniialize the battery management.
 *
 * Return:	None.
 **/
static void zinc_boot(void)
{
	/* Initialize the operation system. */
	os_init();

	/* Control the lifetime of the battery. */
	os_sem_init(&zinc_cs.suspend, 0);

#if 1
	/* Test the spinlock interfaces. */
	os_spin_init(&zinc_cs.spinlock);
	os_spin_lock(&zinc_cs.spinlock);
	os_spin_unlock(&zinc_cs.spinlock);
	os_spin_destroy(&zinc_cs.spinlock);
#endif

	/* Create the test thread. */
	os_thread_create(&zinc_cs.thread, "zinc", OS_THREAD_PRIO_FOREG);
	
	/* Suspend the main process. */
	os_sem_wait(&zinc_cs.suspend);
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
	printf("%s zinc copper battery\n", P);

	/* Iniialize the battery management. */
	zinc_boot();

	return (0);
}
