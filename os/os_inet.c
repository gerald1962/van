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
#include <errno.h>       /* ISO C99 Standard: 7.5 Errors: errno. */
#include <netdb.h>       /* Network database operations: gai_strerror(). */

#include "os.h"          /* Operating system: os_inet_sopen() */
#include "os_private.h"  /* Local interfaces of the OS: os_trap_init() */

/*============================================================================
  EXPORTED INCLUDE REFERENCES
  ============================================================================*/
/*============================================================================
  LOCAL NAME CONSTANTS DEFINITIONS
  ============================================================================*/
#define INET_ADDR_LEN     32  /* Max. length of the IP address string. */
#define INET_COUNT         2  /* Number of the peers. */
#define INET_THR_QSIZE     1  /* Input queue size of the inet threads. */
#define INET_MQ_SIZE    2048  /* SIze of the I/O queues. */
#define INET_MTU_SIZE    512  /* Max. size of a message. */
#define INET_CE_SIZE     512  /* Buffer size for the connection establishment. */

/* Accept message for the connection establishment. */
#define INET_ACCEPT_MSG  "user=vdisplay#"
#define INET_ACCEPT_LEN  (sizeof(INET_ACCEPT_MSG) - 1)

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
 * @sock:     socket address.
 **/
typedef struct inet_sock_s {
	char   ip_addr[INET_ADDR_LEN];
	int    port;
	struct sockaddr_in  sock;
} inet_sock_t;

/**
 * inet_thr_t - state of the receiver or send thread.
 *
 * @tid:        id of the receiver or send thread.
 * @suspend:    rx/tx thread control semaphore.
 * @suspended:  if 1, the rx/tx thread shall be controlled  by suspend.
 * @down:       If 1, initiate the shut down of the rx/tx thread.
 **/
typedef struct inet_thr_s {
	void   *tid;
	sem_t   suspend;
	atomic_int  suspended;
	atomic_int  down;
} inet_thr_t;

/**
 * inet_t - state of the van internet end point.
 *
 * @cid:       communication id of the internet end point.
 * @in:        input queue, written from the receving thread.
 * @out:       output queue, read from the send thread.
 * @sid:       socket file descriptor.
 * @seqno:     sequence number of a message.
 * @recv_thr:  state of the receiver thread.
 * @send_thr:  state of the send thread.
 * @my_addr:   socket address of the local peer.
 * @his_addr:  socket address of the remote peer.
 **/
typedef struct inet_s {
	int    cid;
	void  *in;
	void  *out;
	int    sid;
	int    seqno;
	char   ce_buf[INET_CE_SIZE];
	inet_thr_t   recv_thr;
	inet_thr_t   send_thr;
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
 * inet_snd_exec() - send internet packets to the remote host.
 *
 * @msg:  generic input message.
 *
 * Return:	None.
 **/
static void inet_snd_exec(os_queue_elem_t *g_msg)
{
	struct sockaddr *addr;
	inet_thr_t  *thr;
	socklen_t  addr_len;
	inet_t *ip;
	char *buf;
	int down, size, rv, err;
#if 0
	int snd_count, n;
#endif

	/* Decode the pointer to the inet state. */
	ip = g_msg->param;

	/* Get the pointer to the state of the receiving thread. */
	thr = &ip->send_thr;

	/* Get the pointer to the socket descripton. */
	addr = (struct sockaddr *) &ip->his_addr.sock;
	addr_len = sizeof(ip->his_addr.sock);

#if 0
	snd_count = 0;
#endif
	/* Loop thru the signals, which are exchanged with the controller and
	 * display. */
	for (;;) {
		/* Test the thread state. */
		down = atomic_load(&thr->down);
		if (down)
			return;

		/* Send all pending messages. */
		for(;;) {
			/* Get the pointer to the message. */
			buf = os_mq_get(ip->out, &size);
			if (buf == NULL)
				break;
#if 0
			/* Convert and test the send counter. */
			n = strtol(buf, NULL, 10);
			OS_TRAP_IF(snd_count != n);
			snd_count++;
#endif

			/* Blocking transmission of a message to another
			 * socket. */
			rv = sendto(ip->sid, buf, size, 0, addr, addr_len);

			/* Test the return value. */
			if (rv != size) {
				/* Copy and print the error code. */
				err = errno;
				printf("%s: rv=%d, size=%d, errno=%d\n",
				       F, rv, size, err);
			}
			
			OS_TRAP_IF(rv == -1);

			/* Remove the message from the output queue. */
			os_mq_remove(ip->out, size);
		}

		/* Start the suspend actions. */
		atomic_store(&thr->suspended, 1);
		
		/* Wait for the resume trigger from the controller or display. */
		os_sem_wait(&thr->suspend);

		/* The send thread has been resumed. */
		atomic_store(&thr->suspended, 0);
	}
}

/**
 * inet_rcv_exec() - wait and process internet packets from the remote host.
 *
 * @msg:  generic input message.
 *
 * Return:	None.
 **/
static void inet_rcv_exec(os_queue_elem_t *g_msg)
{
	struct sockaddr *addr;
	inet_thr_t  *thr;
	socklen_t  addr_len;
	inet_t *ip;
	char *buf, *s;
	int calling, down, size, err;

	/* Decode the pointer to the inet state. */
	ip = g_msg->param;

	/* Get the pointer to the state of the receiving thread. */
	thr = &ip->recv_thr;
	
	/* Get the pointer to the socket descripton. */
	addr = (struct sockaddr *) &ip->his_addr.sock;
	addr_len = sizeof(ip->his_addr.sock);

	/* Test the call status. */
	calling = 1;
	
	/* Loop thru the signals, which are exchanged with the controller and
	 * display. */
	for (;;) {
		/* Test the thread state. */
		down = atomic_load(&thr->down);
		if (down)
			return;

		/* Receive all pending messages. */
		for (;;) {
			/* Reserve a message buffer. */
			buf = os_mq_alloc(ip->in, INET_MTU_SIZE);

			/* Test the buffer state. */
			if (buf == NULL)
				break;
			
			/* Blocking receipt of a message from a socket. */
			size = recvfrom(ip->sid, buf, INET_MTU_SIZE, 0,
					addr, &addr_len); 
			if (size == 0) {
				/* Release the reserved message buffer. */
				os_mq_free(ip->in);
				break;
			}
			
			/* Test the return value of the receive operation. */
			if (size == -1) {
				/* Copy and print the error code. */
				err = errno;
				printf("%s: errno=%d\n", F, err);
			}
			
			OS_TRAP_IF(size == -1);

			/* Test the message format. */
			OS_TRAP_IF(buf[size - 1] != '\0');
			
			/* Test the receive phase. */
			if (calling) {
				/* Search for a ring message. */
				s = strstr(buf, ":mode=calling:");

				/* Test the message type. */
				if (s != NULL) {
					/* Discard a call message. */
					os_mq_free(ip->in);
					continue;
				}

				/* Leave the call phase. */
				calling = 0;
			}

			/* Replace end of string with the message delimiter. */
			buf[size - 1] = '#';
	
			/* Complete the receive operation. */
			os_mq_add(ip->in, size);
		}

		/* Start the suspend actions. */
		atomic_store(&thr->suspended, 1);
		
		/* Wait for the resume trigger from the controller or display. */
		os_sem_wait(&thr->suspend);

		/* The receiving thread has been resumed. */
		atomic_store(&thr->suspended, 0);
	}
}

/**
 * inet_threads_start() - start the receiver and send thread.
 *
 * @ip:  pointer to the inet state.
 *
 * Return:	None.
 **/
static void inet_threads_start(inet_t *ip)
{
	os_queue_elem_t msg;
	
	/* Start the receiving thread. */
	os_memset(&msg, 0, sizeof(msg));
	msg.param = ip;
	msg.cb    = inet_rcv_exec;
	OS_SEND(ip->recv_thr.tid, &msg, sizeof(msg));

	/* Start the send thread. */
	os_memset(&msg, 0, sizeof(msg));
	msg.param = ip;
	msg.cb    = inet_snd_exec;
	OS_SEND(ip->send_thr.tid, &msg, sizeof(msg));
}

/**
 * os_inet_connect_req() - analyze the connect request from vdisplay peer.
 *
 * @ip:  pointer to the inet state.
 *
 * Return:	0 if the vdisplay is online, otherwise -1.
 **/
static int os_inet_connect_req(inet_t *ip)
{
	socklen_t len, rv;
	char *s;
	
	/* Unblocking read of the request from the vdisplay. */
	len = sizeof(ip->his_addr.sock);
	rv = recvfrom(ip->sid, ip->ce_buf, INET_CE_SIZE, MSG_DONTWAIT,
		      (struct sockaddr *) &ip->his_addr.sock, &len);
	if (rv < 1)
		return -1;

	/* Analyze the vdisplay request. */
	s = strstr(ip->ce_buf, ":peer=vdisplay:");

	/* Calculate the return value. */
	rv = (s == NULL) ? -1 : 0;

	return rv;
}

/**
 * os_inet_connect_rsp() - analyze the vcontroller response.
 *
 * @ip:  pointer to the inet state.
 *
 * Return:	0 if the vcontroller is online, otherwise -1.
 **/
static int os_inet_connect_rsp(inet_t *ip)
{
	socklen_t len, rv;
	char *s;
	
	/* Test the sequence number. */
	if (ip->seqno < 1)
		return -1;
	
	/* Unblocking read of the response from the vcontroller. */
	len = sizeof(ip->his_addr.sock);
	rv = recvfrom(ip->sid, ip->ce_buf, INET_CE_SIZE, MSG_DONTWAIT,
		      (struct sockaddr *) &ip->his_addr.sock, &len);
	if (rv < 1)
		return -1;

	/* Analyze the vcontroller response. */
	s = strstr(ip->ce_buf, ":peer=vcontroller:");

	/* Calculate the return value. */
	rv = (s == NULL) ? -1 : 0;

	return rv;
}

/**
 * inet_sock_create() - install the socket.
 *
 * @ip:  pointer to the inet state.
 *
 * Return:	None.
 **/
static void inet_sock_create(inet_t *ip)
{
	socklen_t len;
	int size, rv;
	
	/* Creates an endpoint for the internet communication and returns a file
	 * descriptor that refers to that endpoint:
	 * AF_INET      IPv4 Internet protocols.
	 * SOCK_DGRAM   supports datagrams (connectionless, unreliable messages
	 *              of a fixed maximum length).
	 * IPPROTO_UDP  for udp(7) datagram sockets.
	 *  */
	ip->sid = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
	OS_TRAP_IF(ip->sid == -1);

	/* Use 30 MB for the kernel UDP read and write buffer. */
	size = 30 * 1024 * 1024;
	len = sizeof(int);

	/* Set the receive buffer size on the socket. */
	rv = setsockopt(ip->sid, SOL_SOCKET, SO_RCVBUF, &size, len);
	OS_TRAP_IF(rv != 0);

	/* Set the send buffer size on the socket. */
	rv = setsockopt(ip->sid, SOL_SOCKET, SO_SNDBUF, &size, len);
	OS_TRAP_IF(rv != 0);

	/* When a socket is created with socket(), it exists in a name space
	 * (address family) but has no address assigned to it. bind() assigns
	 * the address specified by addr to the socket referred to by the file
	 * descriptor sockfd. */
	rv = bind(ip->sid, (struct sockaddr *) &ip->my_addr.sock,
		  sizeof(ip->my_addr.sock));
	OS_TRAP_IF(rv != 0);
}

/**
 * inet_sock_addr() - define the socket resources of the remote or local peer.
 *
 * @s:        pointer to the socket description.
 * @ip_addr:  pointer to the IP address string.
 * @port:     port number.
 *
 * Return:	None.
 **/
static void inet_sock_addr(inet_sock_t *s, const char *ip_addr, int port)
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

/**
 * inet_thr_cleanup() - free the resources resources of the receiving of send
 * thread.
 *
 * @thr:    pointer to the thread state.
 *
 * Return:	None.
 **/
static void inet_thr_cleanup(inet_thr_t *thr)
{
	/* Trigger for the thread, to terminate it. */
	atomic_store(&thr->down, 1);

	/* Start with the shut down operations. */
	os_sem_release(&thr->suspend);

	/* Remove the receiving or send thread. */
	os_thread_destroy(thr->tid);

	/* Destroy the control semaphore for the thread. */
	os_sem_delete(&thr->suspend);
}

/**
 * inet_thr_init() - create the resources for the receiving or send thread.
 *
 * @cid:    inet communication id.
 * @thr:    pointer to the thread state.
 * @infix:  infix of the thread name.
 *
 * Return:	None.
 **/
static void inet_thr_init(int cid, inet_thr_t *thr, char *infix)
{
	char name[OS_THREAD_NAME_LEN];
	int n;
	
	/* Build the thread name. */
	n = snprintf(name, OS_THREAD_NAME_LEN, "inet_%s_%d", infix, cid);
	OS_TRAP_IF(n >= OS_THREAD_NAME_LEN);
	
	/* Start the receiving or send thread. */
	thr->tid = os_thread_create(name, INET_THR_PRIO, INET_THR_QSIZE);

	/* Create the control semaphore for the receiving thread, e.g. */
	os_sem_init(&thr->suspend, 0);
}

/*============================================================================
  EXPORTED FUNCTIONS
  ============================================================================*/
/**
 * os_inet_sync() - get the fill level of the output queue buffer.
 *
 * @cid:  socket communication id.
 *
 * Return:	the fill level of the output queue.
 **/
int os_inet_sync(int cid)
{
	inet_t *ip;
	int n;
	
	/* Entry conditon. */
	OS_TRAP_IF(cid < 0 || cid >= INET_COUNT);

	/* Map the cid to the inet state. */
	ip = is.inet[cid];

	/* Test the inet state. */
	OS_TRAP_IF(ip == NULL);

	/* Get the fill level of the output queue. */
	n = os_mq_rmem(ip->out);
	
	return n;
}

/**
 * os_inet_write() - write to an internet socket. os_inet_write() writes up to
 * count bytes from the buffer starting at buf to the intnet socket referred to
 * by the communication id cid.
 * The number of bytes written may be less than count if, for example, there is
 * insufficient space on the underlying buffer.
 * On success, the number of bytes written is returned.
 * On error, -1 is returned.
 * Note that a successful os_inet_write() may transfer fewer than count bytes. Such
 * partial writes can occur for example, because there was insufficient space on
 * the buffer to write all of the requested bytes.
 * If no errors are detected, or error detection is not performed, 0 will be
 * returned without causing any other effect.
 *
 * @cid:    socket communication id.
 * @buf:    pointer to the source buffer.
 * @count:  fill level of the source buffer.
 *
 * Return:	the number of bytes written.
 **/
int os_inet_write(int cid, char *buf, int count)
{
	inet_t *ip;
	int rv, suspended;
	
	/* Entry conditon. */
	OS_TRAP_IF(cid < 0 || cid >= INET_COUNT ||
		   buf == NULL || count < 0 || count >= INET_MTU_SIZE);

	/* Test the size of the destination buffer. */
	if (count < 1)
		return 0;
	
	/* Map the cid to the inet state. */
	ip = is.inet[cid];

	/* Test the inet state. */
	OS_TRAP_IF(ip == NULL);

	/* Write to the output queue. */
	rv = os_mq_write(ip->out, buf, count);

	/* Get the state of the send thread. */
	suspended = atomic_load(&ip->send_thr.suspended);
	
	/* Test the state of the send thread. */
	if (suspended) {
		/* Resume the send thread. */
		os_sem_release(&ip->send_thr.suspend);
	}
	
	/* Inform the user, that the output message has been saved or none. */
	return rv ? count : 0;
}

/**
 * os_inet_read() - read from an internet socket. os_inet_read() attempts to
 * read up to count bytes from the internet socket into the buffer starting
 * at buf.
 *
 * @cid     internet communication id.
 * @buf:    pointer to the destination buffer.
 * @count:  size of the receive buffer.
 *
 * Return:	the number of bytes read.
 **/
int os_inet_read(int cid, char *buf, int count)
{
	inet_t *ip;
	int size, suspended;
	
	/* Entry conditon. */
	OS_TRAP_IF(cid < 0 || cid >= INET_COUNT ||
		   buf == NULL || count < 0);

	/* Test the size of the destination buffer. */
	if (count < 1)
		return 0;
	
	/* Map the cid to the inet state. */
	ip = is.inet[cid];

	/* Test the inet state. */
	OS_TRAP_IF(ip == NULL);

	/* Copy the next queue element. */
	size = os_mq_read(ip->in, buf, count);

	/* Get the state of the receiving thread. */
	suspended = atomic_load(&ip->recv_thr.suspended);
	
	/* Test the state of the receiving thread. */
	if (suspended) {
		/* Resume the receiving thread. */
		os_sem_release(&ip->recv_thr.suspend);
	}
	
	return size;
}

/**
 * os_inet_connect() - send the connection establishment message to the vcontroller.
 *
 * @cid:  internet connection id.
 *
 * Return:	0, if the vcontroller peer is active.
 **/
int os_inet_connect(int cid)
{
	inet_t *ip;
	int rv, len;

	/* Enter the critical section. */
	os_cs_enter(&is.mutex);

	/* Entry point. */
	OS_TRAP_IF(cid < 0 || cid >= INET_COUNT ||  is.inet[cid] == NULL ||
		   is.inet[cid]->cid != cid);

	/* Get the pointer to the peer state. */
	ip = is.inet[cid];

	/* Analyze the vcontroller response. */
	rv = os_inet_connect_rsp(ip);
	if (rv == 0) {
		/* Start the receiving and send thread. */
		inet_threads_start(ip);
		goto l_end;
	}
	
	/* Increment the sequence number. */
	ip->seqno++;

	/* Frame the request to the vcontroller. */
	rv = snprintf(ip->ce_buf, INET_CE_SIZE, "seqno=%d::mode=calling::peer=vdisplay:",
		      ip->seqno);
	
	/* Include EOS. */
	rv++;	
	OS_TRAP_IF(rv >= INET_CE_SIZE);
	
	/* Unblocking transmission of a message to another socket. */
	len = sizeof(ip->his_addr.sock);
	sendto (ip->sid, ip->ce_buf, rv, MSG_DONTWAIT,
		(struct sockaddr *) &ip->his_addr.sock, len);

	/* Update the return value. */
	rv = -1;

l_end:
	/* Leave the critical section. */
	os_cs_leave(&is.mutex);

	return rv;
}

/**
 * os_inet_accept() - wait for the vdisplay peer.
 *
 * @cid:  internet connection id.
 *
 * Return:	0, if the vdisplay peer is active.
 **/
int os_inet_accept(int cid)
{
	inet_t *ip;
	int rv, len;

	/* Enter the critical section. */
	os_cs_enter(&is.mutex);

	/* Entry point. */
	OS_TRAP_IF(cid < 0 || cid >= INET_COUNT ||  is.inet[cid] == NULL ||
		   is.inet[cid]->cid != cid);

	/* Get the pointer to the internet end point state. */
	ip = is.inet[cid];

	/* Analyze the connect request from vdisplay peer. */
	rv = os_inet_connect_req(ip);
	if (rv != 0)
		goto l_end;

	/* Increment the sequence number. */
	ip->seqno++;

	/* Frame the response to the vdisplay. */
	rv = snprintf(ip->ce_buf, INET_CE_SIZE, "seqno=%d::peer=vcontroller:",
		      ip->seqno);
	
	/* Include EOS. */
	rv++;
	OS_TRAP_IF(rv >= INET_CE_SIZE);
	
	/* Unblocking transmission of a message to another socket. */
	len = sizeof(ip->his_addr.sock);
	rv = sendto (ip->sid, ip->ce_buf, rv, MSG_DONTWAIT,
		     (struct sockaddr *) &ip->his_addr.sock, len);
	OS_TRAP_IF(rv == -1);
	
	/* Start the receiving and send thread. */
	inet_threads_start(ip);

	
	/* Update the return value. */
	rv = 0;

l_end:
	/* Leave the critical section. */
	os_cs_leave(&is.mutex);

	return rv;
}

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
	OS_TRAP_IF(cid < 0 || cid >= INET_COUNT ||  is.inet[cid] == NULL ||
		   is.inet[cid]->cid != cid);

	/* Get the pointer to the internet end point state. */
	ip = is.inet[cid];

	/* Shut down part of a full-duplex connection. Further receptions and
	 * transmissions will be disallowed. */
	shutdown(ip->sid, SHUT_RDWR);

	/* Remove the endpoint for the internet communication. */
	rv = close(ip->sid);
	OS_TRAP_IF(rv == -1);

	/* Free the send thread resources. */
	inet_thr_cleanup(&ip->send_thr);

	/* Free the receiving thread resources. */
	inet_thr_cleanup(&ip->recv_thr);

	/* Free the I/O queues */
	os_mq_delete(ip->in);
	os_mq_delete(ip->out);

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
	int my_len, his_len, lower_p, upper_p, i;
	
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

	/* Reset the inet element. */
	/* Allocate memory for the state of the internet end point. */
	*p = OS_MALLOC(sizeof(inet_t));
	ip = *p;
	os_memset(ip, 0, sizeof(inet_t));

	/* Save the connection id. */
	ip->cid = i;

	/* Create the I/O queues. */
	ip->in  = os_mq_init(INET_MQ_SIZE);
	ip->out = os_mq_init(INET_MQ_SIZE);

	/* Define the socket address of the local peer. */
	inet_sock_addr(&ip->my_addr, my_addr, my_p);
	
	/* Define the socket address of the remote peer. */
	inet_sock_addr(&ip->his_addr, his_addr, his_p);

	/* Install the socket. */
	inet_sock_create(ip);

	/* Create the resources for the receiving thread. */
	inet_thr_init(i, &ip->recv_thr, "rcv");
	
	/* Create the resources for the send thread. */
	inet_thr_init(i, &ip->send_thr, "snd");
	
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
