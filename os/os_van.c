// SPDX-License-Identifier: GPL-2.0

/*
 * Van shared memory driver.
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
#define OS_VAN_NAME  "/van"  /* Name of the van shared memory device. */
#define VP  "V-I>"           /* Prompt for the van_int thread. */

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

/* Van shared memory devices. */
static os_dev_t *os_van_device;

/*============================================================================
  LOCAL FUNCTION PROTOTYPES
  ============================================================================*/
/*============================================================================
  LOCAL FUNCTIONS
  ============================================================================*/
/**
 * van_dlq_add() - send the DL shm message from van to py.
 *
 * dev:       pointer to the shm device.
 * count:     size of the DL payload.
 * consumed:  if 1, the UL buffer has been released.
 *
 * Return:	None.
 **/
static void van_dlq_add(os_dev_t *dev, int count, int consumed)
{
	os_shm_queue_t *q;
	os_shm_msg_t *shm_m;
	int head;
	
	/* Enter the critical section. */
	os_cs_enter(&dev->top.q_mutex);

	/* Get the pointer of the py shm input queue. */
	q = dev->top.dl_queue;
	
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
 * van_sync_read() - van waits for incoming payload.
 *
 * @dev_id:  id of the shared memory device.
 * @buf:     pointer to the received payload.
 * @count:   size of the destination buffer.
 *
 * Return:	number of the received bytes.
 **/
static int van_sync_read (int dev_id, char *buf, int count)
{
	os_shm_top_t *top;
	os_dev_t *v;
	int n;

	/* Entry conditon. */
	v = os_van_device;
	OS_TRAP_IF(v == NULL || v->id != dev_id || ! v->init ||
		   buf == NULL || count < 1);

	/* Get the pointer to the shm topology. */
	top = &v->top;
	
	/* Enter the critical section. */
	os_cs_enter(&v->read_mutex);

	/* Define the read method. */
	atomic_store(&v->sync_read, 1);

	/* Test the buffer state. */
	if (buf == NULL || count < 1) {
		/* Leave the critical section. */
		os_cs_leave(&v->read_mutex);	
		return 0;
	}
	
	/* Wait for the UL payload. */
	for(;;) {
		/* Get the number of the received bytes. */
		n = atomic_exchange(&top->ul_count, 0);

		/* Test the number of the received bytest. */
		if (n < 1) { 
			/* Suspend the read user. */
			os_sem_wait(&v->suspend_reader);
			continue;
		}

		break;
	}

	/* Test the user buffer. */
	OS_TRAP_IF(count < n);
	
	/* Copy the received payload. */
	os_memcpy(buf, count, top->ul_start, n);

	/* Release the pending UL buffer. */
	van_dlq_add(v, 0, 1);
	
	/* Leave the critical section. */
	os_cs_leave(&v->read_mutex);	

	return n;
}

/**
 * van_sync_zread() - van waits for incoming payload. With each successiv
 *  call, the reference to the previous call is released automatically.
 *
 * @dev_id:  id of the shared memory device.
 * @buf:     pointer to the received payload.
 * @count:   size of the destination buffer.
 *
 * Return:	number of the received bytes.
 **/
static int van_sync_zread (int dev_id, char **buf, int count)
{
	os_shm_top_t *top;
	os_dev_t *v;
	int pending_ul, n;
	
	/* Entry conditon. */
	v = os_van_device;
	OS_TRAP_IF(v == NULL || v->id != dev_id);

	/* Entry condition. */
	OS_TRAP_IF(! v->init);

	/* Get the pointer to the shm topology. */
	top = &v->top;
	
	/* Enter the critical section. */
	os_cs_enter(&v->read_mutex);

	/* Define the read method. */
	atomic_store(&v->sync_read, 1);

	/* If the UL payload is pending, send the shm message to py. */
	pending_ul = atomic_exchange(&v->pending_ul, 0);
	if (pending_ul)
		van_dlq_add(v, 0, 1);
	
	/* Test the buffer state. */
	if (buf == NULL || count < 1) {
		/* Leave the critical section. */
		os_cs_leave(&v->read_mutex);	
		return 0;
	}
	
	/* Wait for the UL payload. */
	for(;;) {
		/* Get the number of the received bytes. */
		n = atomic_exchange(&top->ul_count, 0);

		/* Test the number of the received bytes. */
		if (n < 1) { 
			/* Suspend the read user. */
			os_sem_wait(&v->suspend_reader);
			continue;
		}

		break;
	}

	/* Get the pointer to the received payload. */
	*buf = top->ul_start;
	atomic_store(&v->pending_ul, 1);

	/* Leave the critical section. */
	os_cs_leave(&v->read_mutex);	

	return n;
}

/**
 * van_sync_write() - send the DL payload to py and suspend the caller until the
 * payload has been processed.
 *
 * @dev_id:  id of the shared memory device.
 * @buf:     pointer to the payload.
 * @count:   size of the payload.
 *
 * Return:	None.
 **/
static void van_sync_write(int dev_id, char *buf, int count)
{
	os_shm_top_t *top;
	os_dev_t *v;

	/* Entry conditon. */
	v = os_van_device;
	OS_TRAP_IF(v == NULL || v->id != dev_id || buf == NULL || count < 1);

	/* Entry condition. */
	OS_TRAP_IF(! v->init);

	/* Get the pointer to the shm topology and to the van input queue. */
	top = &v->top;
	
	/* Enter the critical section. */
	os_cs_enter(&v->write_mutex);

	/* XXX Test the state of the DL buffer: van->py. */
	OS_TRAP_IF(top->dl_size < count || v->pending_dl);

	/* Change the DL state. */
	atomic_store(&v->pending_dl, 1);
	atomic_store(&v->sync_write, 1);

	/* Fill the DL buffer. */
	os_memcpy(top->dl_start, OS_BUF_SIZE, buf, count);

	/* Save the size of the payload. */
	atomic_store(&top->dl_count, count);
		
	/* Send the shm message to py. */
	van_dlq_add(v, count, 0);

	/* Suspend the caller until the action is executed. */
	os_sem_wait(&v->suspend_writer);

	/* Leave the critical section. */
	os_cs_leave(&v->write_mutex);	
}

/**
 * van_exit_exec() - the van_int thread shall not be suspended with named
 * van_int semphore.
 *
 * @msg:  generic input message.
 *
 * Return:	None.
 **/
static void van_exit_exec(os_queue_elem_t *msg)
{
	OS_TRACE(("%s [s:ready, m:van-exit] -> [s:ready]\n", VP));
}

/**
 * van_close() - the van process shall call this function to remove the
 * shared memory ressources.
 *
 * @dev_id:  id of the van shm device.
 *
 * Return:	None.
 **/
static void van_close(int dev_id)
{
	os_queue_elem_t msg;
	os_dev_t *v;
	int rv;

	/* Entry conditon. */
	v = os_van_device;
	OS_TRAP_IF(v == NULL || v->id != dev_id);
	
	/* Change the state of the shm area. */
	v->init = 0;

	/* Define the van_int terminate message. */
	os_memset(&msg, 0, sizeof(msg));
	msg.param = v->thread;
	msg.cb    = van_exit_exec;
	
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

	/* Free the van shm device. */
	OS_FREE(os_van_device);
}

/**
 * van_int_exec() - the van_int thread waits for the van_int triggered by py
 * or internally to process a send order.
 *
 * @msg:  generic input message.
 *
 * Return:	None.
 **/
static void van_int_exec(os_queue_elem_t *msg)
{
	os_shm_queue_t *q;
	os_shm_msg_t *shm_m;
	os_shm_top_t *t;
	os_dev_t *v;
	int down, sync;

	/* Get the address of the van shared memory state. */
	v = os_van_device;

	/* Loop thru the van interrupts triggered by py. */
	for(;;) {
		OS_TRACE(("%s [s:ready, m:van-int] -> [s:suspended]\n", VP));

		/* Wait for the van_int trigger. */
		os_sem_wait(v->my_int);

		/* Test the thread state. */
		down = atomic_load(&v->down);
		if (down) {
			OS_TRACE(("%s [s:suspended, m:van-int] -> [s:down]\n", VP));
			return;
		} else {
			OS_TRACE(("%s [s:suspended, m:van-int] -> [s:ready]\n", VP));
		}
	
		/* Get the pointer of the van shm input queue. */
		t = &v->top;
		q = v->top.ul_queue;

		/* Analyze the van shm input queue. */
		while (q->tail != q->head) {
			shm_m = &q->ring[q->tail];

			/* Test the UL payload state from py. */
			if (shm_m->size > 0) {
				/* Save the number of the received UL bytes. */
				atomic_store(&t->ul_count, shm_m->size);
			
				/* XXX */
#if 0
				p->async_read_cb(t->ul_start, shm_m->size);
#endif
				/* Test the read method. */
				sync = atomic_exchange(&v->sync_read, 0);

				OS_TRACE(("%s dl_count=%d, sync_read=%d\n", VP, t->ul_count, sync));

				if (sync)
					os_sem_release(&v->suspend_reader);

			}

			/* Test the state of the sent DL playload from van to py. */
			if (shm_m->consumed) {
				atomic_store(&v->pending_dl, 0);

				/* Test the write method. */
				sync = atomic_exchange(&v->sync_write, 0);
				if (sync)
					os_sem_release(&v->suspend_writer);
			}
		
			/* Increment and test the start of the message list. */
			q->tail++;
			if (q->tail >= OS_SHM_Q_SIZE)
				q->tail = 0;
		}
	}
}

/**
 * van_open() - the van process shall call this function to create the shared
 * memory device.
 *
 * @name:  pointer to the van device name.
 *
 * Return:	the device id.
 **/
static int van_open(char *name)
{
	os_queue_elem_t msg;
	os_dev_t *v;
	mode_t mode;
	int oflag;
	
	/* Entry condition. */
	OS_TRAP_IF(name == NULL || os_strcmp(name, OS_VAN_NAME) != 0);

	/* Allocate the van device state. */
	v = os_van_device = OS_MALLOC(sizeof(os_dev_t));
	os_memset(v, 0, sizeof(os_dev_t));

	/* Save the device identificaton. */
	v->id = OS_DEV_VAN;
	v->name = OS_VAN_NAME;
	v->file_name = OS_SHM_FILE;
	
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
	v->thread = os_thread_create("van_int", PRIO, Q_SIZE);
	
	/* Change the state of the shm area. */
	v->init = 1;

	/* Start with processing of the van_int. */
	os_memset(&msg, 0, sizeof(msg));
	msg.param = v->thread;
	msg.cb    = van_int_exec;
	OS_SEND(v->thread, &msg, sizeof(msg));
	
	return OS_DEV_VAN;
}

/*============================================================================
  EXPORTED FUNCTIONS
  ============================================================================*/
/**
 * os_van_exit() - test the state of the shared memory devices.
 *
 * Return:	None.
 **/
void os_van_exit(void)
{
	OS_TRAP_IF(os_van_device != NULL);
}

/**
 * os_van_init() - trigger the installation of the van shared memory device.
 *
 * @conf:  pointer to the trace configuration.
 * @op:   pointer to the entry points of the shm device.
 * Return:	None.
 **/
void os_van_init(os_conf_t *conf, os_dev_ops_t *op)
{
	/* Save the reference to the OS configuration. */
	os_conf_p = conf;

	/* Save the van shared memory operations. */
	op->device_name = OS_VAN_NAME;
	op->device_id   = OS_DEV_VAN;
	op->open        = van_open;
	op->close       = van_close;
	op->sync_write  = van_sync_write;
	op->sync_zread  = van_sync_zread;
	op->sync_read   = van_sync_read;
}
