// SPDX-License-Identifier: GPL-2.0

/*
 * Py Van Shared Memory Concept
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
 * sac_stat - state of the research project.
 *
 * @suspend:  suspend the main process while the test is running.
 **/
static struct sac_stat_s {
	sem_t   suspend;
} sac_stat;

/*============================================================================
  LOCAL FUNCTION PROTOTYPES
  ============================================================================*/
/*============================================================================
  LOCAL FUNCTIONS
  ============================================================================*/

/**
 * sac_cleanup() - free the resources of the research programme.
 *
 * Return:	None.
 **/
static void sac_cleanup(void)
{
	printf("%s [p:main,s:exit,o:cleanup]\n", P);

	/* Release the van and py resources. */
	van_op_exit();
	py_op_exit();

	os_sem_delete(&sac_stat.suspend);

	/* Release the OS resources. */
	os_exit();
}


/**
 * sac_wait() - suspend the main process.
 *
 * Return:	None.
 **/
static void sac_wait(void)
{
	printf("%s [p:main,s:done,o:suspend]\n", P);
	
	/* Suspend the main process. */
	os_sem_wait(&sac_stat.suspend);
	
	printf("%s [p:main,s:exit,o:resume]\n", P);
}

/**
 * op_init_ind_send() - send the thread addresses to the van and py.
 *
 * Return:	None.
 **/
static void op_van_init_ind_send(void)
{
	os_queue_elem_t msg;
	
	/* Send the start message to the van thread. */
	os_memset(&msg, 0, sizeof(msg));
	msg.param = NULL;
	van_send(VAN_OP_INIT_IND_M, &msg, sizeof(msg));
}

/**
 * sac_init() - iniialize the research programme.
 *
 * Return:	None.
 **/
static void sac_init(void)
{
	printf("%s [p:main,s:boot,o:init]\n", P);

	/* Initialize the operating system. */
	os_init();

	/* Switch off the OS trace. */
	os_trace_button(0);

	/* Control the lifetime of the research programme. */
	os_sem_init(&sac_stat.suspend, 0);

	/* Initialize the van and the py state. */
	van_op_init();
	py_op_init();

	/* Resume the van thread. */
	op_van_init_ind_send();
	
	/* Wait for the resume trigger. */
	sac_wait();	
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
	printf("%s [t:van,s:suspended,m:resume]\n", P);

	/* Resume the main thread. */
	os_sem_release(&sac_stat.suspend);
}

/**
 * main() - start function of the research programme.
 *
 * Return:	0 or force a software trap.
 **/
int main(void)
{
	/* Iniialize the research progamme. */
	sac_init();

	/* Free all resources. */
	sac_cleanup();
	
	return (0);
}
