/* SPDX-License-Identifier: GPL-2.0 */
#ifndef __trap_h__
#define __trap_h__

/*============================================================================
  IMPORTED INCLUDE REFERENCES
  ============================================================================*/
/*============================================================================
  NAME CONSTANTS DEFINITIONS
  ============================================================================*/

/* Name of the current funtion. */
#define F  __FUNCTION__

/*============================================================================
  MACROS
  ============================================================================*/

/* Trigger a core dump */
#define TRAP() \
    trap(__FILE__, F, __LINE__)

#define TRAP_IF(cond) \
    do { \
        if (cond) { \
            TRAP(); \
        } \
    } while (0)

/*============================================================================
  TYPE DEFINITIONS
  ============================================================================*/
/*============================================================================
  GLOBAL DATA
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
void trap(char *file, const char *function, unsigned long line);

/**
 * trap_signal_catch() - Install a signal handler to generate a core dump, if
 * the programm has been terminated with Ctrl-C.
 *
 * Return:	None.
 **/
void trap_signal_catch(void);

#endif /* __trap_h__ */
