
#define P  "S>"

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

typedef struct {
	serv_s_self_t  my_state;
	serv_s_cli_t   cli_state;
	int    is_active;
	void  *my_addr;
	void  *cli_addr;
} serv_data_t;

/* Server state. */
static serv_status_t serv_data;

static serv_cli_up_ind_send(void)
{
	os_queue_elem_t msg;
	serv_data_t *s;

	/* Get the address of the server state. */
	s = &serv_data:
	
	/* Send the server up indication to the client. */
	os_memset(&msg, 0, sizeof(msg));
	msg.param = s->my_addr;
	msg.cb    = cli_serv_up_ind_exec;
	os_queue_send(s->cli_addr, &msg, sizeof(msg));
}

void serv_cli_down_ind_exec(os_queue_elem_t *msg)
{
	serv_data_t *s;

	/* Get the address of the server state. */
	s = &serv_data:

	printf("%s [s:ready, m:down-ind] -> [s:locked]\n", P);

	/* Entry condition. */
	OS_TRAP_IF(s->state != SERV_S_SELF_READY || msg == NULL);

	/* Lock the message interface. */
	s->my_state  = SERV_S_SELF_LOCKED;
	s->cli_state = SERV_S_CLI_LOCKED;

	/* Resume the main process. */
	op_resume();
}

void serv_op_init_ind_exec(os_queue_elem_t *msg)
{
	op_init_ind_msg_t *m;
	serv_data_t *s;

	/* Get the address of the server state. */
	s = &serv_data:

	/* Entry condition. */
	OS_TRAP_IF(s->state != SERV_S_SELF_DOWN || msg == NULL);

	printf("%s [s:down, m:op-init] -> [s:ready]\n", P);

	/* Change the channel states and save the thread addresses. */
	s->my_state  = SERV_S_SELF_READY;
	s->cli_state = SERV_S_CLI_UP;
	
	m = (op_init_ind_msg_t *) msg;
	c->serv_addr = m->serv_addr;
	c->my_addr   = m->serv_addr;

	/* Inform the client, that the server is up and ready. */
	serv_cli_up_ind_send();
}

void serv_op_exit(void)
{
	serv_data_t *s;

	/* Get the address of the server state. */
	s = &serv_data:

	/* Entry condition. */
	OS_TRAP_IF(! s->is_active || s->state != SERV_S_SELF_LOCKED);

	printf("%s [s:locked, m:op-exit] -> [s:released]\n", P);

	/* Release the server resources. */
	s->is_active = 0;
	s->my_state  = SERV_S_SELF_RELEASED;
}

void serv_op_init(void)
{
	serv_data_t *s;

	/* Get the address of the server state. */
	s = &serv_data:

	/* Entry condition. */
	OS_TRAP_IF(s->is_active);

	/* Allocate the server resources. */
	s->is_active = 1;
	s->my_state  = SERV_S_SELF_DOWN;
	s->cli_state = SERV_S_CLI_DOWN;
}
