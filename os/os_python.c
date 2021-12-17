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
 * py_ulq_add() - send the UL shm message from py to van.
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

	/* Trigger the py_int. */
	os_sem_release(dev->other_int);

	/* Leave the critical section. */
	os_cs_leave(&dev->top.q_mutex);	
}

/**
 * py_sync_read() - py waits for incoming payload.
 *
 * @dev_id:  id of the shared memory device.
 * @buf:     pointer to the received payload.
 * @count:   size of the destination buffer.
 *
 * Return:	number of the received bytes.
 **/
int py_sync_read(int dev_id, char *buf, int count)
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
 * py_sync_zread() - py waits for incoming payload. With each successiv call.
 * The reference to the previous call is released automatically.
 *
 * @dev_id:  id of the shared memory device.
 * @buf:     pointer to the received payload.
 * @count:   size of the destination buffer.
 *
 * Return:	number of the received bytes.
 **/
static int py_sync_zread(int dev_id, char **buf, int count)
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
 * py_sync_write() - send the UL payload to van and suspend the caller until the
 * payload has been processed.
 *
 * @dev_id:  id of the shared memory device.
 * @buf:     pointer to the payload.
 * @count:   size of the payload.
 *
 * Return:	None.
 **/
static void py_sync_write (int dev_id, char *buf, int count)
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
	int down, sync;

	/* Get the address of the py shared memory state. */
	p = os_py_device;

	/* Loop thru the py interrupts triggered by van. */
	for(;;) {
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

		/* Get the pointer to the shm topology and to the py input queue. */
		t = &p->top;
		q = t->dl_queue;

		/* Analyze the py shm input queue. */
		while (q->tail != q->head) {
			shm_m = &q->ring[q->tail];

			/* Test the DL payload state from van. */
			if (shm_m->size > 0) {
				/* Save the number of the received DL bytes. */
				atomic_store(&t->dl_count, shm_m->size);
			
				/* XXX */
#if 0
				p->async_read_cb(t->dl_start, shm_m->size);
#endif
				/* Test the read method. */
				sync = atomic_exchange(&p->sync_read, 0);

				OS_TRACE(("%s dl_count=%d, sync_read=%d\n", PP, t->dl_count, sync));

				if (sync)
					os_sem_release(&p->suspend_reader);

			}

			/* Test the state of the sent UL playload from py to van. */
			if (shm_m->consumed) {
				atomic_store(&p->pending_ul, 0);
				
				/* XXX */
#if 0
				p->async_write_cb();
#endif
				/* Test the write method. */
				sync = atomic_exchange(&p->sync_write, 0);
				if (sync)
					os_sem_release(&p->suspend_writer);
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
	p->thread = os_thread_create("py_int", OS_THREAD_PRIO_SOFTRT,
				     OS_THREAD_Q_SIZE);	
	
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
 * os_py_exit() - test the state of the shared memory devices.
 *
 * Return:	None.
 **/
void os_py_exit(void)
{
	OS_TRAP_IF(os_py_device != NULL);
}

/**
 * os_py_init() - trigger the installation of the py shared memory device.
 *
 * @conf:  pointer to the trace configuration.
 * @op:   pointer to the entry points of the shm device.
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
	op->sync_write  = py_sync_write;
	op->sync_zread  = py_sync_zread;
	op->sync_read   = py_sync_read;
}
