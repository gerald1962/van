// SPDX-License-Identifier: GPL-2.0

/*
 * Exception handling
 *
 * Copyright (C) 2021 Gerald Schueller <gerald.schueller@web.de>
 */

/*============================================================================
  IMPORTED INCLUDE REFERENCES
  ============================================================================*/
#include <signal.h>  /* Signal handling of the OS: signal() */
#include "os.h"      /* Operating system: os_sem_create() */

/*============================================================================
  EXPORTED INCLUDE REFERENCES
  ============================================================================*/
#include "os_boot.h"  /* OS bootstrapping: os_trap_init() */

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

/**
 * os_trap_handler() - catch the signal SIGINT to generate a core dump.
 *
 * @signo:  Signal number like SIGINT.
 *
 * Return:	None.
 **/
void os_trap_handler(int signo)
{
        /* Catch Ctrl-C. */
        if (signo == SIGINT) {
                printf("%s: received SIGINT, generate a core dump.\n", F);
                raise(SIGABRT);
        }
}

/**
 * trap_signal_catch() - Install the signal handler to generate a core dump, if
 * the programm has been terminated with Ctrl-C.
 *
 * Return:	None.
 **/
void os_trap_catch(void)
{
        /* Install the signal handler to cath SIGINT. */
        if (signal(SIGINT, os_trap_handler) == SIG_ERR)
                OS_TRAP();
}

/*============================================================================
  EXPORTED FUNCTIONS
  ============================================================================*/

/**
 * os_trap() - force a core dump.
 *
 * @file:      matches __FILE__.
 * @line:      matches __LINE__.
 * @function:  matches __FUNCTION__.
 *
 * Return:	None.
 **/
void os_trap(char *file, const char *function, unsigned long line)
{
	printf("*** core dump at \"%s\", \"%s\", %lu\n", file, function, line);

	/* Force a core dump. */
	raise(SIGABRT);
}


/**
 * os_trap_init() - install the signal handler for the core dump.
 *
 * Return:	None.
 **/
void os_trap_init(void)
{
	/* Install the signal handler for the core dump. */
	os_trap_catch();
}
