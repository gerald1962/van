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
 * inet_sock_t - state of the local or remote peer.
 *
 * @ip_addr:  IP address string.
 * @port:     port number.
 * @sock:     socket address..
 **/
typedef struct inet_sock_s {
	char   ip_addr[INET_ADDR_LEN];
	int    port;
	struct sockaddr_in  sock;
} inet_sock_t;

/**
 * inet_t - state of the van internet end point.
 *
 * @cid:       communication id of the internet end point.
 * @rcv_thr:   id of the receiver thread.
 * @snd_thr:   id of the sender thread.
 * @sid:       socket file descriptor.
 * @my_addr:   socket address of the local peer.
 * @his_addr:  socket address of the remote peer.
 **/
typedef struct inet_s {
	int    cid;
	void  *rcv_thr;
	void  *snd_thr;
	int    sid;
	inet_sock_t  my_addr;
	inet_sock_t  his_addr;	
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
/**
 * inet_sock_create() - define the socket address of the remote peer.
 *
 * @s:        pointer to the socket description.
 * @ip_addr:  pointer to the IP address string.
 * @port:     port number.
 *
 * Return:	None.
 **/
static void inet_sock_create(inet_sock_t *s, const char *ip_addr, int port)
{
	int rv;
	
	/* Save the addresses of the peers. */
	os_strcpy(s->ip_addr, INET_ADDR_LEN, ip_addr);
	s->port = port;
	
	/* Define the socket address of the peer. */
	s->sock.sin_family = AF_INET;
	s->sock.sin_port = htons(port);
	
	/* Convert the Internet host address from the IPv4 numbers-and-dots
	 * notation into binary form. */
	rv = inet_aton(ip_addr, &s->sock.sin_addr);
	OS_TRAP_IF(rv != 1);
}

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
 * @my_addr:   pointer to my peer IP address string.
 * @my_p:      my peer port number.
 * @his_addr:  pointer to the remote peer IP address string.
 * @his_p:     port number of the remove peer.
 *
 * Return:	return the connection id.
 **/
int os_inet_open(const char *my_addr, int my_p, const char *his_addr, int his_p)
{
	inet_t **p, *ip;
	char name[OS_THREAD_NAME_LEN];
	int my_len, his_len, lower_p, upper_p, i, rv;
	
	/* Enter the critical section. */
	os_cs_enter(&is.mutex);

	/* Entry condition. */
	OS_TRAP_IF(my_addr == NULL || his_addr == NULL);
	
	/* Test the length of the IP addresses. */
	my_len  = os_strlen(my_addr);
	his_len = os_strlen(his_addr);
	OS_TRAP_IF(my_len < 1 || my_len >= INET_ADDR_LEN ||
		   his_len < 1 || his_len >= INET_ADDR_LEN);

	/* The port range 49152–65535 (2^15 + 2^14 to 2^16 − 1) contains dynamic
	 * or private ports that cannot be registered with IANA. This range is
	 * used for private or customized services, for temporary purposes, and
	 * for automatic allocation of ephemeral ports: see
	 * https://en.wikipedia.org/wiki/List_of_TCP_and_UDP_port_numbers */
	lower_p = (2 << 13) + (2 << 14);
	upper_p = (2 << 15) - 1;
	OS_TRAP_IF(my_p < lower_p || my_p >= upper_p ||
		   his_p < lower_p || his_p >= upper_p);

	/* Search for a free internet end point. */
	for (i = 0, p = is.inet; i < INET_COUNT && *p != NULL; i++, p++)
		;

	/* Final condition. */
	OS_TRAP_IF(i >= INET_COUNT);

	/* Allocate memory for the state of the internet end point. */
	*p = OS_MALLOC(sizeof(inet_t));
	ip = *p;
	os_memset(ip, 0, sizeof(inet_t));

	/* Save the connection id. */
	ip->cid = i;
	
	/* Build the name of the receiver thread. */
	snprintf(name, OS_THREAD_NAME_LEN, "inet_rcv_%d", i);
	
	/* Create the receiver thread. */
	ip->rcv_thr = os_thread_create(name, INET_THR_PRIO, INET_THR_QSIZE);

	/* Build the name of the sender thread. */
	snprintf(name, OS_THREAD_NAME_LEN, "inet_snd_%d", i);
	
	/* Create the sender thread. */
	ip->snd_thr = os_thread_create(name, INET_THR_PRIO, INET_THR_QSIZE);

	/* Define the socket address of the local peer. */
	inet_sock_create(&ip->my_addr, my_addr, my_p);
	
	/* Define the socket address of the remote peer. */
	inet_sock_create(&ip->his_addr, his_addr, his_p);
	
	/* Creates an endpoint for the internet communication and returns a file
	 * descriptor that refers to that endpoint:
	 * AF_INET      IPv4 Internet protocols.
	 * SOCK_DGRAM   supports datagrams (connectionless, unreliable messages
	 *              of a fixed maximum length).
	 * IPPROTO_UDP  for udp(7) datagram sockets.
	 *  */
	ip->sid = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
	OS_TRAP_IF(ip->sid == -1);

	/* When a socket is created with socket(), it exists in a name space
	 * (address family) but has no address assigned to it. bind() assigns
	 * the address specified by addr to the socket referred to by the file
	 * descriptor sockfd. */
	rv = bind(ip->sid, (struct sockaddr *) &ip->my_addr.sock,
		  sizeof(ip->my_addr.sock));
	OS_TRAP_IF(rv != 0);

	/* Start the receiver and sender thread. */

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
