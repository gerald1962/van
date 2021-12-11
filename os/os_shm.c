// SPDX-License-Identifier: GPL-2.0

/*
 * Shared memory interfaces.
 *
 * Copyright (C) 2021 Gerald Schueller <gerald.schueller@web.de>
 */

/*============================================================================
  IMPORTED INCLUDE REFERENCES
  ============================================================================*/
#include <fcntl.h>       /* For O_* constants. */
#include <sys/stat.h>    /* For mode constants. */
#include <unistd.h>      /* File operationens: close(). */
#include <sys/mman.h>    /* Map file into memory. */
#include <errno.h>       /* Number of the last error: errno. */
#include "os.h"          /* Operating system: os_sem_create() */

/*============================================================================
  EXPORTED INCLUDE REFERENCES
  ============================================================================*/
#include "os_private.h"  /* Local interfaces of the OS: os_trap_init() */

/*============================================================================
  LOCAL NAME CONSTANTS DEFINITIONS
  ============================================================================*/
#define OS_SHM_FILE  "shm.txt"   /* Name of the shared memory file. */
#define OS_SHM_SIZE  8192        /* Expected size of the shm file. */
#define OS_BUF_SIZE  2048        /* Size of the UL/DL transfer buffer. */
#define OS_VAN_INT   "/van_int"  /* Van interrupt simulation. */
#define OS_PY_INT    "/py_int"   /* Python interrupt simulation. */
#define OS_THREAD_Q_SIZE  8      /* Input queue size of the van/py thread. */
#define OS_SHM_Q_SIZE     4      /* Data transfer queue size about shm. */

/*============================================================================
  MACROS
  ============================================================================*/

/* Prompt for the van_int thread. */
#define VP  "V-I>"

/* Prompt for the py_int thread. */
#define PP  "P-I>"

/*============================================================================
  LOCAL TYPE DEFINITIONS
  ============================================================================*/
/**
 * os_shm_msg_t - shm input message with payload status information.
 *
 * @size:      size of the payload.
 * @consumed:  if 1, the payload has been processed.
 **/
typedef struct {
	int  size;
	int  consumed;
} os_shm_msg_t;

/**
 * os_shm_queue_t - shm input queue of van or py.
 *
 * @ring:  list of the input messages.
 * @tail:  start of the message list.
 * @head:  end of the message list.
 **/
typedef struct {
	os_shm_msg_t  ring[OS_SHM_Q_SIZE];
	int  tail;
	int  head;
} os_shm_queue_t;

/** 
 * os_shm_top_t - shm topology.
 *
 * @pqueue:        python shm queue: van -> py.
 * @dl_buf_size:   size of the DL buffer: van->py.
 * @dl_buf_start:  start address of the DL buffer.
 * @dl_buf_end:    end address of the DL buffer.
 *
 * @vqueue:        van shm queue: py -> van.
 * @ul_buf_size:   size of the UL buffer: py->van.
 * @ul_buf_start:  start address of the UL buffer.
 * @ul_buf_end:    end address of the UL buffer.
 **/
typedef struct {
	os_shm_queue_t  *pqueue;
	int    dl_buf_size;
	char  *dl_buf_start;
	char  *dl_buf_end;

	os_shm_queue_t  *vqueue;
	int    ul_buf_size;
	char  *ul_buf_start;
	char  *ul_buf_end;
} os_shm_top_t;

/**
 * os_shm_t - shared memory state.
 *
 * @init:              1, if the shm area is available.
 * @file_name:         name of the shared memory file.
 * @fd:                file descriptor of the shm file.
 * @size:              size of the shm file.
 * @start:             start address of the mapped area.
 * @my_int_name:       name of the van/py interrupt.
 * @other_int_name:    name of the py/van interrupt.
 * @my_int:            points to the named van/py semaphore.
 * @other_int:         points to the named py/van semaphore.
 * @thread:            address of the van/py int handler/thread.
 * @pending_transfer:  if 1, there is a pending UL/DL transfer.
 * @down:              if 1, ignore the py/van interrupt.
 * @mutex:             protect the critical sections.
 * @top:               topology of the shm area.
 **/
typedef struct {
	int     init;
	char   *file_name;
	int     fd;
	off_t   size;
	void   *start;
	char   *my_int_name;
	char   *other_int_name;
	sem_t  *my_int;
	sem_t  *other_int;
	void   *thread;
	atomic_int       pending_transfer;
	atomic_int       down;
	pthread_mutex_t  mutex;
	os_shm_top_t     top;
} os_shm_t;

/*============================================================================
  LOCAL DATA
  ============================================================================*/
/* Van shared memory state. */
static os_shm_t os_shm_van;

/* Python shared memory state. */
static os_shm_t os_shm_py;

/*============================================================================
  LOCAL FUNCTION PROTOTYPES
  ============================================================================*/
/*============================================================================
  LOCAL FUNCTIONS
  ============================================================================*/

/**
 * os_py_exit_exec() - the py_int thread shall not be suspended with named
 * py_int semphore.
 *
 * @msg:  generic input message.
 *
 * Return:	None.
 **/
static void os_py_exit_exec(os_queue_elem_t *msg)
{
	printf("%s [s:ready, m:py-exit] -> [s:ready]\n", PP);
}

/* XXX */
#if 0
static void os_py_write_exec(os_queue_elem_t *msg)
{
	os_py_wr_msg_t *m;
	os_shm_sr_t *sr;
	
	/* Entry condition. */
	TRAP_IF(msg == NULL);
	
	/* Decode the input message. */
	m = (os_py_wr_msg_t *) msg;

	/* Get the reference to the status register. */
	sr = os_shm_py.top.sr;

	/* Test the state of the UL buffer. */
	if (m->ul_size > 0) {
		sr->py_ul_sent = 1;
		sr->py_ul_size = m->ul_size;
	}
	
	/* Test the state of the DL buffer. */
	if (m->dl_consumed)
		sr->py_dl_released = 1;
	
	/* Update the status register and trigger the van_int. */
	sr->van_int_triggered = 1;
	os_sem_release(&os_shm_py.other_int);
}
#endif

/**
 * os_py_int_exec() - the py_int thread waits for the py_int triggered by van or
 * internally to process a send order.
 *
 * @msg:  generic input message.
 *
 * Return:	None.
 **/
static void os_py_int_exec(os_queue_elem_t *msg)
{
	os_queue_elem_t q_msg;
	os_shm_queue_t *q;
	os_shm_msg_t *shm_m;
	os_shm_t *p;
	int down;

	printf("%s [s:ready, m:py-int] -> [s:suspended]\n", VP);

	/* Get the address of the py shared memory state. */
	p = &os_shm_py;

	/* Wait for the py_int trigger. */
	os_sem_wait(p->my_int);

	/* Test the thread state. */
	down = atomic_load(&p->down);
	if (down) {
		printf("%s [s:suspended, m:py-int] -> [s:down]\n", VP);
		return;
	} else {
		printf("%s [s:suspended, m:py-int] -> [s:ready]\n", VP);
	}

	/* Get the pointer of the py shm input queue. */
	q = p->top.pqueue;

	/* Analyze the py shm input queue. */
	while (q->tail < q->head) {
		shm_m = &q->ring[q->tail];

		/* Test the DL payload state from van. */
		if (shm_m->size > 0) {
			/* XXX */
		}

		/* Test the state of sent UL playload to van. */
		if (shm_m->consumed)
			atomic_store(&p->pending_transfer, 0);
		
		/* Increment and test the start of the message list. */
		q->tail++;
		if (q->tail >= OS_SHM_Q_SIZE)
			q->tail = 0;
	}

	/* Test the state of the van_int input queue. */
	os_memset(&q_msg, 0, sizeof(q_msg));
	q_msg.param = p->thread;
	q_msg.cb    = os_py_int_exec;
	OS_SEND(p->thread, &q_msg, sizeof(q_msg));
}

/**
 * os_van_exit_exec() - the van_int thread shall not be suspended with named
 * van_int semphore.
 *
 * @msg:  generic input message.
 *
 * Return:	None.
 **/
static void os_van_exit_exec(os_queue_elem_t *msg)
{
	printf("%s [s:ready, m:van-exit] -> [s:ready]\n", VP);
}

/* XXX */
#if 0
/**
 * os_van_wr_msg_t - initiate the DL transfer: van->python.
 *
 * @OS_QUEUE_MSG_HEAD:  generic message header.
 * @dl_size:            size of the DL buffer.
 * @ul_consumed:        if 1, van has processed the UL payload.
 **/
typedef struct {
	OS_QUEUE_MSG_HEAD;
	int dl_size;
	int ul_consumed;
} os_van_wr_msg_t;
#endif

/* XXX */
#if 0
/**
 * void os_van_write_exec() - trigger python to process the DL payload.
 *
 * @msg:  generic input message.
 *
 * Return:	None.
 **/
static void os_van_write_exec(os_queue_elem_t *msg)
{
	os_van_wr_msg_t *m;
	os_shm_sr_t *sr;
	
	/* Entry condition. */
	TRAP_IF(msg == NULL);
	
	/* Decode the input message. */
	m = (os_van_wr_msg_t *) msg;

	/* Get the reference to the status register. */
	sr = os_shm_van.top.sr;

	/* Test the state of the DL buffer. */
	if (m->dl_size > 0) {
		sr->van_dl_sent = 1;
		sr->van_dl_size = m->dl_size;
	}
	
	/* Test the state of the UL buffer. */
	if (m->ul_consumed)
		sr->van_ul_released = 1;
	
	/* Update the status register and trigger the py_int. */
	sr->py_int_triggered = 1;
	os_sem_release(&os_shm_van.other_int);
}
#endif

/**
 * os_van_int_exec() - the van_int thread waits for the van_int triggered by py
 * or internally to process a send order.
 *
 * @msg:  generic input message.
 *
 * Return:	None.
 **/
static void os_van_int_exec(os_queue_elem_t *msg)
{
	os_queue_elem_t q_msg;
	os_shm_queue_t *q;
	os_shm_msg_t *shm_m;
	os_shm_t *v;
	int down;

	printf("%s [s:ready, m:van-int] -> [s:suspended]\n", VP);

	/* Get the address of the van shared memory state. */
	v = &os_shm_van;

	/* Wait for the van_int trigger. */
	os_sem_wait(v->my_int);

	/* Test the thread state. */
	down = atomic_load(&v->down);
	if (down) {
		printf("%s [s:suspended, m:van-int] -> [s:down]\n", VP);
		return;
	} else {
		printf("%s [s:suspended, m:van-int] -> [s:ready]\n", VP);
	}
	
	/* Get the pointer of the van shm input queue. */
	q = v->top.vqueue;

	/* Analyze the van shm input queue. */
	while (q->tail < q->head) {
		shm_m = &q->ring[q->tail];

		/* Test the UL payload state from py. */
		if (shm_m->size > 0) {
			/* XXX */
		}

		/* Test the state of sent DL playload to py. */
		if (shm_m->consumed)
			atomic_store(&v->pending_transfer, 0);
		
		/* Increment and test the start of the message list. */
		q->tail++;
		if (q->tail >= OS_SHM_Q_SIZE)
			q->tail = 0;
	}

	/* Test the state of the van_int input queue. */
	os_memset(&q_msg, 0, sizeof(q_msg));
	q_msg.param = v->thread;
	q_msg.cb    = os_van_int_exec;
	OS_SEND(v->thread, &q_msg, sizeof(q_msg));
}

/**
 * os_shm_close - delete the access and the mapping to the shared memory.
 *
 * @s:  pointer to the shared memory state.
 *
 * Return:	None.
 **/
static void os_shm_close(os_shm_t *s)
{
	int rv;
	
	/* Remove the reference to the van semaphore. */
	rv = sem_close(s->my_int);
	OS_TRAP_IF(rv != 0);
	
	/* Remove the reference to the python semaphore. */
	rv = sem_close(s->other_int);
	OS_TRAP_IF(rv != 0);

	/* Delete the mapping of the shared memory area. */
	rv = munmap(s->start, s->size);
	
	/* Close the shared memory file. */
	rv = close(s->fd);
	OS_TRAP_IF(rv < 0);

	/* Delete the mutex for the critical sections. */
	os_cs_destroy(&s->mutex);
}

/**
 * os_shm_open - map the file into shared memory.
 *
 * @s:  pointer to the shared memory state.
 *
 * Return:	None.
 **/
static void os_shm_open(os_shm_t *s)
{
	os_shm_top_t *t;
	struct stat statbuf;
	int    rv, prot;
	void  *p;

	/* Open the shared memory file. */
	s->file_name = OS_SHM_FILE;
	s->fd = open(OS_SHM_FILE, O_RDWR);
	OS_TRAP_IF(s->fd < 0);

	/* Get information about the shared memory file. */
	rv = fstat(s->fd, &statbuf);
	s->size = statbuf.st_size;
	OS_TRAP_IF(rv != 0 || statbuf.st_size != OS_SHM_SIZE);

	/* Define the access to the shared memory. */
	prot = PROT_READ | PROT_WRITE;
	
	/* Map the file into shared memory. */
	s->start = mmap(NULL, s->size, prot, MAP_SHARED, s->fd, 0);
	OS_TRAP_IF(s->start == MAP_FAILED);

	/* Create the mutex for the critical sections. */
	os_cs_init(&s->mutex);
	
	/* Get the reference to the shm topology. */
	t = &s->top;

	/* Copy the pointer to the start of the shm. */
	p = s->start;

	/* Map the data transfer queues to the shm. */
	t->pqueue = (os_shm_queue_t *) p;
	p = p + sizeof(os_shm_queue_t);
	
	t->vqueue = (os_shm_queue_t *) p;
	p = p + sizeof(os_shm_queue_t);
	
	/* Map the UL buffer to the shm. */
	t->ul_buf_size  = OS_BUF_SIZE;
	t->ul_buf_start = (char *) p;
	t->ul_buf_end   = (char *) p + OS_BUF_SIZE - 1;
	p = (char *) p + OS_BUF_SIZE;
	
	/* Map the DL buffer to the shm. */
	t->dl_buf_size  = OS_BUF_SIZE;
	t->dl_buf_start = (char *) p;
	t->dl_buf_end   = (char *) p + OS_BUF_SIZE - 1;
}

/*============================================================================
  EXPORTED FUNCTIONS
  ============================================================================*/
/**
 * os_vcreate - the van process shall call this function to create the named
 * semaphores.
 *
 * Return:	None.
ŵ **/
void os_vcreate(void)
{
	os_queue_elem_t msg;
	os_shm_t *v;
	mode_t mode;
	int oflag;

	/* Get the address of the van shared memory state. */
	v = &os_shm_van;

	/* Save the interrupt names. */
	v->my_int_name    = OS_VAN_INT;
	v->other_int_name = OS_PY_INT;

	/* Control flags for the sem_open call. */
	oflag = O_CREAT | O_EXCL;
	mode =  S_IRUSR | S_IWUSR | S_IRGRP | S_IWGRP | S_IROTH | S_IWOTH;

	/* Create the named van semaphore. */
	v->my_int = sem_open(OS_VAN_INT, oflag, mode, 0);
	OS_TRAP_IF(v->my_int == SEM_FAILED);
	
	/* Create the named python semaphore. */
	v->other_int = sem_open(OS_PY_INT, oflag, mode, 0);
	OS_TRAP_IF(v->other_int == SEM_FAILED);

	/* Map the file into shared memory. */
	os_shm_open(v);

	/* Initialize the shm area. */
	os_memset(v->start, 0, v->size);

	/* Install the van interrupt handler/thread. */
	v->thread = os_thread_create("van_int", OS_THREAD_PRIO_SOFTRT, OS_THREAD_Q_SIZE);
	
	/* Change the state of the shm area. */
	v->init = 1;

	/* Resume the van_int thread, to wait for the van_int. */
	os_memset(&msg, 0, sizeof(msg));
	msg.param = v->thread;
	msg.cb    = os_van_int_exec;
	OS_SEND(v->thread, &msg, sizeof(msg));
}

/**
 * os_vdestroy - the van process shall call this function to remove the named
 * semaphores.
 *
 * Return:	None.
 **/
void os_vdestroy(void)
{
	os_queue_elem_t msg;
	os_shm_t *v;
	int rv;
	
	/* Get the address of the van shared memory state. */
	v = &os_shm_van;

	/* Change the state of the shm area. */
	v->init = 0;

	/* Define the van_int terminate message. */
	os_memset(&msg, 0, sizeof(msg));
	msg.param = v->thread;
	msg.cb    = os_van_exit_exec;
	
	/* Resume the van_int thread, to terminate it. */
	OS_SEND(v->thread, &msg, sizeof(msg));
	atomic_store(&v->down, 1);
	os_sem_release(v->my_int);
	
	/* Delete the van interrupt handler. */
	os_thread_destroy(v->thread);
	
	/* Delete the access and the mapping to the shared memory. */
	os_shm_close(v);
	
	/* Delete the named van semaphore. */
	rv = sem_unlink(v->my_int_name);
	OS_TRAP_IF(rv != 0);
	
	/* Delete the named python semaphore. */
	rv = sem_unlink(v->other_int_name);
	OS_TRAP_IF(rv != 0);
}

/**
 * os_popen - the python process shall call this function to get the reference
 * to the named semaphores.
 *
 * Return:	None.
 **/
void os_popen(void)
{
	os_queue_elem_t msg;
	os_shm_t *p;

	/* Get the address of the python shared memory state. */
	p = &os_shm_py;
	
	/* Save the interrupt names. */
	p->my_int_name    = OS_PY_INT;
	p->other_int_name = OS_VAN_INT;
	
	/* Get the reference to the python semaphore. */
	p->my_int = sem_open(OS_PY_INT, O_CREAT);
	OS_TRAP_IF(p->my_int == SEM_FAILED);
	
	/* Get the reference to the van semaphore. */
	p->other_int = sem_open(OS_VAN_INT, O_CREAT);
	OS_TRAP_IF(p->other_int == SEM_FAILED);
	
	/* Map the file into shared memory. */
	os_shm_open(p);
	
	/* Install the py interrupt handler/thread. */
	p->thread = os_thread_create("van_int", OS_THREAD_PRIO_SOFTRT,
				     OS_THREAD_Q_SIZE);	
	
	/* Change the state of the shm area. */
	p->init = 1;
	
	/* Resume the py_int thread, to wait for the py int. */
	os_memset(&msg, 0, sizeof(msg));
	msg.param = p->thread;
	msg.cb    = os_py_int_exec;
	OS_SEND(p->thread, &msg, sizeof(msg));
}

/**
 * os_pclose - the python process shall call this function to remove the
 * reference to the named semaphores.
 *
 * Return:	None.
 **/
void os_pclose(void)
{
	os_queue_elem_t msg;
	os_shm_t *p;

	/* Get the address of the python shared memory state. */
	p = &os_shm_py;
	
	/* Change the state of the shm area. */
	p->init = 0;
	
	/* Define the py_int terminate message. */
	os_memset(&msg, 0, sizeof(msg));
	msg.param = p->thread;
	msg.cb    = os_py_exit_exec;
	
	/* Resume the py_int thread, to terminate it. */
	OS_SEND(p->thread, &msg, sizeof(msg));
	atomic_store(&p->down, 1);
	os_sem_release(p->my_int);
	
	/* Delete the py interrupt handler. */
	os_thread_destroy(p->thread);
	
	/* Delete the access and the mapping to the shared memory. */
	os_shm_close(&os_shm_py);	
}

/* XXX */
#if 0
/**
 **/
void os_vwrite(char *buf, int count, int ul_consumed)
{
	os_van_wr_msg_t msg;
	os_shm_top_t *top;
	os_shm_t *v;
	int pending;

	/* Get the address of the van shared memory state. */
	v = &os_shm_van;

	/* Entry condition. */
	OS_TRAP_IF(! v->init);

	/* There is nothing to do if, ... */
	if ( ! ((buf != NULL && count > 0) || ul_consumed) )
		return;	
	
	/* Get the pointer to the shm topology and to the py input queue. */
	top = &p->top;
	sr = top->sr;
	
	/* Enter the critical section. */
	os_cs_enter(v->mutex);

	/* Prepare the van write message. */
	os_memset(&msg, 0, sizeof(msg));
	msg.param = v->thread;
	msg.cb    = os_van_wr_exec;
	
	/* Entry condition for the DL transfer. */
	if (buf != NULL && count > 0) {	
		/* Test the state of the DL buffer: van->py. */
		TRAP_IF(sr->shm_locked || top->dl_buf_size < count ||
			v->pending_transfer);

		/* Change the DL state. */
		v->pending_transfer = 1;
	
		/* Fill the DL buffer. */
		os_memcpy(top->dl_buf_start, buf, count);

		/* Update the SR element van_dl_size in the van_int context. */
		msg.dl_size = count;
	}

	/* Test the UL buffer state on van. */
	if (ul_consumed) {
		msg.ul_consumed = 1;
	}

	/* Leave the critical section. */
	os_cs_leave(v->mutex);
	
	/* Trigger the van_int internally, to process the message. */
	atomic_store(&v->internal_int, 1);
	OS_SEND(v->thread, &msg, sizeof(msg));
	os_sem_release(v->my_int);		
}

void os_vread(void)
{
}
void os_pwrite(char *buf, int count, int dl_consumed)
{
	os_py_wr_msg_t msg;
	os_shm_top_t *top;
	os_shm_sr_t *sr;
	os_shm_t *p;

	/* Get the address of the python shared memory state. */
	p = &os_shm_py;

	/* Entry condition. */
	OS_TRAP_IF(! p->init);

	/* There is nothing to do if, ... */
	if ( ! ((buf != NULL && count > 0) || dl_consumed) )
		return;	
	
	/* Get the pointer to the shm topology and to the status register. */
	top = &p->top;
	sr = top->sr;
	
	/* Enter the critical section. */
	os_cs_enter(p->mutex);

	/* Prepare the py write message. */
	os_memset(&msg, 0, sizeof(msg));
	msg.param = p->thread;
	msg.cb    = os_py_wr_exec;
	
	/* Entry condition for the UL transfer. */
	if (buf != NULL && count > 0) {	
		/* Test the state of the UL buffer: py->van. */
		TRAP_IF(sr->shm_locked || top->ul_buf_size < count ||
			p->pending_transfer);

		/* Change the UL state. */
		p->pending_transfer = 1;
	
		/* Fill the UL buffer. */
		os_memcpy(top->ul_buf_start, buf, count);

		/* Update the SR-element py_ul_size in the py_int context. */
		msg.ul_size = count;
	}

	/* Test the DL buffer state on py. */
	if (dl_consumed) {
		msg.dl_consumed = 1;
	}

	/* Leave the critical section. */
	os_cs_leave(p->mutex);
	
	/* Trigger the py_int internally, to process the message. */
	atomic_store(&p->internal_int, 1);
	OS_SEND(p->thread, &msg, sizeof(msg));
	os_sem_release(p->my_int);		
}

void os_pread(void)
{
}

#endif
