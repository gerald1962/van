// SPDX-License-Identifier: GPL-2.0

/*
 * Exception handling
 *
 * Copyright (C) 2021 Gerald Schueller <gerald.schueller@web.de>
 */

/*============================================================================
  IMPORTED INCLUDE REFERENCES
  ============================================================================*/

#include <stdio.h>   /* Standard C library: printf(). */
#include <signal.h>  /* Signal handling of the OS: signal() */

/*============================================================================
  EXPORTED INCLUDE REFERENCES
  ============================================================================*/
#include "trap.h"    /* Exception handling: trap_signal_catch() */

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
 * trap() - force a core dump.
 *
 * @file:      matches __FILE__.
 * @line:      matches __LINE__.
 * @function:  matches __FUNCTION__.
 *
 * Return:	None.
 **/
void trap(char *file, const char *function, unsigned long line)
{
	printf ("*** core dump at \"%s\", \"%s\", %lu\n", file, function, line);

	/* Force a core dump. */
	raise (SIGABRT);
}


/**
 * trap_signal_handler() - catch the signal SIGINT to generate a core dump.
 *
 * @signo:  Signal number like SIGINT.
 *
 * Return:	None.
 **/
void trap_signal_handler(int signo)
{
        /* Catch Ctrl-C. */
        if (signo == SIGINT) {
                printf ("%s: received SIGINT, generate a core dump.\n", F);
                raise (SIGABRT);
        }
}


/**
 * trap_signal_catch() - Install a signal handler to generate a core dump, if
 * the programm has been terminated with Ctrl-C.
 *
 * Return:	None.
 **/
void trap_signal_catch(void)
{
        /* Install the signal handler to cath SIGINT. */
        if (signal(SIGINT, trap_signal_handler) == SIG_ERR)
                TRAP();

}
