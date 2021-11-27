// SPDX-License-Identifier: GPL-2.0

/*
 * Coverage of the VAN operating system tests.
 *
 * Copyright (C) 2021 Gerald Schueller <gerald.schueller@web.de>
 */
/*============================================================================
  IMPORTED INCLUDE REFERENCES
  ============================================================================*/
#include "os.h"      /* Operating system: os_sem_create(). */

/*============================================================================
  EXPORTED INCLUDE REFERENCES
  ============================================================================*/
/*============================================================================
  LOCAL NAME CONSTANTS DEFINITIONS
  ============================================================================*/
/* Size of the message buffer. */
#define BUF_LEN  32

/*============================================================================
  MACROS
  ============================================================================*/
/*============================================================================
  LOCAL TYPE DEFINITIONS
  ============================================================================*/

/* ICMP message for the ping protocol. */
typedef struct {
        OS_QUEUE_MSG_HEAD;
        char buf[BUF_LEN];
} icmp_msg_t;

/*============================================================================
  LOCAL DATA
  ============================================================================*/

/* Control semaphore for the main process. */
static sem_t suspend;

/* Ping message text. */
static char *ping = "ping";

/*============================================================================
  LOCAL FUNCTION PROTOTYPES
  ============================================================================*/

/**
 * ping_send() - start function of the VAN code coverage.
 *
 * @server:  address of the server thread.
 *
 * Return:	None.
 **/
static void ping_exec(os_queue_elem_t *m)
{
        icmp_msg_t *msg;
	int ret;
	
	/* Decode the icmp message. */
        msg = (icmp_msg_t *) m;

	/* Test the message. */
	ret = os_strcmp(msg->buf, ping);
	OS_TRAP_IF(ret != 0);
	
	printf("%s\n", msg->buf);

	/* Resume the main process. */
	os_sem_release(&suspend);
}

/**
 * ping_send() - send the ping message to the server thread.
 *
 * @server:  address of the server thread.
 *
 * Return:	None.
 **/
static void ping_send(void *server)
{
        icmp_msg_t msg;

	/* Define the ping message. */
	os_memset(&msg, 0, sizeof(msg));
	msg.param = server;
	msg.cb    = ping_exec;
	os_strcpy(msg.buf, BUF_LEN, ping);

	/* Send the message to the server thread. */
	OS_SEND(server, &msg, sizeof(msg));
}

/*============================================================================
  LOCAL FUNCTIONS
  ============================================================================*/
/*============================================================================
  EXPORTED FUNCTIONS
  ============================================================================*/

/**
 * main() - start function of the VAN code coverage.
 *
 * Return:	0 or force a software trap.
 **/
int main(void)
{
	void *server, *p;
	int i;
	
	/* Initialize the operating system. */
	os_init();

	/* Create the control semaphore for the main process. */
	os_sem_init(&suspend, 0);

	/* malloc and free test */
	for (i = 0; i < (2 * OS_MALLOC_LIMIT); i++) {
		p = OS_MALLOC(1);
		OS_FREE(p);
		p = OS_MALLOC(1);
		OS_FREE(p);
	}
	
	/* Test a thread with an invalid priority. */
	p = os_thread_create("xxx", 11, 256);
	os_thread_destroy(p);
	
	/* Create and start the server thread. */
	server = os_thread_create("server", OS_THREAD_PRIO_FOREG, 256);

	/* Send the ping messagae to the server and test the cleanup of the
	 *  message queue in os_exit. */
	for (i = 0; i < 256; i++)
		ping_send(server);
	
	/* Suspend the main process. */
	os_sem_wait(&suspend);

	/* Destroy the server thread. */
	os_thread_destroy(server);

	/* Release the control semaphore for the main process. */
	os_sem_delete(&suspend);

	/* Release the OS resources. */
	os_exit();

	/* Test the trap handling. */
	os_trap(__FILE__, "*coverage*", __LINE__);
	
	return (0);
}
