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

/* Prompt for the client thread. */
#define P  "C>"

/*============================================================================
  MACROS
  ============================================================================*/
/*============================================================================
  LOCAL TYPE DEFINITIONS
  ============================================================================*/
/**
 * cli_s_self_t - list of the client states.
 *
 * @CLI_S_SELF_DOWN:      waiting for the thread addresses.
 * @CLI_S_SELF_INIT:      waiting for the start of the server.
 * @CLI_S_SELF_READY:     active client server channel.
 * @CLI_S_SELF_LOCKED:    the message interface is locked.
 * @CLI_S_SELF_RELEASED:  all resources have been released.
 * @CLI_S_SELF_INV:       illegal client state.
 **/
typedef enum {
	CLI_S_SELF_DOWN,
	CLI_S_SELF_INIT,
	CLI_S_SELF_READY,
	CLI_S_SELF_LOCKED,
	CLI_S_SELF_RELEASED,
	CLI_S_SELF_INV
} cli_s_self_t;

/**
 * cli_s_serv_t - list of the server states.
 *
 * @CLI_S_SERV_DOWN:    waiting for the active server.
 * @CLI_S_SERV_UP:      server is ready.
 * @CLI_S_SERV_LOCKED:  the message interface is locked.
 * @CLI_S_SERV_INV:     illegal server state.
 **/
typedef enum {
	CLI_S_SERV_DOWN,
	CLI_S_SERV_UP,
	CLI_S_SERV_LOCKED,
	CLI_S_SERV_INV
} cli_s_serv_t;

/**
 * cli_data_t - client status.
 *
 * @my_state:    current client state.
 * @serv_state:  server state.
 * @is_active:   1, if the resources are available.
 * @my_addr:     client thread address.
 * @cli_addr:    server thread address.
 **/
typedef struct {
	cli_s_self_t  my_state;
	cli_s_serv_t  serv_state;
	int    is_active;
	void  *my_addr;
	void  *serv_addr;
} cli_data_t;

/*============================================================================
  LOCAL DATA
  ============================================================================*/
/* server -> client message */
static void cli_serv_up_ind_exec(os_queue_elem_t *msg);

/**
 * cli_msg_list_s - list of all client input messages.
 *
 * @id:  message id.
 * @cb:  callback for the input message.
 *
 **/
static struct cli_msg_list_s {
	cli_msg_t      id;
	os_queue_cb_t  *cb;
} cli_msg_list[CLI_COUNT_M] = {
	{ CLI_SERV_UP_IND_M,  cli_serv_up_ind_exec }
};

/* Client state. */
static cli_data_t cli_data;

/*============================================================================
  LOCAL FUNCTION PROTOTYPES
  ============================================================================*/
/*============================================================================
  LOCAL FUNCTIONS
  ============================================================================*/
/**
 * cli_serv_down_ind_send() - send the down indication to the server.
 *
 * Return:	None.
 **/
static void cli_serv_down_ind_send(void)
{
	os_queue_elem_t msg;
	cli_data_t *c;

	/* Get the address of the client state. */
	c = &cli_data;

	printf("%s [s:ready, m:serv-down] -> [s:locked]\n", P);

	/* Entry condition. */
	OS_TRAP_IF(c->my_state != CLI_S_SELF_READY);

	/* Change the channel states. */
	c->my_state   = CLI_S_SELF_LOCKED;
	c->serv_state = CLI_S_SERV_LOCKED;
	
	/* Send the down indication to the server. */
	os_memset(&msg, 0, sizeof(msg));
	msg.param = c->my_addr;
	serv_send(SERV_CLI_DOWN_IND_M, &msg, sizeof(msg));
}

/**
 * cli_serv_up_ind_exec() - up indication from the server.
 *
 * @msg:  generic input message.
 *
 * Return:	None.
 **/
static void cli_serv_up_ind_exec(os_queue_elem_t *msg)
{
	cli_data_t *c;

	/* Get the address of the client state. */
	c = &cli_data;

	printf("%s [s:init, m:serv-up] -> [s:ready]\n", P);
	
	/* Entry condition. */
	OS_TRAP_IF(c->my_state != CLI_S_SELF_INIT || msg == NULL);

	/* The communication channels between client and server are active. */
	c->my_state   = CLI_S_SELF_READY;
	c->serv_state = CLI_S_SERV_UP;

	/* Simulate the shutdown interworking initiated by the control terminal. */
	cli_serv_down_ind_send();
}

/*============================================================================
  EXPORTED FUNCTIONS
  ============================================================================*/
/**
 * cli_send() - send the message to the client thread.
 *
 * @id:    id of the input message.
 * @msg:   generic pointer to the input message.
 * @size:  size of the input message.
 *
 * Return:	None.
 **/
void cli_send(cli_msg_t id, os_queue_elem_t *msg, int size)
{
	/* Entry condition. */
	OS_TRAP_IF(id >= CLI_COUNT_M || msg == NULL);

	/* Copy the callback and save the message in the input queue. */
	msg->cb = cli_msg_list[id].cb;
	os_queue_send(cli_data.my_addr, msg, size);
}

/**
 * cli_op_exit() - release the client resources.
 *
 * Return:	None.
 **/
void cli_op_exit(void)
{
	cli_data_t *c;
	void *p;

	/* Get the address of the client state. */
	c = &cli_data;

	/* Entry condition. */
	OS_TRAP_IF(! c->is_active || c->my_state != CLI_S_SELF_LOCKED);

	printf("%s [s:locked, m:op-exit] -> [s:released]\n", P);

	/* Release the client resources. */
	c->is_active = 0;
	c->my_state  = CLI_S_SELF_RELEASED;

	/* Destroy the client thread. */
	p = c->my_addr;
	c->my_addr = NULL;
	os_thread_destroy(p);
}

/**
 * cli_op_init() - initialize the client state.
 *
 * Return:	None.
 **/
void cli_op_init(void)
{
	cli_data_t *c;
	
	printf("%s [s:down, m:op-init] -> [s:init]\n", P);

	/* Get the address of the client state. */
	c = &cli_data;
	
	/* Entry condition. */
	OS_TRAP_IF(c->is_active);

	/* Install the client thread. */
	c->my_addr = os_thread_create("client", OS_THREAD_PRIO_FOREG, 16);

	/* Allocate the client resources. */
	c->is_active  = 1;
	c->my_state   = CLI_S_SELF_INIT;
	c->serv_state = CLI_S_SERV_DOWN;
}
