
#define P  "C>"

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

typedef struct {
	cli_s_self_t  my_state;
	cli_s_serv_t  serv_state;
	int    is_active;
	void  *my_addr;
	void  *serv_addr;
} cli_data_t;

/* Client state. */
static cli_status_t cli_data;

static void cli_serv_down_ind_send(void)
{
	os_queue_elem_t msg;
	cli_data_t *c;

	/* Get the address of the client state. */
	c = &cli_data:

	/* XXX */
#if 1
	printf("%s [s:ready, m:serv-down] -> [s:locked]\n", P);

	/* Entry condition. */
	OS_TRAP_IF(c->state != CLI_S_SELF_READY || msg == NULL);

	/* Change the channel states. */
	c->my_state   = CLI_S_SELF_LOCKED;
	c->serv_state = CLI_S_SERV_LOCKED;
#endif
	
	/* Send the down indication to the server. */
	os_memset(&msg, 0, sizeof(msg));
	msg.param = c->my_addr;
	msg.cb    = serv_cli_down_ind_exec;
	os_queue_send(c->serv_addr, &msg, sizeof(msg));
}

void cli_serv_up_ind_exec(os_queue_elem_t *msg)
{
	cli_data_t *c;

	/* Get the address of the client state. */
	c = &cli_data:

	printf("%s [s:init, m:serv-up] -> [s:ready]\n", P);
	
	/* Entry condition. */
	OS_TRAP_IF(c->state != CLI_S_SELF_INIT || msg == NULL);

	/* The communication channels between client and server are active. */
	c->my_state   = CLI_S_SELF_READY;
	c->serv_state = CLI_S_SERV_READY;

	/* Simulate the shutdow interworking initiated by the control terminal. */

	/* XXX Start a count down timer. */
#if 1
	cli_serv_down_ind_send();
#endif
}

void cli_op_init_ind_exec(os_queue_elem_t *msg)
{
	op_init_ind_msg_t *m;
	cli_data_t *c;

	/* Get the address of the client state. */
	c = &cli_data:

	/* Entry condition. */
	OS_TRAP_IF(c->state != CLI_S_SELF_DOWN || msg == NULL);

	printf("%s [s:down, m:op-init] -> [s:init]\n", P);

	/* Change the client channel state. */
	c->my_state = CLI_S_SELF_INIT;

	/* Save the thread addersses. */
	m = (op_init_ind_msg_t *) msg;
	c->serv_addr = m->serv_addr;
	c->my_addr   = m->cli_addr;
}

void cli_op_exit(void)
{
	cli_data_t *c;

	/* Get the address of the client state. */
	c = &cli_data:

	/* Entry condition. */
	OS_TRAP_IF(! c->is_active || c->state != CLI_S_SELF_LOCKED);

	printf("%s [s:locked, m:op-exit] -> [s:released]\n", P);

	/* Release the client resources. */
	c->is_active = 0;
	c->my_state  = CLI_S_SELF_RELEASED;
}

void cli_op_init(void)
{
	cli_data_t *c;
	
	/* Get the address of the client state. */
	c = &cli_data:
	
	/* Entry condition. */
	OS_TRAP_IF(c->is_active);

	/* Allocate the client resources. */
	c->is_active  = 1;
	c->my_state   = CLI_S_SELF_DOWN;
	c->serv_state = CLI_S_SERV_DOWN;
}
