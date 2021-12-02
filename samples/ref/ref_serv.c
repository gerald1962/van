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
 * @cli_addr:   client thread address.
 **/
typedef struct {
	serv_s_self_t  my_state;
	serv_s_cli_t   cli_state;
	int    is_active;
	void  *my_addr;
	void  *cli_addr;
} serv_data_t;

/*============================================================================
  LOCAL DATA
  ============================================================================*/
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
	msg.cb    = cli_serv_up_ind_exec;
	os_queue_send(s->cli_addr, &msg, sizeof(msg));
}

/*============================================================================
  EXPORTED FUNCTIONS
  ============================================================================*/

/**
 * serv_cli_down_ind_exec() - the client request the shutdown of the server.
 *
 * @msg:  addess of the generic input message.
 *
 * Return:	None.
 **/
void serv_cli_down_ind_exec(os_queue_elem_t *msg)
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
void serv_op_init_ind_exec(os_queue_elem_t *msg)
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

/**
 * serv_op_exit() - release the server resources.
 *
 * Return:	None.
 **/
void serv_op_exit(void)
{
	serv_data_t *s;

	/* Get the address of the server state. */
	s = &serv_data;

	/* Entry condition. */
	OS_TRAP_IF(! s->is_active || s->my_state != SERV_S_SELF_LOCKED);

	printf("%s [s:locked, m:op-exit] -> [s:released]\n", P);

	/* Release the server resources. */
	s->is_active = 0;
	s->my_state  = SERV_S_SELF_RELEASED;
}

/**
 * serv_op_init() - initialize the server state.
 *
 * @server:  address of the server thread.
 * @client:  address of the client thread.
 *
 * Return:	None.
 **/
void serv_op_init(void *server, void *client)
{
	serv_data_t *s;

	/* Get the address of the server state. */
	s = &serv_data;

	/* Entry condition. */
	OS_TRAP_IF(s->is_active);

	/* Save the thread addresses. */
	s->my_addr  = server;
	s->cli_addr = client;
	
	/* Allocate the server resources. */
	s->is_active = 1;
	s->my_state  = SERV_S_SELF_DOWN;
	s->cli_state = SERV_S_CLI_DOWN;
}
