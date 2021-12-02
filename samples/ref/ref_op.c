// SPDX-License-Identifier: GPL-2.0

/*
 * XXX
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

/* Prompt of the research programme. */
#define P  "M:"

/*============================================================================
  MACROS
  ============================================================================*/
/*============================================================================
  LOCAL TYPE DEFINITIONS
  ============================================================================*/
/*============================================================================
  LOCAL DATA
  ============================================================================*/

/** 
 * ref_stat - state of the research project.
 *
 * @suspend:  suspend the main process while the test is running.
 * @server:   address of the server thread.
 * @client:   address of the server thread.
 **/
static struct ref_stat_s {
	sem_t   suspend;
	void    *server;
	void    *client;
} ref_stat;

/*============================================================================
  LOCAL FUNCTION PROTOTYPES
  ============================================================================*/
/*============================================================================
  LOCAL FUNCTIONS
  ============================================================================*/

/**
 * ref_cleanup() - free the resources of the research programme.
 *
 * Return:	None.
 **/
static void ref_cleanup(void)
{
	void *p;
	
	printf("%s [p=main,s=exit,o=cleanup]\n", P);

	/* Destroy the client and server thread. */
	p = ref_stat.client;
	ref_stat.client = NULL;
	os_thread_destroy(p);
	
	p = ref_stat.server;
	ref_stat.server = NULL;
	os_thread_destroy(p);

	/* Release the server and client resources. */
	serv_op_exit();
	cli_op_exit();

	os_sem_delete(&ref_stat.suspend);

	/* Release the OS resources. */
	os_exit();
}


/**
 * ref_wait() - suspend the main process.
 *
 * Return:	None.
 **/
static void ref_wait(void)
{
	printf("%s [p=main,s=done,o=suspend]\n", P);
	
	/* Suspend the main process. */
	os_sem_wait(&ref_stat.suspend);
	
	printf("%s [p=main,s=exit,o=resume]\n", P);
}

/**
 * op_init_ind_send() - send the thread addresses to the server and client.
 *
 * Return:	None.
 **/
static void op_serv_init_ind_send(void)
{
	os_queue_elem_t msg;
	
	/* Send the start message to the server thread. */
	os_memset(&msg, 0, sizeof(msg));
	msg.param = NULL;
	msg.cb = serv_op_init_ind_exec;
	os_queue_send(ref_stat.server, &msg, sizeof(msg));
}

/**
 * ref_init() - iniialize the research programme.
 *
 * Return:	None.
 **/
static void ref_init(void)
{
	static struct ref_stat_s *s;

	/* Get the address of the maint process state. */
	s = &ref_stat;
	
	printf("%s [p=main,s=boot,o=init]\n", P);

	/* Initialize the operating system. */
	os_init();

	/* Control the lifetime of the research programme. */
	os_sem_init(&ref_stat.suspend, 0);

	/* Create and start the server and client thread. */
	s->server = os_thread_create("server", OS_THREAD_PRIO_FOREG, 16);
	s->client = os_thread_create("client", OS_THREAD_PRIO_FOREG, 16);

	/* Initialize the server and client state. */
	serv_op_init(s->server, s->client);
	cli_op_init(s->server, s->client);

	/* Send the thread addresses to the server and client. */
	op_serv_init_ind_send();
	
	/* Wait for the resume trigger. */
	ref_wait();	
}


/*============================================================================
  EXPORTED FUNCTIONS
  ============================================================================*/

/**
 * op_resume() - resume the main process.
 *
 * Return:	0 or force a software trap.
 **/
void op_resume(void)
{
	printf("%s [t=server,s=suspended,m=resume]\n", P);

	/* Resume the main thread. */
	os_sem_release(&ref_stat.suspend);
}

/**
 * main() - start function of the research programme.
 *
 * Return:	0 or force a software trap.
 **/
int main(void)
{
	/* Iniialize the research progamme. */
	ref_init();

	/* Free all resources. */
	ref_cleanup();
	
	return (0);
}
