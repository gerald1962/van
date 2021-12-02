
/**
 * op_init_ind_msg_t - start message for the server and client.
 *
 * @OS_QUEUE_MSG_HEAD: generic message header.
 * @serv_addr: server address.
 * @cli_addr:  client address.
 **/
typedef struct {
	OS_QUEUE_MSG_HEAD;
	void *serv_addr;
	void *cli_addr;
} op_init_ind_msg_t;


/* ======================= Exported server interfaces ======================= */

/* operater -> server calls */
void serv_op_init(void);
void serv_op_exit(void);

/* operater -> server message */
void serv_op_init_ind_exec(os_queue_elem_t *msg);
/* client -> server message */
void serv_cli_down_ind_exec(os_queue_elem_t *msg);

/* ======================= Exported client interfaces ======================= */

/* operater -> client calls */
void cli_op_init(void);
void cli_op_exit(void);
	
/* operater -> client message */
void cli_op_init_ind_exec(os_queue_elem_t *msg);
/* server -> client message */
void cli_serv_up_ind_exec(os_queue_elem_t *msg);

/* ======================= Exported operator interface ====================== */

/* server -> operator call */
void op_resume(void);

