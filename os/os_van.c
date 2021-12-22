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
 * van_aio_dlq_add() - send the DL shm message from van to py asynchronously.
 *
 * dev:       pointer to the shm device.
 * count:     size of the DL payload.
 * consumed:  if 1, the UL buffer has been released.
 *
 * Return:	None.
 **/
static void van_aio_dlq_add(os_dev_t *dev, int count, int consumed)
{
	os_shm_queue_t *q;
	os_shm_msg_t *shm_m;
	int head;
	
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
}

/**
 * van_dlq_add() - send the DL shm message from van to py synchronously.
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
 * van_aio_action() - install the read and write callback for the asynchronous
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
void van_aio_action(int dev_id, os_aio_cb_t *cb)
{
	os_dev_t *v;
	
	/* Entry conditon. */
	v = os_van_device;
	OS_TRAP_IF(v == NULL || v->id != dev_id ||
		   cb == NULL || cb->read_cb == NULL || cb->write_cb == NULL);

	/* Enter the critical section. */
	os_cs_enter(&v->aio_mutex);

	/* Extended entry conditon. */
	OS_TRAP_IF(v->aio_use ||
		   v->pending_dl || v->sync_read || v->sync_write);

	/* Copy the async. I/O operations. */
	v->aio_cb = *cb;

	/* Activate the async. I/O transfer. */
	atomic_store(&v->aio_use, 1);

	/* Leave the critical section. */
	os_cs_leave(&v->aio_mutex);	
}

/**
 * van_aio_write() - this async. I/O operation triggers the py irq, to start or
 * restart the send procedure.
 *
 * @dev_id:  id of the shared memory device.
 *
 * Return:	None.
 **/
void van_aio_write(int dev_id)
{
	os_dev_t *v;
	
	/* Entry conditon. */
	v = os_van_device;
	OS_TRAP_IF(v == NULL || v->id != dev_id);

	/* Enter the critical section. */
	os_cs_enter(&v->aio_mutex);

	/* Extended entry conditon. */
	OS_TRAP_IF(! v->aio_use);

	/* Update the write trigger. */
	atomic_store(&v->aio_wr_trigger, 1);

	/* Resume the van interrupt handler. */
	os_sem_release(v->my_int);

	/* Leave the critical section. */
	os_cs_leave(&v->aio_mutex);	
}

/**
 * van_aio_read() - this async. I/O operation triggers the py irq, to start or
 * restart the receive procedure.
 *
 * @dev_id:  id of the shared memory device.
 *
 * Return:	None.
 **/
void van_aio_read(int dev_id)
{
	os_dev_t *v;
	
	/* Entry conditon. */
	v = os_van_device;
	OS_TRAP_IF(v == NULL || v->id != dev_id);

	/* Enter the critical section. */
	os_cs_enter(&v->aio_mutex);

	/* Extended entry conditon. */
	OS_TRAP_IF(! v->aio_use);

	/* Update the read trigger. */
	atomic_store(&v->aio_rd_trigger, 1);

	/* Resume the van interrupt handler. */
	os_sem_release(v->my_int);

	/* Leave the critical section. */
	os_cs_leave(&v->aio_mutex);	
}

/**
 * van_read() - van waits for incoming payload.
 *
 * @dev_id:  id of the shared memory device.
 * @buf:     pointer to the received payload.
 * @count:   size of the destination buffer.
 *
 * Return:	number of the received bytes.
 **/
static int van_read(int dev_id, char *buf, int count)
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
	
	/* Release the sync. read operation. */
	atomic_store(&v->sync_read, 0);
	
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
 * van_zread() - van waits for incoming payload. With each successiv
 *  call, the reference to the previous call is released automatically.
 *
 * @dev_id:  id of the shared memory device.
 * @buf:     pointer to the received payload.
 * @count:   size of the destination buffer.
 *
 * Return:	number of the received bytes.
 **/
static int van_zread(int dev_id, char **buf, int count)
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
	
	/* Update the sync. read flag. */
	atomic_store(&v->sync_read, 1);
	
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

	/* Release the sync. read operation. */
	atomic_store(&v->sync_read, 0);
	
	/* Leave the critical section. */
	os_cs_leave(&v->read_mutex);	

	return n;
}

/**
 * van_write() - send the DL payload to py and suspend the caller until the
 * payload has been processed.
 *
 * @dev_id:  id of the shared memory device.
 * @buf:     pointer to the payload.
 * @count:   size of the payload.
 *
 * Return:	None.
 **/
static void van_write(int dev_id, char *buf, int count)
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

#if defined(OS_CLOSE_NET)
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
#endif

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
#if defined(OS_CLOSE_NET)
	os_queue_elem_t msg;
#endif
	os_dev_t *v;
	int rv;

	/* Entry conditon. */
	v = os_van_device;
	OS_TRAP_IF(v == NULL || v->id != dev_id);
	
	/* Change the state of the shm area. */
	v->init = 0;

#if defined(OS_CLOSE_NET)
	/* Define the van_int terminate message. */
	os_memset(&msg, 0, sizeof(msg));
	msg.param = v->thread;
	msg.cb    = van_exit_exec;	
	OS_SEND(v->thread, &msg, sizeof(msg));
#endif
	/* Resume the van_int thread, to terminate it. */
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

	/* Remove the shared memory file. */
	rv = remove(OS_SHM_FILE);
	OS_TRAP_IF(rv != 0);
	
	/* Free the van shm device. */
	OS_FREE(os_van_device);
}

/**
 * van_int_write() - resume the suspend caller in van_write or request DL data
 * from the aio user.
 *
 * @v:  pointer to the van device state.
 * @t:  pointer to the shared memory topology.
 *
 * Return:	None.
 **/
static void van_int_write(os_dev_t *v, os_shm_top_t *t)
{
	int aio_use, sync, pending_dl, count;
	
	/* Test the aio status request. */
	aio_use = atomic_load(&v->aio_use);
	if (! aio_use) {
		/* Test the write method. */
		sync = atomic_exchange(&v->sync_write, 0);
		if (sync)
			os_sem_release(&v->suspend_writer);
		
		return;
	}

	/* Reset the write trigger. */
	atomic_store(&v->aio_wr_trigger, 0);

	/* Test the release status of the DL buffer, which py has not
	 * consumed. */
	pending_dl = atomic_load(&v->pending_dl);
	if (pending_dl)
		return;
	
	/* Request DL data from the aio user. */
	count = v->aio_cb.write_cb(v->id, t->dl_start, OS_BUF_SIZE);
	if (count < 1)
		return;

	/* Change the DL state. */
	atomic_store(&v->pending_dl, 1);
					
	/* Save the size of the payload. */
	atomic_store(&t->dl_count, count);

	/* Send the shm message to py. */
	van_aio_dlq_add(v, count, 0);
}

/**
 * van_int_read() - pass the UL data to the async. caller or resume the
 * suspended caller in read or zread.
 *
 * @v:      pointer to the van device state.
 * @t:      pointer to the shared memory topology.
 * @count:  number of the pending DL characters.
 *
 * Return:	None.
 **/
static void van_int_read(os_dev_t *v, os_shm_top_t *t, int count)
{
	int aio_use, sync, consumed;
	
	/* Test the aio status. */
	aio_use = atomic_load(&v->aio_use);
	if (aio_use) {
		/* Reset the read trigger. */
		atomic_store(&v->aio_rd_trigger, 0);

		/* Test the number of the pending UL characters. */
		if (count < 1)
			return;
		
		/* Pass the UL data to the aio user. */
		consumed = v->aio_cb.read_cb(v->id, t->ul_start, count);

		/* Update the DL state. */
		OS_TRAP_IF(consumed > count || consumed < 0);
		if (consumed != count) {
			atomic_store(&t->ul_count, count - consumed);
			return;
		}

		/* Reset the UL state. */
		atomic_store(&t->ul_count, 0);
		
		/* Release the pending UL buffer. */
		van_aio_dlq_add(v, 0, 1);
	}
	else {
		/* Save the number of the received UL bytes. */
		atomic_store(&t->ul_count, count);
			
		/* Test the read method. */
		sync = atomic_exchange(&v->sync_read, 0);

		OS_TRACE(("%s dl_count=%d, read=%d\n", VP, t->ul_count, sync));

		if (sync)
			os_sem_release(&v->suspend_reader);
	}
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
	int down;

	/* Get the address of the van shared memory state. */
	v = os_van_device;

	/* Get the pointer of the van shm input queue. */
	t = &v->top;
	q = v->top.ul_queue;

	/* Loop thru the van interrupts triggered by py. */
	for(;;) {
		/* Test the access method to the van device. */
		if (v->aio_use) {
			/* Test the UL payload state from py and trigger the
			 *  user actions. */
			if (v->aio_rd_trigger)
				van_int_read(v, t, t->ul_count);
			
			/* Test the aio status, request data from the user and
			 *  inform py. */
			if (v->aio_wr_trigger)
				van_int_write(v, t);
		}		

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
	
		/* Analyze the van shm input queue. */
		while (q->tail != q->head) {
			shm_m = &q->ring[q->tail];

			/* Test the UL payload state from van. */
			if (shm_m->size > 0) {
				/* Test the UL payload state from py and trigger the user actions. */
				van_int_read(v, t, shm_m->size);
			}

			/* Test the state of the sent DL playload from van to py. */
			if (shm_m->consumed) {
				/* Release the DL transfer. */
				atomic_store(&v->pending_dl, 0);

				/* Trigger the van user DL actions. */
				van_int_write(v, t);
			}
		
			/* Increment and test the start of the message list. */
			q->tail++;
			if (q->tail >= OS_SHM_Q_SIZE)
				q->tail = 0;
		}
	}
}

/**
 * van_file_create() - create the shared memory file.
 *
 * Return:	None.
 **/
static void van_file_create(void)
{
	int oflag, fd, rv;
	mode_t mode;
	
	/* Control flags for the open call. */
	oflag = O_RDWR | O_CREAT | O_EXCL;
	mode =  S_IRUSR | S_IWUSR | S_IRGRP | S_IWGRP | S_IROTH | S_IWOTH;

	fd = open(OS_SHM_FILE, oflag, mode);
	OS_TRAP_IF(fd == -1);

	/* Stretch the file size. */
	rv = lseek(fd, OS_SHM_SIZE - 1, SEEK_SET);
	OS_TRAP_IF(rv == -1);

	/* Write just one byte at the end and close the file. */
	rv = write(fd, "", 1);
	OS_TRAP_IF(rv == -1);

	rv = close(fd);
	OS_TRAP_IF(rv != 0);
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

	/* Create the shared memory file. */
	van_file_create();
	
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
 * os_van_ripcord() - release critical van device resources.
 *
 * @coverage:  if 0, release critical device resoures.
 *
 * Return:	None.
 **/
void os_van_ripcord(int coverage)
{
	os_dev_t *v;

	/* Entry conditon. */
	v = os_van_device;

	/* Test the van device state. */
	if (v == NULL || coverage)
		return;

	/* Remove the reference to the van semaphore. */
	if (v->my_int) {
		sem_close(v->my_int);
		sem_unlink(v->my_int_name);
	}
	
	/* Remove the reference to the python semaphore. */
	if (v->other_int) {
		sem_close(v->other_int);
		sem_unlink(v->other_int_name);
	}

	/* Test the file descriptor of the shared memory file. */
	if (v->fd == 0)
		return;

	/* Delete the mapping of the shared memory area. */
	if (v->start != 0)
		munmap(v->start, v->size);

	/* Close and remove the shared memory file. */
	close(v->fd);
	remove(OS_SHM_FILE);
}

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
 * os_van_init() - request the entry points for the van shared memory device.
 *
 * @conf:  pointer to the trace configuration.
 * @op:    pointer to the entry points of the shm device.
 *
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
	op->write       = van_write;
	op->zread       = van_zread;
	op->read        = van_read;
	op->aio_action  = van_aio_action;
	op->aio_write   = van_aio_write;
	op->aio_read    = van_aio_read;
}
