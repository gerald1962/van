// SPDX-License-Identifier: GPL-2.0

/*
 * Van thread
 *
 * Copyright (C) 2021 Gerald Schueller <gerald.schueller@web.de>
 */

/*============================================================================
  IMPORTED INCLUDE REFERENCES
  ============================================================================*/
#include "sac_chart.h"  /* Py and van routins. */

/*============================================================================
  EXPORTED INCLUDE REFERENCES
  ============================================================================*/
/*============================================================================
  LOCAL NAME CONSTANTS DEFINITIONS
  ============================================================================*/

/* Prompt for the van thread. */
#define P  "V>"

/*============================================================================
  MACROS
  ============================================================================*/
/*============================================================================
  LOCAL TYPE DEFINITIONS
  ============================================================================*/
/**
 * van_s_self_t - list of the van states.
 *
 * @VAN_S_SELF_DOWN:      waiting for the thread addresses.
 * @VAN_S_SELF_READY:     active van channel.
 * @VAN_S_SELF_LOCKED:    locked message interface.
 * @VAN_S_SELF_RELEASED:  all resources have been released.
 * @VAN_S_SELF_INV:       illegal van state.
 **/
typedef enum {
	VAN_S_SELF_DOWN,
	VAN_S_SELF_READY,
	VAN_S_SELF_LOCKED,
	VAN_S_SELF_RELEASED,
	VAN_S_SELF_INV
} van_s_self_t;

/**
 * van_s_py_t - list of the py states.
 *
 * @VAN_S_PY_DOWN:    waiting for the active py.
 * @VAN_S_PY_UP:      py is up.
 * @VAN_S_PY_LOCKED:  its message interface is locked.
 * @VAN_S_PY_INV:     illegal py state.
 **/
typedef enum {
	VAN_S_PY_DOWN,
	VAN_S_PY_UP,
	VAN_S_PY_LOCKED,
	VAN_S_PY_INV
} van_s_py_t;

/**
 * van_data_t - van status.
 *
 * @dev_id:     id of the van shared memory device.
 * @my_state:   current van state.
 * @py_state:   py state.
 * @is_active:  1, if the resources are available.
 * @my_addr:    van thread address.
 **/
typedef struct {
	os_dev_type_t   dev_id;
	van_s_self_t   my_state;
	van_s_py_t     py_state;
	int            is_active;
	void          *my_addr;
} van_data_t;

/*============================================================================
  LOCAL DATA
  ============================================================================*/
/* operater -> van message */
static void van_op_init_ind_exec(os_queue_elem_t *msg);
/* py -> van message */
static void van_py_down_ind_exec(os_queue_elem_t *msg);

/**
 * van_msg_list_s - list of all van input messages.
 *
 * @id:  message id.
 * @cb:  callback for the input message.
 *
 **/
static struct van_msg_list_s {
	van_msg_t      id;
	os_queue_cb_t  *cb;
} van_msg_list[VAN_COUNT_M] = {
	{ VAN_OP_INIT_IND_M,  van_op_init_ind_exec },
	{ VAN_PY_DOWN_IND_M,  van_py_down_ind_exec }
};

/* Van state. */
static van_data_t van_data;

/*============================================================================
  LOCAL FUNCTION PROTOTYPES
  ============================================================================*/
/*============================================================================
  LOCAL FUNCTIONS
  ============================================================================*/
/**
 * van_py_up_ind_send() - inform py, that van is ready.
 *
 * Return:	None.
 **/
static void van_py_up_ind_send(void)
{
	os_queue_elem_t msg;
	van_data_t *v;

	/* Get the address of the van state. */
	v = &van_data;
	
	/* Send the van up indication to py. */
	os_memset(&msg, 0, sizeof(msg));
	msg.param = v->my_addr;
	py_send(PY_VAN_UP_IND_M, &msg, sizeof(msg));
}

/**
 * van_py_down_ind_exec() - py requests the shutdown of the van.
 *
 * @msg:  addess of the generic input message.
 *
 * Return:	None.
 **/
static void van_py_down_ind_exec(os_queue_elem_t *msg)
{
	van_data_t *v;

	/* Get the address of the van state. */
	v = &van_data;

	printf("%s [s:ready, m:down-ind] -> [s:locked]\n", P);

	/* Entry condition. */
	OS_TRAP_IF(v->my_state != VAN_S_SELF_READY || msg == NULL);

	/* Lock the message interface. */
	v->my_state = VAN_S_SELF_LOCKED;
	v->py_state = VAN_S_PY_LOCKED;

	/* Release the shared memory device. */
	os_close(v->dev_id);
	
	/* Resume the main process. */
	op_resume();
}

/**
 * van_op_init_ind_exec() - the main process initiates the van py
 * interworking.
 *
 * @msg:  addess of the generic input message.
 *
 * Return:	None.
 **/
static void van_op_init_ind_exec(os_queue_elem_t *msg)
{
	van_data_t *v;

	/* Get the address of the van state. */
	v = &van_data;

	/* Entry condition. */
	OS_TRAP_IF(v->my_state != VAN_S_SELF_DOWN || msg == NULL);

	printf("%s [s:down, m:op-init] -> [s:ready]\n", P);

	/* Change the channel states and save the thread addresses. */
	v->my_state = VAN_S_SELF_READY;
	v->py_state = VAN_S_PY_UP;

	/* Create the shared memory device. */
	v->dev_id = os_open("/van");
	
	/* Inform py, that van is up and ready. */
	van_py_up_ind_send();
	
	/* XXX Send an unsolicited response. */
	os_sync_write(v->dev_id, "*READY*", 8);

	/* XXX */
#if 0
	char buf[32];
	int i, n;
	
	/* Test the DL channel of the shared memory device. */
	for (i = 0; i < 999999; i++) {
		n = snprintf(buf, 32, "%d", i);
		os_sync_write(v->dev_id, buf, n + 1);		
	}
#endif
}

/*============================================================================
  EXPORTED FUNCTIONS
  ============================================================================*/

/**
 * van_send() - send the message to the van thread.
 *
 * @id:    id of the input message.
 * @msg:   generic pointer to the input message.
 * @size:  size of the input message.
 *
 * Return:	None.
 **/
void van_send(van_msg_t id, os_queue_elem_t *msg, int size)
{
	/* Entry condition. */
	OS_TRAP_IF(id >= VAN_COUNT_M || msg == NULL);

	/* Copy the callback and save the message in the input queue. */
	msg->cb = van_msg_list[id].cb;
	os_queue_send(van_data.my_addr, msg, size);
}

/**
 * van_op_exit() - release the van resources.
 *
 * Return:	None.
 **/
void van_op_exit(void)
{
	van_data_t *v;
	void *p;

	/* Get the address of the van state. */
	v = &van_data;

	/* Entry condition. */
	OS_TRAP_IF(! v->is_active || v->my_state != VAN_S_SELF_LOCKED);

	printf("%s [s:locked, m:op-exit] -> [s:released]\n", P);

	/* Release the van resources. */
	v->is_active = 0;
	v->my_state  = VAN_S_SELF_RELEASED;

	/* Destroy the van thread. */
	p = v->my_addr;
	v->my_addr = NULL;
	os_thread_destroy(p);
}

/**
 * van_op_init() - initialize the van state.
 *
 * Return:	None.
 **/
void van_op_init(void)
{
	van_data_t *v;

	/* Get the address of the van state. */
	v = &van_data;

	/* Entry condition. */
	OS_TRAP_IF(v->is_active);

	/* Install the van thread. */
	v->my_addr = os_thread_create("van", OS_THREAD_PRIO_FOREG, 16);
	
	/* Allocate the van resources. */
	v->is_active = 1;
	v->my_state  = VAN_S_SELF_DOWN;
	v->py_state  = VAN_S_PY_DOWN;
}
