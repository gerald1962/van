// SPDX-License-Identifier: GPL-2.0

/*
 * Python shared memory driver.
 *
 * Copyright (C) 2021 Gerald Schueller <gerald.schueller@web.de>
 */

/*============================================================================
  IMPORTED INCLUDE REFERENCES
  ============================================================================*/
#include "os_shm.h"      /* Shared memory entry points. */

/*============================================================================
  EXPORTED INCLUDE REFERENCES
  ============================================================================*/
/*============================================================================
  LOCAL NAME CONSTANTS DEFINITIONS
  ============================================================================*/
#define OS_PY_NAME  "/python"  /* Name of the python shared memory device. */
#define PP          "P-I>"     /* Prompt for the py_int thread. */

#if defined(USE_OS_RT)
#define PRIO    OS_THREAD_PRIO_HARDRT
#else
#define PRIO    OS_THREAD_PRIO_SOFTRT
#endif

#define Q_SIZE  OS_THREAD_Q_SIZE

/*============================================================================
  MACROS
  ============================================================================*/
/*============================================================================
  LOCAL TYPE DEFINITIONS
  ============================================================================*/
/*============================================================================
  LOCAL DATA
  ============================================================================*/
/* Pointer to the OS configuration */
static os_conf_t *os_conf_p;

/* Python shared memory devices. */
static os_dev_t *os_py_device;

/*============================================================================
  LOCAL FUNCTION PROTOTYPES
  ============================================================================*/
/*============================================================================
  LOCAL FUNCTIONS
  ============================================================================*/
/**
 * py_aio_ulq_add() - send the UL shm message from py to van asynchronously.
 *
 * dev:       pointer to the shm device.
 * count:     size of the UL payload.
 * consumed:  if 1, the DL buffer has been released.
 *
 * Return:	None.
 **/
static void py_aio_ulq_add(os_dev_t *dev, int count, int consumed)
{
	os_shm_queue_t *q;
	os_shm_msg_t *shm_m;
	int head;
	
	/* Get the pointer of the van shm input queue. */
	q = dev->top.ul_queue;
	
	/* Fill the next free message. */
	head = q->head;
	shm_m = &q->ring[head];
	shm_m->size     = count;
	shm_m->consumed = consumed;

	/* Increment and test the end of the message list. */
	head++;
	if (head >= OS_SHM_Q_SIZE)
		head = 0;

	OS_TRAP_IF(head == q->tail);
	q->head = head;

	/* Trigger the van_int. */
	os_sem_release(dev->other_int);
}

/**
 * py_ulq_add() - send the UL shm message from py to van synchronously.
 *
 * dev:       pointer to the shm device.
 * count:     size of the UL payload.
 * consumed:  if 1, the DL buffer has been released.
 *
 * Return:	None.
 **/
static void py_ulq_add(os_dev_t *dev, int count, int consumed)
{
	os_shm_queue_t *q;
	os_shm_msg_t *shm_m;
	int head;
	
	/* Enter the critical section. */
	os_cs_enter(&dev->top.q_mutex);

	/* Get the pointer of the van shm input queue. */
	q = dev->top.ul_queue;
	
	/* Fill the next free message. */
	head = q->head;
	shm_m = &q->ring[head];
	shm_m->size     = count;
	shm_m->consumed = consumed;

	/* Increment and test the end of the message list. */
	head++;
	if (head >= OS_SHM_Q_SIZE)
		head = 0;

	OS_TRAP_IF(head == q->tail);
	q->head = head;

	/* Trigger the van_int. */
	os_sem_release(dev->other_int);

	/* Leave the critical section. */
	os_cs_leave(&dev->top.q_mutex);	
}

/**
 * py_aio_action() - install the read and write callback for the asynchronous
 * operations. The reconfiguration of the async. I/O operations is not
 * supported. It is recommended to decide themselves for aio immediately after
 * os_open before data transfer starts, in order to avoid deadlocks in the sync.
 * operations.
 *
 * @dev_id:  id of the shared memory device.
 * @cb:      pointer to the async. I/O callbacks.
 *
 * Return:	None.
 **/
void py_aio_action(int dev_id, os_aio_cb_t *cb)
{
	os_dev_t *p;
	
	/* Entry conditon. */
	p = os_py_device;
	OS_TRAP_IF(p == NULL || p->id != dev_id ||
		   cb == NULL || cb->read_cb == NULL || cb->write_cb == NULL);

	/* Enter the critical section. */
	os_cs_enter(&p->aio_mutex);

	/* Extended entry conditon. */
	OS_TRAP_IF(p->aio_use ||
		   p->pending_ul || p->sync_read || p->sync_write);

	/* Copy the async. I/O operations. */
	p->aio_cb = *cb;

	/* Activate the async. I/O transfer. */
	atomic_store(&p->aio_use, 1);

	/* Leave the critical section. */
	os_cs_leave(&p->aio_mutex);	
}

/**
 * py_aio_write() - this async. I/O operation triggers the py irq, to start or
 * restart the send procedure.
 *
 * @dev_id:  id of the shared memory device.
 *
 * Return:	None.
 **/
void py_aio_write(int dev_id)
{
	os_dev_t *p;
	
	/* Entry conditon. */
	p = os_py_device;
	OS_TRAP_IF(p == NULL || p->id != dev_id);

	/* Enter the critical section. */
	os_cs_enter(&p->aio_mutex);

	/* Extended entry conditon. */
	OS_TRAP_IF(! p->aio_use);

	/* Update the write trigger. */
	atomic_store(&p->aio_wr_trigger, 1);

	/* Resume the py interrupt handler. */
	os_sem_release(p->my_int);

	/* Leave the critical section. */
	os_cs_leave(&p->aio_mutex);	
}

/**
 * py_aio_read() - this async. I/O operation triggers the py irq, to start or
 * restart the receive procedure.
 *
 * @dev_id:  id of the shared memory device.
 *
 * Return:	None.
 **/
void py_aio_read(int dev_id)
{
	os_dev_t *p;
	
	/* Entry conditon. */
	p = os_py_device;
	OS_TRAP_IF(p == NULL || p->id != dev_id);

	/* Enter the critical section. */
	os_cs_enter(&p->aio_mutex);

	/* Extended entry conditon. */
	OS_TRAP_IF(! p->aio_use);

	/* Update the read trigger. */
	atomic_store(&p->aio_rd_trigger, 1);

	/* Resume the py interrupt handler. */
	os_sem_release(p->my_int);

	/* Leave the critical section. */
	os_cs_leave(&p->aio_mutex);	
}

/**
 * py_read() - py waits for incoming payload.
 *
 * @dev_id:  id of the shared memory device.
 * @buf:     pointer to the received payload.
 * @count:   size of the destination buffer.
 *
 * Return:	number of the received bytes.
 **/
int py_read(int dev_id, char *buf, int count)
{
	os_shm_top_t *top;
	os_dev_t *p;
	int n;

	/* Entry conditon. */
	p = os_py_device;
	OS_TRAP_IF(p == NULL || p->id != dev_id || ! p->init ||
		   buf == NULL || count < 1);

	/* Get the pointer to the shm topology. */
	top = &p->top;
	
	/* Enter the critical section. */
	os_cs_enter(&p->read_mutex);

	/* Define the read method. */
	atomic_store(&p->sync_read, 1);

	/* Test the buffer state. */
	if (buf == NULL || count < 1) {
		/* Leave the critical section. */
		os_cs_leave(&p->read_mutex);	
		return 0;
	}
	
	/* Wait for the DL payload. */
	for(;;) {
		/* Get the number of the received bytes. */
		n = atomic_exchange(&top->dl_count, 0);

		/* Test the number of the received bytes. */
		if (n < 1) { 
			/* Suspend the read user. */
			os_sem_wait(&p->suspend_reader);
			continue;
		}

		break;
	}

	/* Test the user buffer. */
	OS_TRAP_IF(count < n);
	
	/* Copy the received payload. */
	os_memcpy(buf, count, top->dl_start, n);

	/* Release the pending DL buffer. */
	py_ulq_add(p, 0, 1);
	
	/* Leave the critical section. */
	os_cs_leave(&p->read_mutex);	

	return n;
}

/**
 * py_zread() - py waits for incoming payload. With each successiv call.
 * The reference to the previous call is released automatically.
 *
 * @dev_id:  id of the shared memory device.
 * @buf:     pointer to the received payload.
 * @count:   size of the destination buffer.
 *
 * Return:	number of the received bytes.
 **/
static int py_zread(int dev_id, char **buf, int count)
{
	os_shm_top_t *top;
	os_dev_t *p;
	int pending_dl, n;
	
	/* Entry conditon. */
	p = os_py_device;
	OS_TRAP_IF(p == NULL || p->id != dev_id);

	/* Entry condition. */
	OS_TRAP_IF(! p->init);

	/* Get the pointer to the shm topology. */
	top = &p->top;
	
	/* Enter the critical section. */
	os_cs_enter(&p->read_mutex);

	/* Define the read method. */
	atomic_store(&p->sync_read, 1);

	/* If the DL payload is, send the shm message to van. */
	pending_dl = atomic_exchange(&p->pending_dl, 0);
	if (pending_dl)
		py_ulq_add(p, 0, 1);
	
	/* Test the buffer state. */
	if (buf == NULL || count < 1) {
		/* Leave the critical section. */
		os_cs_leave(&p->read_mutex);	
		return 0;
	}
	
	/* Wait for the DL payload. */
	for(;;) {
		/* Get the number of the received bytes. */
		n = atomic_exchange(&top->dl_count, 0);

		/* Test the number of the received bytest. */
		if (n < 1) { 
			/* Suspend the read user. */
			os_sem_wait(&p->suspend_reader);
			continue;
		}

		break;
	}

	/* Get the pointer to the received payload. */
	*buf = top->dl_start;
	atomic_store(&p->pending_dl, 1);

	/* Leave the critical section. */
	os_cs_leave(&p->read_mutex);	

	return n;
}

/**
 * py_write() - send the UL payload to van and suspend the caller until the
 * payload has been processed.
 *
 * @dev_id:  id of the shared memory device.
 * @buf:     pointer to the payload.
 * @count:   size of the payload.
 *
 * Return:	None.
 **/
static void py_write (int dev_id, char *buf, int count)
{
	os_shm_top_t *top;
	os_dev_t *p;

	/* Entry conditon. */
	p = os_py_device;
	OS_TRAP_IF(p == NULL || p->id != dev_id || buf == NULL || count < 1);

	/* Entry condition. */
	OS_TRAP_IF(! p->init);

	/* Get the pointer to the shm topology and to the py input queue. */
	top = &p->top;
	
	/* Enter the critical section. */
	os_cs_enter(&p->write_mutex);

	/* XXX Test the state of the UL buffer: py->van. */
	OS_TRAP_IF(top->ul_size < count || p->pending_ul);

	/* Change the UL state. */
	atomic_store(&p->pending_ul, 1);
	atomic_store(&p->sync_write, 1);

	/* Fill the UL buffer. */
	os_memcpy(top->ul_start, OS_BUF_SIZE, buf, count);

	/* Save the size of the payload. */
	atomic_store(&top->ul_count, count);
		
	/* Send the shm message to van. */
	py_ulq_add(p, count, 0);

	/* Suspend the caller until the action is executed. */
	os_sem_wait(&p->suspend_writer);

	/* Leave the critical section. */
	os_cs_leave(&p->write_mutex);	
}

/**
 * py_exit_exec() - the py_int thread shall not be suspended with named
 * py_int semphore.
 *
 * @msg:  generic input message.
 *
 * Return:	None.
 **/
static void py_exit_exec(os_queue_elem_t *msg)
{
	OS_TRACE(("%s [s:ready, m:py-exit] -> [s:ready]\n", PP));
}

/**
 * py_close() - the python process shall call this function to remove the
 * shared memory ressources.
 *
 * @dev_id:  id of the py shm device.
 *
 * Return:	None.
 **/
static void py_close(int dev_id)
{
	os_queue_elem_t msg;
	os_dev_t *p;
	
	/* Entry conditon. */
	p = os_py_device;
	OS_TRAP_IF(p == NULL || p->id != dev_id);

	/* Change the state of the shm area. */
	p->init = 0;
	
	/* Define the py_int terminate message. */
	os_memset(&msg, 0, sizeof(msg));
	msg.param = p->thread;
	msg.cb    = py_exit_exec;
	
	/* Resume the py_int thread, to terminate it. */
	OS_SEND(p->thread, &msg, sizeof(msg));
	atomic_store(&p->down, 1);
	os_sem_release(p->my_int);
	
	/* Delete the py interrupt handler. */
	os_thread_destroy(p->thread);
	
	/* Delete the access and the mapping to the shared memory. */
	os_shm_close(p);
	
	/* Free the py shm device. */
	OS_FREE(os_py_device);
}

/**
 * py_int_write() - resume the syspend caller in write or request UL data
 * from the aio user.
 *
 * @p:  pointer to the py device state.
 * @t:  pointer to the shared memory topology.
 *
 * Return:	None.
 **/
static void py_int_write(os_dev_t *p, os_shm_top_t *t)
{
	int aio_use, sync, pending_ul, count;
	
	/* Test the aio status request. */
	aio_use = atomic_load(&p->aio_use);
	if (! aio_use) {
		/* Test the write method. */
		sync = atomic_exchange(&p->sync_write, 0);
		if (sync)
			os_sem_release(&p->suspend_writer);
		
		return;
	}

	/* Reset the write trigger. */
	atomic_store(&p->aio_wr_trigger, 0);

	/* Test the UL status. */
	pending_ul = atomic_load(&p->pending_ul);
	if (pending_ul)
		return;
	
	/* Request UL data from the aio user. */
	count = p->aio_cb.write_cb(p->id, t->ul_start, OS_BUF_SIZE);
	if (count < 1)
		return;

	/* Change the UL state. */
	atomic_store(&p->pending_ul, 1);
					
	/* Save the size of the payload. */
	atomic_store(&t->ul_count, count);

	/* Send the shm message to van. */
	py_aio_ulq_add(p, count, 0);
}

/**
 * py_int_read() - pass the DL data to the async. caller or resume the
 * suspended caller in read or zread.
 *
 * @p:      pointer to the py device state.
 * @t:      pointer to the shared memory topology.
 * @count:  number of the pending DL characters.
 *
 * Return:	None.
 **/
static void py_int_read(os_dev_t *p, os_shm_top_t *t, int count)
{
	int aio_use, sync, consumed;
	
	/* Test the aio status. */
	aio_use = atomic_load(&p->aio_use);
	if (aio_use) {
		/* Reset the read trigger. */
		atomic_store(&p->aio_rd_trigger, 0);

		/* Test the number of the pending DL characters. */
		if (count < 1)
			return;
		
		/* Pass the DL data to the aio user. */
		consumed = p->aio_cb.read_cb(p->id, t->dl_start, count);

		/* Update the DL state. */
		OS_TRAP_IF(consumed > count || consumed < 0);
		if (consumed != count) {
			atomic_store(&t->dl_count, count - consumed);
			return;
		}
		
		/* Reset the DL state. */
		atomic_store(&t->dl_count, 0);

		/* Release the pending DL buffer. */
		py_aio_ulq_add(p, 0, 1);
	}
	else {
		/* Save the number of the received DL bytes. */
		atomic_store(&t->dl_count, count);
			
		/* Test the read method. */
		sync = atomic_exchange(&p->sync_read, 0);

		OS_TRACE(("%s dl_count=%d, read=%d\n", PP, t->dl_count, sync));

		if (sync)
			os_sem_release(&p->suspend_reader);
	}
}

/**
 * py_int_exec() - the py_int thread waits for the py_int triggered by van or
 * internally to process a send order.
 *
 * @msg:  generic input message.
 *
 * Return:	None.
 **/
static void py_int_exec(os_queue_elem_t *msg)
{
	os_shm_queue_t *q;
	os_shm_msg_t *shm_m;
	os_shm_top_t *t;
	os_dev_t *p;
	int down;

	/* Get the address of the py shared memory state. */
	p = os_py_device;

	/* Get the pointer to the shm topology and to the py input queue. */
	t = &p->top;
	q = t->dl_queue;

	/* Loop thru the py interrupts triggered by van. */
	for(;;) {
		/* Test the access method to the python device. */
		if (p->aio_use) {
			/* Test the DL payload state from van and trigger the
			 *  user actions. */
			if (p->aio_rd_trigger)
				py_int_read(p, t, t->dl_count);
			
			/* Test the aio status, request data from the user and
			 *  inform van. */
			if (p->aio_wr_trigger)
				py_int_write(p, t);
		}		

		OS_TRACE(("%s [s:ready, m:py-int] -> [s:suspended]\n", PP));

		/* Wait for the py_int trigger. */
		os_sem_wait(p->my_int);

		/* Test the thread state. */
		down = atomic_load(&p->down);
		if (down) {
			OS_TRACE(("%s [s:suspended, m:py-int] -> [s:down]\n", PP));
			return;
		} else {
			OS_TRACE(("%s [s:suspended, m:py-int] -> [s:ready]\n", PP));
		}

		/* Analyze the py shm input queue. */
		while (q->tail != q->head) {
			shm_m = &q->ring[q->tail];
			
			/* Test the DL payload state from van. */
			if (shm_m->size > 0) {
				/* Test the DL payload state from van and trigger the user actions. */
				py_int_read(p, t, shm_m->size);
			}

			/* Test the state of the sent UL playload from py to van. */
			if (shm_m->consumed) {
				/* Release the UL transfer. */
				atomic_store(&p->pending_ul, 0);

				/* Trigger the py user UL actions. */
				py_int_write(p, t);
			}
		
			/* Increment and test the start of the message list. */
			q->tail++;
			if (q->tail >= OS_SHM_Q_SIZE)
				q->tail = 0;
		}
	}
}

/**
 * py_open() - the python process shall call this function to create the
 * shared memory device.
 *
 * @name:  pointer to the van device name.
 *
 * Return:	None.
 **/
static int py_open(char *name)
{
	os_queue_elem_t msg;
	os_dev_t *p;

	/* Entry condition. */
	OS_TRAP_IF(name == NULL || os_strcmp(name, OS_PY_NAME) != 0);

	/* Allocate the van device state. */
	p = os_py_device = OS_MALLOC(sizeof(os_dev_t));
	os_memset(p, 0, sizeof(os_dev_t));

	/* Save the device identificaton. */
	p->id = OS_DEV_PY;
	p->name = OS_PY_NAME;
	p->file_name = OS_SHM_FILE;

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
	p->thread = os_thread_create("py_int", PRIO, Q_SIZE);	
	
	/* Change the state of the shm area. */
	p->init = 1;
	
	/* Start with processing of the py_int. */
	os_memset(&msg, 0, sizeof(msg));
	msg.param = p->thread;
	msg.cb    = py_int_exec;
	OS_SEND(p->thread, &msg, sizeof(msg));
	
	return OS_DEV_PY;
}

/*============================================================================
  EXPORTED FUNCTIONS
  ============================================================================*/
/**
 * os_py_ripcord() - release critical van device resources.
 *
 * Return:	None.
 **/
void os_py_ripcord(void)
{
	os_dev_t *p;

	/* Entry conditon. */
	p = os_py_device;

	/* Test the van device state. */
	if (p == NULL)
		return;

	/* Remove the reference to the python semaphore. */
	if (p->my_int)
		sem_close(p->my_int);
	
	/* Remove the reference to the van semaphore. */
	if (p->other_int)
		sem_close(p->other_int);

	/* Test the file descriptor of the shared memory file. */
	if (p->fd == 0)
		return;

	/* Delete the mapping of the shared memory area. */
	if (p->start != 0)
		munmap(p->start, p->size);

	/* Close the shared memory file. */
	close(p->fd);
}

/**
 * os_py_exit() - test the state of the shared memory devices.
 *
 * Return:	None.
 **/
void os_py_exit(void)
{
	OS_TRAP_IF(os_py_device != NULL);
}

/**
 * os_py_init() - request the entry points for the python shared memory device.
 *
 * @conf:  pointer to the trace configuration.
 * @op:    pointer to the entry points of the shm device.
 *
 * Return:	None.
 **/
void os_py_init(os_conf_t *conf, os_dev_ops_t *op)
{
	/* Save the reference to the OS configuration. */
	os_conf_p = conf;

	/* Save the py shared memory operations. */
	op->device_name = OS_PY_NAME;
	op->device_id   = OS_DEV_PY;
	op->open        = py_open;
	op->close       = py_close;
	op->write       = py_write;
	op->zread       = py_zread;
	op->read        = py_read;
	op->aio_action  = py_aio_action;
	op->aio_write   = py_aio_write;
	op->aio_read    = py_aio_read;
}
