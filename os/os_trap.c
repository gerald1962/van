// SPDX-License-Identifier: GPL-2.0

/*
 * Exception handling
 *
 * Copyright (C) 2021 Gerald Schueller <gerald.schueller@web.de>
 */

/*============================================================================
  IMPORTED INCLUDE REFERENCES
  ============================================================================*/
#include <signal.h>      /* Signal handling of the OS: signal() */
#include "os.h"          /* Operating system: os_sem_create() */

/*============================================================================
  EXPORTED INCLUDE REFERENCES
  ============================================================================*/
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

/* Pointer to the OS configuration */
static os_conf_t *os_conf_p;

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
		
		/* Release critical shared memory resources. */
		os_cab_ripcord(0);
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
	__sighandler_t ret;
	
        /* Install the signal handler to catch SIGINT. */
        ret = signal(SIGINT, os_trap_handler);
	OS_TRAP_IF(ret == SIG_ERR);
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
	/* Entry condition. */
	if (file != NULL && function != NULL) {
		if (os_strcmp(function, "*coverage*") == 0) {
			OS_TRACE(("*** coverage test at \"%s\", \"%s\", %lu\n",
				  file, function, line));

			/* Test the release of critical shared memory resources. */
			os_cab_ripcord(1);

			/* Test the trap handler. */
			os_trap_handler(SIGUSR1);
			return;
		}
		else {
			OS_TRACE(("*** core dump at \"%s\", \"%s\", %lu\n",
				  file, function, line));
		}
	}

	/* Release critical shared memory resources. */
	os_cab_ripcord(0);

	/* Force a core dump. */
	raise(SIGABRT);
}


/**
 * os_trap_init() - install the signal handler for the core dump.
 *
 * Return:	None.
 **/
void os_trap_init(os_conf_t *conf)
{
	/* Save the reference to the OS configuration. */
	os_conf_p = conf;
	
	/* Install the signal handler for the core dump. */
	os_trap_catch();
}
