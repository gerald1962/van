// SPDX-License-Identifier: GPL-2.0

/*
 * Operating system interfaces.
 *
 * Copyright (C) 2022 Gerald Schueller <gerald.schueller@web.de>
 */

/*============================================================================
  IMPORTED INCLUDE REFERENCES
  ============================================================================*/
#include <sys/socket.h>  /* Internet communication: socket(). */
#include <arpa/inet.h>   /* Internet communication: IPPROTO_UDP. */
#include "os.h"          /* Operating system: os_inet_sopen() */
#include "os_private.h"  /* Local interfaces of the OS: os_trap_init() */

/*============================================================================
  EXPORTED INCLUDE REFERENCES
  ============================================================================*/
/*============================================================================
  LOCAL NAME CONSTANTS DEFINITIONS
  ============================================================================*/
#define INET_ADDR_LEN   32  /* Max. length of the IP address string. */
#define INET_COUNT       2  /* Number of the peers. */
#define INET_THR_QSIZE   1  /* Input queue size of the inet threads. */

/* Define the priority of the inet threads. */
#if defined(USE_OS_RT)
#define INET_THR_PRIO    OS_THREAD_PRIO_HARDRT
#else
#define INET_THR_PRIO    OS_THREAD_PRIO_SOFTRT
#endif

/*============================================================================
  MACROS
  ============================================================================*/
/*============================================================================
  LOCAL TYPE DEFINITIONS
  ============================================================================*/
/**
 * inet - state of the van internet end point.
 *
 * @cid:      communication id of the internet end point.
 * @is_srv:   if 1, the internet end point is a server.
 * @ip_addr:  copy of the IP address string.
 * @rcv_thr:  id of the receiver thread.
 * @snd_thr:  id of the sender thread.
 * @sid:      socket file descriptor.
 **/
typedef struct inet_s {
	int    cid;
	int    is_srv;
	char   ip_addr[INET_ADDR_LEN];
	void  *rcv_thr;
	void  *snd_thr;
	int    sid;
} inet_t;

/*============================================================================
  LOCAL DATA
  ============================================================================*/
/**
 * is - state of the inet module.
 *
 * @mutex:  protect the critical section in the os_inet_open() and
 *          os_inet_close() operation.
 * @inet:   list of the internet cable end points.
 **/
static struct is_s {
	pthread_mutex_t  mutex;
	inet_t  *inet[INET_COUNT];	
} is;

/*============================================================================
  LOCAL FUNCTION PROTOTYPES
  ============================================================================*/
/*============================================================================
  LOCAL FUNCTIONS
  ============================================================================*/
/*============================================================================
  EXPORTED FUNCTIONS
  ============================================================================*/
/**
 * os_inet_close() - remove a server or client internet end point.
 *
 * @cid:  internet connection id.
 *
 * Return:	None.
 **/
void os_inet_close(int cid)
{
	inet_t *ip;
	int rv;

	/* Enter the critical section. */
	os_cs_enter(&is.mutex);

	/* Entry point. */
	OS_TRAP_IF(cid < 0 || cid >= INET_COUNT || is.inet[cid] == NULL ||
		   is.inet[cid]->cid != cid);

	/* Get the pointer to the internet end point state. */
	ip = is.inet[cid];

	/* Release the endpoint for the internet communication. */
	rv = close(ip->sid);
	OS_TRAP_IF(rv == -1);

	/* XXX */
	
	/* Release the sender thread. */
	os_thread_destroy(ip->snd_thr);
	
	/* Release the receiver thread. */
	os_thread_destroy(ip->rcv_thr);

	/* Free the memory for the state of the internet end point. */
	OS_FREE(is.inet[cid]);
	
	/* Leave the critical section. */
	os_cs_leave(&is.mutex);
}

/**
 * os_inet_open() - establish a server or client internet end point.
 *
 * @is_server:  if 1, the van server shall be installed, otherwise the client.
 * ip_address:  pointer to the IP address string.
 *
 * Return:	return the connection id.
 **/
int os_inet_open(int is_server, const char *ip_address)
{
	inet_t **p, *ip;
	char name[OS_THREAD_NAME_LEN];
	int n, i;
	
	/* Entry condition. */
	OS_TRAP_IF(ip_address == NULL);

	/* Enter the critical section. */
	os_cs_enter(&is.mutex);

	/* Test the length of the IP address. */
	n = os_strlen(ip_address);
	OS_TRAP_IF(n >= INET_ADDR_LEN);

	/* Search for a free internet end point. */
	for (i = 0, p = is.inet; i < INET_COUNT && *p != NULL; i++, p++)
		;

	/* Final condition. */
	OS_TRAP_IF(i >= INET_COUNT);

	/* Allocate memory for the state of the internet end point. */
	*p = OS_MALLOC(sizeof(inet_t));
	ip = *p;
	os_memset(ip, 0, sizeof(inet_t));

	/* Initialize the state of the internet end point. */
	ip->cid = i;
	ip->is_srv = is_server;
	os_strcpy(ip->ip_addr, INET_ADDR_LEN, ip_address);

	/* Build the name of the receiver thread. */
	snprintf(name, OS_THREAD_NAME_LEN, "inet_rcv_%d", i);
	
	/* Create the receiver thread. */
	ip->rcv_thr = os_thread_create(name, INET_THR_PRIO, INET_THR_QSIZE);

	/* Build the name of the sender thread. */
	snprintf(name, OS_THREAD_NAME_LEN, "inet_snd_%d", i);
	
	/* Create the sender thread. */
	ip->snd_thr = os_thread_create(name, INET_THR_PRIO, INET_THR_QSIZE);
	
	/* Creates an endpoint for the internet communication and returns a file
	 * descriptor that refers to that endpoint:
	 * AF_INET      IPv4 Internet protocols.
	 * SOCK_DGRAM   supports datagrams (connectionless, unreliable messages
	 *              of a fixed maximum length).
	 * IPPROTO_UDP  for udp(7) datagram sockets.
	 *  */
	ip->sid = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
	OS_TRAP_IF(ip->sid == -1);

	/* Test the peer reference point. */
	if (is_server) {
		/* Only allow a specific van client IP address. */
		
	}
	
	/* XXX */
	
	/* Leave the critical section. */
	os_cs_leave(&is.mutex);

	/* Id of the internet end point. */
	return i;
}

/**
 * os_inet_exit() - test the state of the shared memory devices.
 *
 * Return:	None.
 **/
void os_inet_exit(void)
{
	int i;
	
	/* Test the state of all internet end points. */
	for (i = 0; i < INET_COUNT; i++)
		OS_TRAP_IF(is.inet[i] != NULL);
	
	/* Release the protection mutex. */
	os_cs_destroy(&is.mutex);
}

/**
 * os_inet_init() - trigger the installation of the internet devices.
 *
 * Return:	None.
 **/
void os_inet_init(void)
{
	/* Create the mutex to protect the os_inet_open() and os_inet_close
	 * operation. */
	os_cs_init(&is.mutex);
}
