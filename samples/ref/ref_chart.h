/* SPDX-License-Identifier: GPL-2.0 */
#ifndef __ref_chart_h__
#define __ref_chart_h__

/*============================================================================
  IMPORTED INCLUDE REFERENCES
  ============================================================================*/
#include "os.h"      /* Operating system: os_sem_create(). */

/*============================================================================
  NAME CONSTANTS DEFINITIONS
  ============================================================================*/
/*============================================================================
  MACROS
  ============================================================================*/
/*============================================================================
  TYPE DEFINITIONS
  ============================================================================*/
/*============================================================================
  GLOBAL DATA
  ============================================================================*/
/*============================================================================
  EXPORTED FUNCTIONS
  ============================================================================*/

/* ======================= Exported server interfaces ======================= */

/* operater -> server calls */
void serv_op_init(void *server, void *client);
void serv_op_exit(void);

/* operater -> server message */
void serv_op_init_ind_exec(os_queue_elem_t *msg);
/* client -> server message */
void serv_cli_down_ind_exec(os_queue_elem_t *msg);

/* ======================= Exported client interfaces ======================= */

/* operater -> client calls */
void cli_op_init(void *server, void *client);
void cli_op_exit(void);
	
/* server -> client message */
void cli_serv_up_ind_exec(os_queue_elem_t *msg);

/* ======================= Exported operator interface ====================== */

/* server -> operator call */
void op_resume(void);

#endif /* __ref_chart_h__ */
