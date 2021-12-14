/* SPDX-License-Identifier: GPL-2.0 */

/*
 * sac_chart.h
 *
 * Copyright (C) 2021 Gerald Schueller <gerald.schueller@web.de>
 */

#ifndef __sac_chart_h__
#define __sac_chart_h__

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
 * van_msg_t - input messages of van.
 *
 * @VAN_OP_INIT_IND_M:  init indication from the operator.
 * @VAN_PY_DOWN_IND_M:  down indication from py.
 **/
typedef enum {
	VAN_OP_INIT_IND_M,
	VAN_PY_DOWN_IND_M,
	VAN_COUNT_M
} van_msg_t;

/**
 * py_msg_t - input messages of py.
 *
 * @PY_VAN_UP_IND_M:  up indication from van.
 **/
typedef enum {
	PY_VAN_UP_IND_M,
	PY_COUNT_M
} py_msg_t;

/*============================================================================
  GLOBAL DATA
  ============================================================================*/
/*============================================================================
  EXPORTED FUNCTIONS
  ============================================================================*/

/* ======================= Exported van interfaces ======================= */

/* operater -> van calls */
void van_op_init(void);
void van_op_exit(void);

void van_send(van_msg_t id, os_queue_elem_t *msg, int size);
#define SRV_SEND(id_, msg_, size_) do { \
        van_send((id_), (os_queue_elem_t *) (msg_), (size_)); \

/* ======================= Exported py interfaces ======================= */

/* operater -> py calls */
void py_op_init(void);
void py_op_exit(void);
	
void py_send(py_msg_t id, os_queue_elem_t *msg, int size);
#define PY_SEND(id_, msg_, size_) do { \
        py_send((id_), (os_queue_elem_t *) (msg_), (size_)); \

/* ======================= Exported operator interface ====================== */

/* van -> operator call */
void op_resume(void);

#endif /* __sac_chart_h__ */
