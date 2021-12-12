// SPDX-License-Identifier: GPL-2.0

/*
 * Py thread
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

/* Prompt for the py thread. */
#define P  "P>"

/*============================================================================
  MACROS
  ============================================================================*/
/*============================================================================
  LOCAL TYPE DEFINITIONS
  ============================================================================*/
/**
 * py_s_self_t - list of the py states.
 *
 * @PY_S_SELF_DOWN:      waiting for the thread addresses.
 * @PY_S_SELF_INIT:      waiting for the start of van.
 * @PY_S_SELF_READY:     active py van channel.
 * @PY_S_SELF_LOCKED:    the message interface is locked.
 * @PY_S_SELF_RELEASED:  all resources have been released.
 * @PY_S_SELF_INV:       illegal py state.
 **/
typedef enum {
	PY_S_SELF_DOWN,
	PY_S_SELF_INIT,
	PY_S_SELF_READY,
	PY_S_SELF_LOCKED,
	PY_S_SELF_RELEASED,
	PY_S_SELF_INV
} py_s_self_t;

/**
 * py_s_van_t - list of the van states.
 *
 * @PY_S_VAN_DOWN:    waiting for the active van.
 * @PY_S_VAN_UP:      van is ready.
 * @PY_S_VAN_LOCKED:  the message interface is locked.
 * @PY_S_VAN_INV:     illegal van state.
 **/
typedef enum {
	PY_S_VAN_DOWN,
	PY_S_VAN_UP,
	PY_S_VAN_LOCKED,
	PY_S_VAN_INV
} py_s_van_t;

/**
 * py_data_t - py status.
 *
 * @dev_id:     id of the py shared memory device.
 * @my_state:   current py state.
 * @van_state:  van state.
 * @is_active:  1, if the resources are available.
 * @my_addr:    py thread address.
 **/
typedef struct {
	os_dev_type_t  dev_id;
	py_s_self_t    my_state;
	py_s_van_t     van_state;
	int            is_active;
	void          *my_addr;
} py_data_t;

/*============================================================================
  LOCAL DATA
  ============================================================================*/
/* van -> py message */
static void py_van_up_ind_exec(os_queue_elem_t *msg);

/**
 * py_msg_list_s - list of all py input messages.
 *
 * @id:  message id.
 * @cb:  callback for the input message.
 *
 **/
static struct py_msg_list_s {
	py_msg_t        id;
	os_queue_cb_t  *cb;
} py_msg_list[PY_COUNT_M] = {
	{ PY_VAN_UP_IND_M,  py_van_up_ind_exec }
};

/* Py state. */
static py_data_t py_data;

/*============================================================================
  LOCAL FUNCTION PROTOTYPES
  ============================================================================*/
/*============================================================================
  LOCAL FUNCTIONS
  ============================================================================*/
/**
 * py_van_down_ind_send() - send the down indication to van.
 *
 * Return:	None.
 **/
static void py_van_down_ind_send(void)
{
	os_queue_elem_t msg;
	py_data_t *p;

	/* Get the address of the py state. */
	p = &py_data;

	printf("%s [s:ready, m:van-down] -> [s:locked]\n", P);

	/* Entry condition. */
	OS_TRAP_IF(p->my_state != PY_S_SELF_READY);

	/* Change the channel states. */
	p->my_state  = PY_S_SELF_LOCKED;
	p->van_state = PY_S_VAN_LOCKED;
	
	/* Send the down indication to the van. */
	os_memset(&msg, 0, sizeof(msg));
	msg.param = p->my_addr;
	van_send(VAN_PY_DOWN_IND_M, &msg, sizeof(msg));
}

/**
 * py_van_up_ind_exec() - up indication from van.
 *
 * @msg:  generic input message.
 *
 * Return:	None.
 **/
static void py_van_up_ind_exec(os_queue_elem_t *msg)
{
	py_data_t *p;
	char *buf;
	int n;
	
	/* Get the address of the py state. */
	p = &py_data;

	printf("%s [s:init, m:van-up] -> [s:ready]\n", P);
	
	/* Entry condition. */
	OS_TRAP_IF(p->my_state != PY_S_SELF_INIT || msg == NULL);

	/* The communication channels between py and van are active. */
	p->my_state   = PY_S_SELF_READY;
	p->van_state = PY_S_VAN_UP;

	/* Initialize the shared memory device. */
	p->dev_id = os_open("/python");

	/* XXX */
#if 0
	/* Wait for the unsolicited responses from van. */
	for(;;) {
		buf = NULL;
		n = os_sync_read(p->dev_id, &buf, 1024);
		OS_TRAP_IF(n < 1 || buf == NULL);	
		printf("%s [b:\"%s\", s:%d]\n", P, buf, n);
	}
#else
	/* XXX Wait for the unsolicited responses from van. */
	buf = NULL;
	n = os_sync_read(p->dev_id, &buf, 1024);
	OS_TRAP_IF(n < 1 || buf == NULL);	
	printf("%s [b:\"%s\", s:%d]\n", P, buf, n);

	/* Release the DL buffer. */
	n = os_sync_read(p->dev_id, NULL, 0);
	OS_TRAP_IF(n > 0);
	
	/* Release the shared memory device. */
	os_close(p->dev_id);
	
	/* Simulate the shutdown interworking initiated by the control terminal. */
	py_van_down_ind_send();
#endif
}

/*============================================================================
  EXPORTED FUNCTIONS
  ============================================================================*/
/**
 * py_send() - send the message to the py thread.
 *
 * @id:    id of the input message.
 * @msg:   generic pointer to the input message.
 * @size:  size of the input message.
 *
 * Return:	None.
 **/
void py_send(py_msg_t id, os_queue_elem_t *msg, int size)
{
	/* Entry condition. */
	OS_TRAP_IF(id >= PY_COUNT_M || msg == NULL);

	/* Copy the callback and save the message in the input queue. */
	msg->cb = py_msg_list[id].cb;
	os_queue_send(py_data.my_addr, msg, size);
}

/**
 * py_op_exit() - release the py resources.
 *
 * Return:	None.
 **/
void py_op_exit(void)
{
	py_data_t *p;
	void *my_addr;

	/* Get the address of the py state. */
	p = &py_data;

	/* Entry condition. */
	OS_TRAP_IF(! p->is_active || p->my_state != PY_S_SELF_LOCKED);

	printf("%s [s:locked, m:op-exit] -> [s:released]\n", P);

	/* Release the py resources. */
	p->is_active = 0;
	p->my_state  = PY_S_SELF_RELEASED;

	/* Destroy the py thread. */
	my_addr = p->my_addr;
	p->my_addr = NULL;
	os_thread_destroy(my_addr);
}

/**
 * py_op_init() - initialize the py state.
 *
 * Return:	None.
 **/
void py_op_init(void)
{
	py_data_t *p;
	
	printf("%s [s:down, m:op-init] -> [s:init]\n", P);

	/* Get the address of the py state. */
	p = &py_data;
	
	/* Entry condition. */
	OS_TRAP_IF(p->is_active);

	/* Install the py thread. */
	p->my_addr = os_thread_create("py", OS_THREAD_PRIO_FOREG, 16);

	/* Allocate the py resources. */
	p->is_active = 1;
	p->my_state  = PY_S_SELF_INIT;
	p->van_state = PY_S_VAN_DOWN;
}
