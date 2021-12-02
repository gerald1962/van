// SPDX-License-Identifier: GPL-2.0

/*
 * XXX
 *
 * Copyright (C) 2021 Gerald Schueller <gerald.schueller@web.de>
 */

/*============================================================================
  IMPORTED INCLUDE REFERENCES
  ============================================================================*/
#include "csd_chart.h"  /* Client and server routins. */

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
 * csd_stat - state of the research project.
 *
 * @suspend:  suspend the main process while the test is running.
 **/
static struct csd_stat_s {
	sem_t   suspend;
} csd_stat;

/*============================================================================
  LOCAL FUNCTION PROTOTYPES
  ============================================================================*/
/*============================================================================
  LOCAL FUNCTIONS
  ============================================================================*/

/**
 * csd_cleanup() - free the resources of the research programme.
 *
 * Return:	None.
 **/
static void csd_cleanup(void)
{
	void *p;
	
	printf("%s [p=main,s=exit,o=cleanup]\n", P);

	/* Release the server and client resources. */
	serv_op_exit();
	cli_op_exit();

	os_sem_delete(&csd_stat.suspend);

	/* Release the OS resources. */
	os_exit();
}


/**
 * csd_wait() - suspend the main process.
 *
 * Return:	None.
 **/
static void csd_wait(void)
{
	printf("%s [p=main,s=done,o=suspend]\n", P);
	
	/* Suspend the main process. */
	os_sem_wait(&csd_stat.suspend);
	
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
	serv_send(SERV_OP_INIT_IND_M, &msg, sizeof(msg));
}

/**
 * csd_init() - iniialize the research programme.
 *
 * Return:	None.
 **/
static void csd_init(void)
{
	static struct csd_stat_s *s;

	/* Get the address of the maint process state. */
	s = &csd_stat;
	
	printf("%s [p=main,s=boot,o=init]\n", P);

	/* Initialize the operating system. */
	os_init();

	/* Control the lifetime of the research programme. */
	os_sem_init(&csd_stat.suspend, 0);

	/* Initialize the server and the client state. */
	serv_op_init();
	cli_op_init();

	/* Send the thread addresses to the server and client. */
	op_serv_init_ind_send();
	
	/* Wait for the resume trigger. */
	csd_wait();	
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
	os_sem_release(&csd_stat.suspend);
}

/**
 * main() - start function of the research programme.
 *
 * Return:	0 or force a software trap.
 **/
int main(void)
{
	/* Iniialize the research progamme. */
	csd_init();

	/* Free all resources. */
	csd_cleanup();
	
	return (0);
}
