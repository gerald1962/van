/* SPDX-License-Identifier: GPL-2.0 */
#ifndef __csd_chart_h__
#define __csd_chart_h__

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
/**
 * serv_msg_t - input messages of the server.
 *
 * @SERV_OP_INIT_IND_M:   init indication from the operator.
 * @SERV_CLI_DOWN_IND_M:  down indication from the client.
 **/
typedef enum {
	SERV_OP_INIT_IND_M,
	SERV_CLI_DOWN_IND_M,
	SERV_COUNT_M
} serv_msg_t;

/**
 * cli_msg_t - input messages of the client.
 *
 * @CLI_SERV_UP_IND_M:  up indication from the server.
 **/
typedef enum {
	CLI_SERV_UP_IND_M,
	CLI_COUNT_M
} cli_msg_t;

/*============================================================================
  GLOBAL DATA
  ============================================================================*/
/*============================================================================
  EXPORTED FUNCTIONS
  ============================================================================*/

/* ======================= Exported server interfaces ======================= */

/* operater -> server calls */
void serv_op_init(void);
void serv_op_exit(void);

void serv_send(serv_msg_t id, os_queue_elem_t *msg, int size);

/* ======================= Exported client interfaces ======================= */

/* operater -> client calls */
void cli_op_init(void);
void cli_op_exit(void);
	
void cli_send(cli_msg_t id, os_queue_elem_t *msg, int size);

/* ======================= Exported operator interface ====================== */

/* server -> operator call */
void op_resume(void);

#endif /* __csd_chart_h__ */
