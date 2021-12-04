/* SPDX-License-Identifier: GPL-2.0 */
#ifndef __os_private_h__
#define __os_private_h__

/*============================================================================
  IMPORTED INCLUDE REFERENCES
  ============================================================================*/
#include <stdatomic.h>  /* ISO C11 Standard:  7.17  Atomics */

/*============================================================================
  NAME CONSTANTS DEFINITIONS
  ============================================================================*/

/* OS prompt. */
#define OS  "O>"

/*============================================================================
  MACROS
  ============================================================================*/

/* OS trace routine. */
#define OS_TRACE(info_)  do { \
		if (os_conf_p && os_conf_p->trace_stat) \
			printf info_; \
} while (0)

/*============================================================================
  TYPE DEFINITIONS
  ============================================================================*/
/**
 * os_conf_t - OS configuration.
 *
 * trace_stat:  if 1, suppres trace outputs.
 **/
typedef struct {
	int trace_stat;
} os_conf_t;

/*============================================================================
  GLOBAL DATA
  ============================================================================*/
/*============================================================================
  EXPORTED FUNCTIONS
  ============================================================================*/

/* Data gathering. */
void os_thread_statistics(os_statistics_t *stat);
void os_mem_statistics(os_statistics_t *stat);

/* Bootstrapping. */
void os_thread_init(os_conf_t *conf);
void os_mem_init(void);
void os_trap_init(os_conf_t *conf);

/* Free the OS resources. */
void os_thread_exit(void);
void os_mem_exit(void);

#endif /* __os_private_h__ */
