// SPDX-License-Identifier: GPL-2.0

/*
 * Client thread
 *
 * Copyright (C) 2021 Gerald Schueller <gerald.schueller@web.de>
 */

/*============================================================================
  IMPORTED INCLUDE REFERENCES
  ============================================================================*/
#include "ref_chart.h"  /* Client and server routins. */

/*============================================================================
  EXPORTED INCLUDE REFERENCES
  ============================================================================*/
/*============================================================================
  LOCAL NAME CONSTANTS DEFINITIONS
  ============================================================================*/

/* Prompt for the server thread. */
#define P  "S>"

/*============================================================================
  MACROS
  ============================================================================*/
/*============================================================================
  LOCAL TYPE DEFINITIONS
  ============================================================================*/
/**
 * serv_s_self_t - list of the server states.
 *
 * @SERV_S_SELF_DOWN:      waiting for the thread addresses.
 * @SERV_S_SELF_READY:     active server channel.
 * @SERV_S_SELF_LOCKED:    locked message interface.
 * @SERV_S_SELF_RELEASED:  all resources have been released.
 * @SERV_S_SELF_INV:       illegal server state.
 **/
typedef enum {
	SERV_S_SELF_DOWN,
	SERV_S_SELF_READY,
	SERV_S_SELF_LOCKED,
	SERV_S_SELF_RELEASED,
	SERV_S_SELF_INV
} serv_s_self_t;

/**
 * serv_s_cli_t - list of the client states.
 *
 * @SERV_S_CLI_DOWN:    waiting for the active client.
 * @SERV_S_CLI_UP:      the client is up.
 * @SERV_S_CLI_LOCKED:  its message interface is locked.
 * @SERV_S_CLI_INV:     illegal client state.
 **/
typedef enum {
	SERV_S_CLI_DOWN,
	SERV_S_CLI_UP,
	SERV_S_CLI_LOCKED,
	SERV_S_CLI_INV
} serv_s_cli_t;

/**
 * serv_data_t - server status.
 *
 * @my_state:   current server state.
 * @cli_state:  client state.
 * @is_active:  1, if the resources are available.
 * @my_addr:    server thread address.
 **/
typedef struct {
	serv_s_self_t  my_state;
	serv_s_cli_t   cli_state;
	int    is_active;
	void  *my_addr;
} serv_data_t;

/*============================================================================
  LOCAL DATA
  ============================================================================*/
/* operater -> server message */
static void serv_op_init_ind_exec(os_queue_elem_t *msg);
/* client -> server message */
static void serv_cli_down_ind_exec(os_queue_elem_t *msg);

/**
 * serv_msg_list_s - list of all server input messages.
 *
 * @id:  message id.
 * @cb:  callback for the input message.
 *
 **/
static struct serv_msg_list_s {
	serv_msg_t      id;
	os_queue_cb_t  *cb;
} serv_msg_list[SERV_COUNT_M] = {
	{ SERV_OP_INIT_IND_M,  serv_op_init_ind_exec },
	{ SERV_CLI_DOWN_IND_M, serv_cli_down_ind_exec }
};

/* Server state. */
static serv_data_t serv_data;

/*============================================================================
  LOCAL FUNCTION PROTOTYPES
  ============================================================================*/
/*============================================================================
  LOCAL FUNCTIONS
  ============================================================================*/
/**
 * serv_cli_up_ind_send() - inform the client, that the server is ready.
 *
 * Return:	None.
 **/
static void serv_cli_up_ind_send(void)
{
	os_queue_elem_t msg;
	serv_data_t *s;

	/* Get the address of the server state. */
	s = &serv_data;
	
	/* Send the server up indication to the client. */
	os_memset(&msg, 0, sizeof(msg));
	msg.param = s->my_addr;
	cli_send(CLI_SERV_UP_IND_M, &msg, sizeof(msg));
}

/**
 * serv_cli_down_ind_exec() - the client request the shutdown of the server.
 *
 * @msg:  addess of the generic input message.
 *
 * Return:	None.
 **/
static void serv_cli_down_ind_exec(os_queue_elem_t *msg)
{
	serv_data_t *s;

	/* Get the address of the server state. */
	s = &serv_data;

	printf("%s [s:ready, m:down-ind] -> [s:locked]\n", P);

	/* Entry condition. */
	OS_TRAP_IF(s->my_state != SERV_S_SELF_READY || msg == NULL);

	/* Lock the message interface. */
	s->my_state  = SERV_S_SELF_LOCKED;
	s->cli_state = SERV_S_CLI_LOCKED;

	/* Resume the main process. */
	op_resume();
}

/**
 * serv_op_init_ind_exec() - the main process initiates the server client
 * interworking.
 *
 * @msg:  addess of the generic input message.
 *
 * Return:	None.
 **/
static void serv_op_init_ind_exec(os_queue_elem_t *msg)
{
	serv_data_t *s;

	/* Get the address of the server state. */
	s = &serv_data;

	/* Entry condition. */
	OS_TRAP_IF(s->my_state != SERV_S_SELF_DOWN || msg == NULL);

	printf("%s [s:down, m:op-init] -> [s:ready]\n", P);

	/* Change the channel states and save the thread addresses. */
	s->my_state  = SERV_S_SELF_READY;
	s->cli_state = SERV_S_CLI_UP;

	/* Inform the client, that the server is up and ready. */
	serv_cli_up_ind_send();
}

/*============================================================================
  EXPORTED FUNCTIONS
  ============================================================================*/

/**
 * serv_send() - send the message to the server thread.
 *
 * @id:    id of the input message.
 * @msg:   generic pointer to the input message.
 * @size:  size of the input message.
 *
 * Return:	None.
 **/
void serv_send(serv_msg_t id, os_queue_elem_t *msg, int size)
{
	/* Entry condition. */
	OS_TRAP_IF(id >= SERV_COUNT_M || msg == NULL);

	/* Copy the callback and save the message in the input queue. */
	msg->cb = serv_msg_list[id].cb;
	os_queue_send(serv_data.my_addr, msg, size);
}

/**
 * serv_op_exit() - release the server resources.
 *
 * Return:	None.
 **/
void serv_op_exit(void)
{
	serv_data_t *s;
	void *p;

	/* Get the address of the server state. */
	s = &serv_data;

	/* Entry condition. */
	OS_TRAP_IF(! s->is_active || s->my_state != SERV_S_SELF_LOCKED);

	printf("%s [s:locked, m:op-exit] -> [s:released]\n", P);

	/* Release the server resources. */
	s->is_active = 0;
	s->my_state  = SERV_S_SELF_RELEASED;

	/* Destroy the server thread. */
	p = s->my_addr;
	s->my_addr = NULL;
	os_thread_destroy(p);
}

/**
 * serv_op_init() - initialize the server state.
 *
 * Return:	None.
 **/
void serv_op_init(void)
{
	serv_data_t *s;

	/* Get the address of the server state. */
	s = &serv_data;

	/* Entry condition. */
	OS_TRAP_IF(s->is_active);

	/* Install the server thread. */
	s->my_addr = os_thread_create("server", OS_THREAD_PRIO_FOREG, 16);
	
	/* Allocate the server resources. */
	s->is_active = 1;
	s->my_state  = SERV_S_SELF_DOWN;
	s->cli_state = SERV_S_CLI_DOWN;
}
