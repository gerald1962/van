// SPDX-License-Identifier: GPL-2.0

/*
 * Operating system interfaces.
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
#include "os.h"          /* Operating system: os_sem_create() */
#include "os_private.h"  /* Local interfaces of the OS: os_trap_init() */

/*============================================================================
  EXPORTED INCLUDE REFERENCES
  ============================================================================*/
/*============================================================================
  LOCAL NAME CONSTANTS DEFINITIONS
  ============================================================================*/
#define P  "I>"  /* Prompt for the interrup thread. */

#define CAB_SHM_FILE  "/tmp/van.shm"  /* Name of the shared memory file. */

/* Size of the shm file: queue + transfer buffer + alignment */
#define CAB_SHM_SIZE  \
	( ( sizeof(cab_queue_t) + (OS_BUF_SIZE) + sizeof(int) ) * CAB_COUNT )

#define CAB_Q_SIZE     4  /* Data transfer queue size about shm. */

/* Configuration of the controll cable endpoints . */
#define CAB_CB_INT  "van_c_ba_int" /* Ctrl-battery interrupt simulation. */
#define CAB_CD_INT  "van_c_di_int" /* Ctrl-display interrupt simulation. */
#define CAB_CB_N    "/ctrl_batt"   /* Name of the battery shm device. */
#define CAB_CD_N    "/ctrl_disp"   /* Name of the display shm device. */
#define CAB_CB_T    "c_batt_int"   /* Name of the ctrl-batt int. handler. */
#define CAB_CD_T    "c_disp_int"   /* Name of the ctrl-disp int. handler. */

/* Configuration of the neighbour endpoints. */
#define CAB_BA_INT  "van_batt_int"  /* Battery interrupt simulation. */
#define CAB_DI_INT  "van_disp_int"  /* Display interrupt simulation. */
#define CAB_BA_N    "/battery"      /* Name of the batter shm device. */
#define CAB_DI_N    "/display"      /* Name of the display shm device. */
#define CAB_BA_T    "battery_int"   /* Name of the batery int. handler. */
#define CAB_DI_T    "display_int"   /* Name of the display int. handler. */

/* Start index of the van-python and van-tcl shared memory resources. */
#define CAB_CB_OFFS_V  0
#define CAB_CD_OFFS_V  ( (sizeof(cab_queue_t) * 2) + ((OS_BUF_SIZE) * 2) + sizeof(int) )

/* Aligned start index of the van-python and van-tcl shared memory resources. */
#define CAB_CB_OFFS_A  CAB_CB_OFFS_V
#define CAB_CD_OFFS_A  CAB_ALIGN(CAB_CD_OFFS_V, sizeof(int))

#define OS_SHM_Q_SIZE     4  /* Data transfer queue size about shm. */
#define OS_THREAD_Q_SIZE  8  /* Input queue size of the van/py thread. */

#if defined(USE_OS_RT)
#define PRIO    OS_THREAD_PRIO_HARDRT
#else
#define PRIO    OS_THREAD_PRIO_SOFTRT
#endif

#define Q_SIZE  OS_THREAD_Q_SIZE

/*============================================================================
  MACROS
  ============================================================================*/
/**
 * CAB_ALIGN - calculation of the alignment value. 
 *
 * @int_:   this integer value shall be aligned.
 * @size_:  alignment value.
 *
 * Return:	the aligned interger value.
 **/
#define CAB_ALIGN(int_, size_)  ( ((int_) + (size_) - 1) & ~( (size_) - 1) )

/*============================================================================
  LOCAL TYPE DEFINITIONS
  ============================================================================*/
/**
 * cab_type_t - ids of the shared memory devices.
 *
 * CAB_CB:  id of the ctrl-batterry shared memory device.
 * CAB_CD:  id of the ctrl-display shared memory device.
 * CAB_BA:  id of the battery shared memory device.
 * CAB_DI:  id of the display shared memory device.
 * CAB_COUNT:  number of the shared memory devices.
 **/
typedef enum {
	CAB_CB,
	CAB_CD,
	CAB_BA,
	CAB_DI,
	CAB_COUNT
} cab_type_t;

/**
 * shm_shell_t - shared memory shell for inter process communication.
 *
 * @sem_n:    list of the named semaphores to simulate interrupts.
 * @file_n:   name of the shared memory file.
 * @fd:       file descriptor of the shared memory file.
 * @size:     size of the shared memory area.
 * @start:    start address of the shared memory area.
 * @creator:  if 1, this user has created the shm resources.
 **/
typedef struct {
	char  *sem_n[CAB_COUNT];
	char  *file_n;
	int    fd;
	int    size;
	void  *start;
	int    creator;
} cab_shell_t;

/**
 * cab_conf_t - shared memory configuration.
 *
 * @id:           controller/follower device id.
 * @is_ctrl:      1, if the it is a controller device.
 * @dev_name:     device name.
 * @thr_name:     interrupt thread name.
 * @my_int_n:     my named semaphore or interrupt name.
 * @other_int_n:  named semaphore or interrupt name of the other device.
 * @start_idx:    start index of the shared memory area.
 **/
typedef struct {
	cab_type_t id;
	int    is_ctrl;
	char  *dev_name;
	char  *thr_name;
	char  *my_int_n;
	char  *other_int_n;
	int    start_idx;
} cab_conf_t;

/**
 * cab_msg_t - shm input message with payload status information.
 *
 * @id:        message counter.
 * @size:      size of the payload.
 * @consumed:  if 1, the payload has been processed.
 **/
typedef struct {
	unsigned char id;
	int  size;
	int  consumed;
} cab_msg_t;

/**
 * cab_queue_t - shm input queue of van or py.
 *
 * @ring:   list of the input messages.
 * @tail:   start of the message list.
 * @head:   end of the message list.
 **/
typedef struct {
	cab_msg_t  ring[CAB_Q_SIZE];
	int  tail;
	int  head;
} cab_queue_t;

/** 
 * cab_io_t - element of the input or output channel.
 *
 * @queue:    queue with control information.
 * @p_count:  size of the payload.
 * @b_size:   size of the channel buffer.
 * @b_start:  start address of the channel buffer.
 * @b_end:    end address of the channel buffer.
 **/
typedef struct {
	cab_queue_t  *queue;
	atomic_int    p_count;
	int    b_size;
	char  *b_start;
	char  *b_end;
} cab_io_t;

/**
 * cab_dev_t - shared memory device state.
 *
 * @id:                device id.
 * @name:              name of the shared memory device.
 * @mode:              devie mode like O_NBLOCK: non blocking I/O operations.
 * @my_int:            points to the my named semaphore.
 * @other_int:         points to the named semaphore of the other device.
 * @thread:            address of the interrup handler/thread.
 *
 * @down:              if 1, ignore the interrupt.
 *
 * @in:                input channel.
 * @pending_in:        if 1, the input buffer is pending.
 * @out:               output channel.
 * @pending_out:       if 1, the ouput buffer is pending.
 * @msg_id;            message counter about shared memory.
 * @q_mutex:           critical section in cab_queue_add.
 *
 * @aio_cb:            aio read and write callbacks.
 * @aio_use:           if 1, the aio actions shall be executed.
 * @aio_wr_trigger:    if 1, the int handler shall invoke the aio write_cb.
 * @aio_rd_trigger:    if 1, the int handler shall invoke the aio read_cb.
 * @aio_mutex:         protect the critical sections in aio_action.
 *
 * @suspend_writer:    suspend the write caller in write.
 * @suspend_reader:    suspend the read caller in read.
 * @sync_write:        if 1, the user has invoked write.
 * @sync_read:         if 1, the user has invoked read.
 * @sync_wait          if 1, update the wait condition.
 * @wait_id;           assignd id, to select the wait element.
 * @write_mutex:       protect the critical sections in write.
 * @read_mutex:        protect the critical sections in read.
 **/
typedef struct {
	cab_type_t  id;
	char   *name;
	int     mode;
	sem_t  *my_int;
	sem_t  *other_int;
	void   *thread;

	atomic_int       down;
	
	cab_io_t         in;
	atomic_int       pending_in;
	cab_io_t         out;
	atomic_int       pending_out;
	unsigned char    msg_id;
	pthread_mutex_t  q_mutex;
	
	os_aio_cb_t      aio_cb;
	atomic_int       aio_use;
	atomic_int       aio_wr_trigger;
	atomic_int       aio_rd_trigger;
	pthread_mutex_t  aio_mutex;

	sem_t   suspend_writer;
	sem_t   suspend_reader;
	atomic_int       sync_read;
	atomic_int       sync_write;
	atomic_int       sync_wait;
	int              wait_id;

	pthread_mutex_t  write_mutex;
	pthread_mutex_t  read_mutex;
} cab_dev_t;

/**
 * cab_wait_t - state of a suspended caller in os_c_wait, who is waiting for a
 * read or write event.
 *
 * @mutex:     protect the critical section in the wait operations.
 * @id:        id of the wait element.
 * @assigned:  if 1, the element has been addressed.
 * @suspend:   suspend the wait caller.
 * @probe:     if 1, the caller shall probe all input and output wires.
 **/
typedef struct {
	pthread_mutex_t  mutex;

	struct cab_wait_elem_s {
		int id;
		int assigned;
		sem_t       suspend;
		atomic_int  probe;
	} elem[CAB_COUNT];
} cab_wait_t;

/*============================================================================
  LOCAL DATA
  ============================================================================*/
/* Pointer to the OS configuration */
static os_conf_t *os_conf_p;

/* Pillars of the shared memory transfer. */
static cab_shell_t cab_shell = {
	{ CAB_CB_INT, CAB_CD_INT, CAB_BA_INT, CAB_DI_INT },
	NULL
};

/* Configuration of the shared memory devices. */
static cab_conf_t cab_conf[] = {
	{ CAB_CB, 1, CAB_CB_N, CAB_CB_T, CAB_CB_INT, CAB_BA_INT, CAB_CB_OFFS_A },
	{ CAB_CD, 1, CAB_CD_N, CAB_CD_T, CAB_CD_INT, CAB_DI_INT, CAB_CD_OFFS_A },
	{ CAB_BA, 0, CAB_BA_N, CAB_BA_T, CAB_BA_INT, CAB_CB_INT, CAB_CB_OFFS_A },
	{ CAB_DI, 0, CAB_DI_N, CAB_DI_T, CAB_DI_INT, CAB_CD_INT, CAB_CD_OFFS_A },
	{ 0, }
};

/* List of all shared memory devices. */
static cab_dev_t *cab_device[CAB_COUNT];

/* State of the suspended caller in the wait operations. */
static cab_wait_t cab_wait;

/*============================================================================
  LOCAL FUNCTION PROTOTYPES
  ============================================================================*/
/*============================================================================
  LOCAL FUNCTIONS
  ============================================================================*/
/**
 * cab_aio_q_add() - send the control message asynchronously.
 *
 * dev:       pointer to the cable device.
 * count:     size of the ouput payload.
 * consumed:  if 1, the input buffer has been released.
 *
 * Return:	None.
 **/
static void cab_aio_q_add(cab_dev_t *dev, int count, int consumed)
{
	cab_queue_t *q;
	cab_msg_t *msg;
	int head;
	
	/* Get the pointer to the output control queue. */
	q = dev->out.queue;
	
	/* Fill the next free message. */
	head = q->head;
	msg = &q->ring[head];
	msg->id       = dev->msg_id;
	msg->size     = count;
	msg->consumed = consumed;

	/* Increment the message counter. */
	dev->msg_id++;
	
	/* Increment and test the end of the message list. */
	head++;
	if (head >= CAB_Q_SIZE)
		head = 0;

	OS_TRAP_IF(head == q->tail);
	q->head = head;

	OS_TRACE(("%s %s: msg-snd: [i=%u, s=%d, c=%d]\n", P, dev->name,
		  msg->id, msg->size, msg->consumed));

	/* Trigger the interrupt of the other device. */
	os_sem_release(dev->other_int);
}

/**
 * cab_wait_trigger() - the interrupt handler resumes the suspended os_c_wait
 * caller, if events are available for the input or output wire for all devices,
 * that interest them.
 *
 * @dev:      pointer to the shm device.
 * event:     readable or writeable.
 *
 * Return:	None.
 **/
static void cab_wait_trigger(cab_dev_t *dev, char *event)
{
	struct cab_wait_elem_s *elem;
	int probe;
	
	/* Get the pointer to the wait state. */
	elem = &cab_wait.elem[dev->wait_id];
	OS_TRAP_IF(! elem->assigned);
	
	/* Update the wait condition. */
	probe = atomic_exchange(&elem->probe, 1);
	if (! probe) {
		OS_TRACE(("%s %s: wakeup: [i=%d, e=%s]\n",
			  P, dev->name, dev->wait_id, event));
		os_sem_release(&elem->suspend);
	}
}

/**
 * cab_int_write() - resume the suspend caller in os_c_write, os_c_wait or request
 * output data from the aio user.
 *
 * @dev:  pointer to the device state.
 * @out:  pointer to the output channel.
 *
 * Return:	None.
 **/
static void cab_int_write(cab_dev_t *dev, cab_io_t *out)
{
	int aio_use, sync_write, sync_wait, pending_out, count;

	/* Test the aio status request. */
	aio_use = atomic_load(&dev->aio_use);
	if (! aio_use) {
		/* Test the write method. */
		sync_write = atomic_exchange(&dev->sync_write, 0);
		if (sync_write) {
			os_sem_release(&dev->suspend_writer);
		}
		else {
			/* Test the wait condition. */
			sync_wait = atomic_load(&dev->sync_wait);
			if (sync_wait)
				cab_wait_trigger(dev, "writeable");
		}
		return;
	}

	/* Reset the write trigger. */
	atomic_store(&dev->aio_wr_trigger, 0);

	/* Test the release status of the output buffer. */
	pending_out = atomic_load(&dev->pending_out);
	if (pending_out)
		return;
	
	/* Request output data from the aio user. */
	count = dev->aio_cb.write_cb(dev->id, out->b_start, out->b_size);
	if (count < 1)
		return;

	/* Save the size of the payload. */
	atomic_store(&out->p_count, count);

	/* Change the output state. */
	atomic_store(&dev->pending_out, 1);
					
	/* Send the control message to the other device. */
	cab_aio_q_add(dev, count, 0);
}

/**
 * cab_int_read() - pass the input payload to the async. caller or resume the
 * suspended caller in os_c_read, os_c_zread or os_c_wait.
 *
 * @dev:    pointer to the device state.
 * @in:     pointer to the input channel.
 * @count:  number of the pending input characters.
 *
 * Return:	None.
 **/
static void cab_int_read(cab_dev_t *dev, cab_io_t *in, int count)
{
	int aio_use, sync_read, consumed, sync_wait;

	/* Test the aio status. */
	aio_use = atomic_load(&dev->aio_use);
	if (aio_use) {
		/* Reset the read trigger. */
		atomic_store(&dev->aio_rd_trigger, 0);

		/* Test the number of the pending input characters. */
		if (count < 1)
			return;
		
		/* Pass the input payload to the aio user. */
		consumed = dev->aio_cb.read_cb(dev->id, in->b_start, count);

		/* Update the input state. */
		OS_TRAP_IF(consumed > count || consumed < 0);
		if (consumed != count) {
			atomic_store(&in->p_count, count - consumed);
			return;
		}

		/* Reset the input state. */
		atomic_store(&in->p_count, 0);
		
		/* Release the pending input buffer. */
		cab_aio_q_add(dev, 0, 1);
	}
	else {
		/* Save the number of the received bytes. */
		atomic_store(&in->p_count, count);
			
		/* Test the read method. */
		sync_read = atomic_exchange(&dev->sync_read, 0);
		if (sync_read) {
			os_sem_release(&dev->suspend_reader);
		}
		else {
			/* Test the wait condition. */
			sync_wait = atomic_load(&dev->sync_wait);
			if (sync_wait)
				cab_wait_trigger(dev, "readable");
		}
	}
}

/**
 * cab_int_exec() - the int thread waits for the int triggered by the involved
 * devices.
 *
 * @msg:  generic input message.
 *
 * Return:	None.
 **/
static void cab_int_exec(os_queue_elem_t *g_msg)
{
	cab_queue_t *q;
	cab_msg_t *msg;
	cab_dev_t *dev;
	cab_io_t *in, *out;
	char *n;
	int down, pending_out;

	/* Get the address of the device state. */
	dev = g_msg->param;

	/* Get the pointer of the input control queue. */
	in  = &dev->in;
	out = &dev->out;
	q   = in->queue;
	n   = dev->name;

	/* Loop thru the interrupts triggered by the involved devices. */
	for(;;) {
		/* Test the access method to the device. */
		if (dev->aio_use) {
			/* Test the input payload state. */
			if (dev->aio_rd_trigger)
				cab_int_read(dev, in, in->p_count);
			
			/* Test the aio status, request data from the user and
			 *  inform the other device. */
			if (dev->aio_wr_trigger)
				cab_int_write(dev, out);
		}		

		OS_TRACE(("%s %s:[s:ready, m:int] -> [s:suspended]\n", P, n));

		/* Wait for the int trigger. */
		os_sem_wait(dev->my_int);

		/* Test the thread state. */
		down = atomic_load(&dev->down);
		if (down) {
			OS_TRACE(("%s %s:[s:suspended, m:int] -> [s:down]\n",
				  P, n));
			return;
		} else {
			OS_TRACE(("%s %s:[s:suspended, m:int] -> [s:ready]\n",
				  P, n));
		}
	
		/* Test the mailbox. */
		while (q->tail != q->head) {
			msg = &q->ring[q->tail];

			OS_TRACE(("%s %s: msg-rcv: [i=%u, s=%d, c=%d]\n", P, n,
				  msg->id, msg->size, msg->consumed));
			
			/* In case a brief is there, inform the adressee. */
			if (msg->size > 0) {
				/* Trigger the user actions. */
				cab_int_read(dev, in, msg->size);
			}

			/* Test the progess of the procesing of the output
			 * payload. */
			if (msg->consumed) {
				/* Release the output buffer. */
				pending_out = atomic_exchange(&dev->pending_out, 0);
				OS_TRAP_IF(! pending_out);

				/* Trigger the user actions. */
				cab_int_write(dev, out);
			}
		
			/* The current message from the shm input queue was
			 * processed. We prepare us for the next mailbox entry. */
			q->tail++;
			if (q->tail >= CAB_Q_SIZE)
				q->tail = 0;
		}
	}
}

/**
 * cab_dev_get() - map the device id to the device state.
 *
 * @dev_id:  device id.
 *
 * Return:	the pointer to the device.
 **/
static cab_dev_t *cab_dev_get(int dev_id)
{
	cab_dev_t **dev;
	int i;

	/* Loop thru the device list. */
	for (i = 0, dev = cab_device; i < CAB_COUNT; i++, dev++) {
		if (*dev == NULL)
			continue;

		/* Test the device id. */
		if ((*dev)->id == dev_id)
			break;
	}

	/* Final condition. */
	OS_TRAP_IF(i >= CAB_COUNT);

	return *dev;
}

/**
 * cab_q_add() - send the control message to the other device.
 *
 * dev:       pointer to the shm device.
 * count:     size of the output payload.
 * consumed:  if 1, the input buffer has been released.
 *
 * Return:	None.
 **/
static void cab_q_add(cab_dev_t *dev, int count, int consumed)
{
	cab_queue_t *q;
	cab_msg_t *msg;
	int head;
	
	/* Enter the critical section. */
	os_cs_enter(&dev->q_mutex);

	/* Get the pointer of the output control queue. */
	q = dev->out.queue;
	
	/* Fill the next free message. */
	head = q->head;
	msg = &q->ring[head];
	msg->id       = dev->msg_id;
	msg->size     = count;
	msg->consumed = consumed;
	
	/* Increment the message counter. */
	dev->msg_id++;

	/* Increment and test the end of the message list. */
	head++;
	if (head >= CAB_Q_SIZE)
		head = 0;

	OS_TRAP_IF(head == q->tail);
	q->head = head;

	OS_TRACE(("%s %s: msg-snd: [i=%u, s=%d, c=%d]\n", P, dev->name,
		  msg->id, msg->size, msg->consumed));

	/* Trigger the interrupt of the other device. */
	os_sem_release(dev->other_int);

	/* Leave the critical section. */
	os_cs_leave(&dev->q_mutex);	
}

/**
 * cab_io_map() - Get the shared memory addresses for the cable.
 *
 * @addr:   start address of the shared memory area.
 * @first:  pointer to first channel.
 * @second: pointer to second channel.
 *
 * Return:	None.
 **/
static void cab_io_map(void *addr, cab_io_t *first, cab_io_t *second) {
	
	/* Get the addresses of the control queues. */
	first->queue = (cab_queue_t *) addr;
	addr = addr + sizeof(cab_queue_t);
	
	second->queue = (cab_queue_t *) addr;
	addr = addr + sizeof(cab_queue_t);
		
	/* Get the address of the input and output buffer. */
	first->b_size  = OS_BUF_SIZE;
	first->b_start = (char *) addr;
	first->b_end   = (char *) addr + OS_BUF_SIZE - 1;
	addr = (char *) addr + OS_BUF_SIZE;
	
	/* Get the address of the output buffer. */
	second->b_size  = OS_BUF_SIZE;
	second->b_start = (char *) addr;
	second->b_end   = (char *) addr + OS_BUF_SIZE - 1;
}

/**
 * shm_conf_get() - search for the shared memory device configuration.
 *
 * @conf:  addess of the first shared memory configuration.
 * @name:  pointer to the controller/follower device name.
 *
 * Return:	the address of the found configuration.
 **/
static cab_conf_t *cab_conf_get(cab_conf_t *conf, char *name)
{
	cab_conf_t *c;

	/* Loop thru the list of the shm configurations. */
	for (c = conf; c->dev_name != NULL; c++) {
		/* Test the device name. */
		if (os_strcmp(c->dev_name, name) == 0)
			break;
	}

	/* Final condition. */
	OS_TRAP_IF(c->dev_name == NULL);

	return c;
}

/**
 * cab_map() - create the clean shared memory area.
 *
 * Return:	None.
 **/
static void cab_map(void)
{
	struct stat statbuf;
	cab_shell_t *s;
	int rv, prot;

	/* Get the address of the shm shell. */
	s = &cab_shell;
	
	/* Open the shared memory file. */
	s->file_n = CAB_SHM_FILE;
	s->fd = open(CAB_SHM_FILE, O_RDWR);
	OS_TRAP_IF(s->fd < 0);

	/* Get information about the shared memory file. */
	rv = fstat(s->fd, &statbuf);
	s->size = statbuf.st_size;
	OS_TRAP_IF(rv != 0 || statbuf.st_size != CAB_SHM_SIZE);

	/* Define the access to the shared memory. */
	prot = PROT_READ | PROT_WRITE;
	
	/* Define the size of the shm area about the shm file. */
	s->start = mmap(NULL, s->size, prot, MAP_SHARED, s->fd, 0);
	OS_TRAP_IF(s->start == MAP_FAILED);
}

/**
 * cab_create() - create the named semaphores for interrupt simulation and the shared
 * memory file.
 *
 * Return:	None.
 **/
static void cab_create(void)
{
	mode_t mode;
	sem_t *sem;
	char *name;
	int i, oflag, rv, fd;

	/* Control flags for the creation of the named semaphored. */
	oflag = O_CREAT | O_EXCL;
	mode =  S_IRUSR | S_IWUSR | S_IRGRP | S_IWGRP | S_IROTH | S_IWOTH;

	/* Loop thru the named semaphore names. */
	for (i = 0; i < CAB_COUNT; i++) {
		name = cab_shell.sem_n[i];
		
		/* Create the named semaphore. */
		sem = sem_open(name, oflag, mode, 0);
		OS_TRAP_IF(sem == SEM_FAILED);

		/* Remove the reference to the named semaphore. */
		rv = sem_close(sem);
		OS_TRAP_IF(rv != 0);
	}

	/* Control flags for the creation of the shared memory file. */
	oflag = O_RDWR | O_CREAT | O_EXCL;
	mode  = S_IRUSR | S_IWUSR | S_IRGRP | S_IWGRP | S_IROTH | S_IWOTH;

	fd = open(CAB_SHM_FILE, oflag, mode);
	OS_TRAP_IF(fd == -1);

	/* Stretch the file size. */
	rv = lseek(fd, CAB_SHM_SIZE - 1, SEEK_SET);
	OS_TRAP_IF(rv == -1);

	/* Write just one byte at the end and close the file. */
	rv = write(fd, "", 1);
	OS_TRAP_IF(rv == -1);

	rv = close(fd);
	OS_TRAP_IF(rv != 0);
}

/*============================================================================
  EXPORTED FUNCTIONS
  ============================================================================*/
/**
 * os_c_close() - the user shall call this function to remove the
 * shared memory ressources.
 *
 * @dev_id:  id of the shared memory device.
 *
 * Return:	None.
 **/
void os_c_close(int dev_id)
{
	cab_dev_t *dev;
	int rv, i;

	/* Map the id to the device state. */
	dev = cab_dev_get(dev_id);
	
	/* Resume the interrupt thread, to terminate it. */
	atomic_store(&dev->down, 1);
	os_sem_release(dev->my_int);
	
	/* Delete the interrupt handler. */
	os_thread_destroy(dev->thread);

	/* Remove the reference to the named semaphores. */
	rv = sem_close(dev->my_int);
	OS_TRAP_IF(rv != 0);
	
	rv = sem_close(dev->other_int);
	OS_TRAP_IF(rv != 0);

	/* Destroy the semaphore for os_c_write(). */
	os_sem_delete(&dev->suspend_writer);
	
	/* Destroy the semaphore for os_c_read(). */
	os_sem_delete(&dev->suspend_reader);
	
	/* Delete the mutex for the critical sections in os_c_write. */
	os_cs_destroy(&dev->write_mutex);

	/* Delete the mutex for the critical sections in os_c_zread. */
	os_cs_destroy(&dev->read_mutex);

	/* Delete the mutex for the critical sections in aio_action and in aio_write. */
	os_cs_destroy(&dev->aio_mutex);
	
	/* Destroy the mutex for the critical sections in cab_q_add. */
	os_cs_destroy(&dev->q_mutex);
	
	/* Clear the input queue of the endpoint. */
	dev->in.queue->tail = dev->in.queue->head;

	/* Free the the shared memory device. */
	for (i = 0; i < CAB_COUNT; i++) {
		if (cab_device[i] == dev)
			break;
	}
	
	/* Test the release conditon. */
	OS_TRAP_IF(i >= CAB_COUNT);
	cab_device[i] = NULL;
	
	OS_FREE(dev);
}

/** 
 * os_c_wait() - the caller shall be suspended, until a read or write event is
 * available for the wires of a cable.
 *
 * @wait_id:  id of the wait element.
 *
 * Return:	None.
 **/
void os_c_wait(int id)
{
	struct cab_wait_elem_s *elem;
	int probe;

	/* Entry condition. */
	OS_TRAP_IF(id < 0 || id >= CAB_COUNT);
	
	/* Get the pointer to the wait state. */
	elem = &cab_wait.elem[id];
	OS_TRAP_IF(! elem->assigned);
	
	/* Test the state of the merged state of all input and output wires. */
	probe = atomic_exchange(&elem->probe, 0);
	if (! probe)
		os_sem_wait(&elem->suspend);
}

/** 
 * os_c_wait_release() - free the wait element.
 *
 * @id:  id of the assigned element.
 *
 * Return:	None.
 **/
void os_c_wait_release(int id)
{
	struct cab_wait_elem_s *elem;
	cab_wait_t *w;
	
	/* Get the pointer to the wait state. */
	w = &cab_wait;
	
	/* Enter the critical section. */
	os_cs_enter(&w->mutex);
	
	/* Entry condition. */
	OS_TRAP_IF(id < 0 || id >= CAB_COUNT);

	/* Get the pointer to the wait element. */
	elem = &w->elem[id];
	OS_TRAP_IF(! elem->assigned);
	
	/* Free the wait element. */
	elem->assigned = 0;
		
	/* Leave the critical section. */
	os_cs_leave(&w->mutex);
}

/** 
 * os_c_wait_init() - if a device has been opened with O_NBLOCK - non blocking mode, it is allowed to suspend the
 * caller. He wants be resumed if reading or writing is possible again, i.e. if
 * one or the other wire of a cable can be used. 
 *
 * @list:  list of device ids.
 * @len:   number of the list elements.
 *
 * Return:	the id of the assigned wait element.
 **/
int os_c_wait_init(int *list, int len)
{
	struct cab_wait_elem_s *elem;
	cab_wait_t *w;
	cab_dev_t *dev;
	int i, wait_id;
	
	/* Entry condition. */
	OS_TRAP_IF(list == NULL || len < 1);

	/* Get the pointer to the wait state. */
	w = &cab_wait;
	
	/* Enter the critical section. */
	os_cs_enter(&w->mutex);

	/* Search for a free wait element. */
	for (i = 0, elem = w->elem; i < CAB_COUNT; i++, elem++) {
		if (! elem->assigned)
			break;
	}

	OS_TRAP_IF(i >= CAB_COUNT);
	wait_id = i;
	elem->assigned = 1;

	/* Run thru the device id list. */
	for (i = 0; i < len; i++) {
		/* Request the pointer to the device state. */
		dev = cab_dev_get(list[i]);

		/* Test the device mode. */
		OS_TRAP_IF(dev->mode != O_NBLOCK);

		/* Propagate the wait condition to the device state. */
		dev->wait_id = wait_id;
		atomic_store(&dev->sync_wait, 1);
	}

	/* Leave the critical section. */
	os_cs_leave(&w->mutex);

	return wait_id;
}

/**
 * os_c_zread() - the caller for incoming payload. With each successiv call, the
 * reference to the previous call is released automatically.
 *
 * @dev_id:  id of the shared memory device.
 * @buf:     pointer to the received payload.
 * @count:   size of the destination buffer.
 *
 * Return:	number of the received bytes.
 **/
int os_c_zread(int dev_id, char **buf, int count)
{
	cab_dev_t *dev;
	cab_io_t *in;
	int pending_in, n;
	
	/* Map the id to the device state. */
	dev = cab_dev_get(dev_id);
	
	/* Get the pointer to the input channel. */
	in = &dev->in;
	
	/* Enter the critical section. */
	os_cs_enter(&dev->read_mutex);

	/* If the input payload is pending, send the control message. */
	pending_in = atomic_exchange(&dev->pending_in, 0);
	if (pending_in)
		cab_q_add(dev, 0, 1);
	
	/* Test the buffer state. */
	if (buf == NULL || count < 1) {
		/* Leave the critical section. */
		os_cs_leave(&dev->read_mutex);	
		return 0;
	}

	/* Test the device mode. */
	if (dev->mode == O_NBLOCK) {
		/* Get the number of the received bytes. */
		n = atomic_exchange(&in->p_count, 0);
		if (n > 0)
			goto l_zcopy;
		else
			goto l_leave;
	}

	/* Update the sync. read flag. */
	atomic_store(&dev->sync_read, 1);
	
	/* Wait for the input payload. */
	for(;;) {
		/* Get the number of the received bytes. */
		n = atomic_exchange(&in->p_count, 0);

		/* Test the number of the received bytes. */
		if (n < 1) { 
			/* Suspend the read user. */
			os_sem_wait(&dev->suspend_reader);
			continue;
		}

		break;
	}

	/* Release the sync. read operation. */
	atomic_store(&dev->sync_read, 0);

l_zcopy:
	/* Get the pointer to the received payload. */
	atomic_store(&dev->pending_in, 1);
	*buf = in->b_start;

l_leave:
	/* Leave the critical section. */
	os_cs_leave(&dev->read_mutex);	

	return n;
}

/**
 * os_c_read() - the caller waits for the incoming payload.
 *
 * @dev_id:  id of the shared memory device.
 * @buf:     pointer to the received payload.
 * @count:   size of the destination buffer.
 *
 * Return:	number of the received bytes.
 **/
int os_c_read(int dev_id, char *buf, int count)
{
	cab_dev_t *dev;
	cab_io_t *in;
	int n;

	/* Entry conditon. */
	OS_TRAP_IF(buf == NULL || count < 1);

	/* Map the id to the device state. */
	dev = cab_dev_get(dev_id);
	
	/* Get the pointer to the input channel. */
	in = &dev->in;
	
	/* Enter the critical section. */
	os_cs_enter(&dev->read_mutex);

	/* Test the device mode. */
	if (dev->mode == O_NBLOCK) {
		/* Get the number of the received bytes. */
		n = atomic_exchange(&in->p_count, 0);
		if (n > 0)
			goto l_copy;
		else
			goto l_leave;
	}
	
	/* Define the read method. */
	atomic_store(&dev->sync_read, 1);

	/* Wait for the input payload. */
	for(;;) {
		/* Get the number of the received bytes. */
		n = atomic_exchange(&in->p_count, 0);

		/* Test the number of the received bytest. */
		if (n < 1) { 
			/* Suspend the read user. */
			os_sem_wait(&dev->suspend_reader);
			continue;
		}

		break;
	}
	
	/* Release the sync. read operation. */
	atomic_store(&dev->sync_read, 0);

l_copy:
	/* Test the user buffer. */
	OS_TRAP_IF(count < n);
	
	/* Copy the received payload. */
	os_memcpy(buf, count, in->b_start, n);

	/* Release the pending input buffer. */
	cab_q_add(dev, 0, 1);

l_leave:
	/* Leave the critical section. */
	os_cs_leave(&dev->read_mutex);	

	return n;
}

/**
 * os_c_write() - send the output payload to the other device and suspend the
 * caller until the payload has been processed.
 *
 * @dev_id:  id of the shared memory device.
 * @buf:     pointer to the payload.
 * @count:   size of the payload.
 *
 * Return:	the number of the copied bytes.
 **/
int os_c_write(int dev_id, char *buf, int count)
{
	cab_dev_t *dev;
	cab_io_t *out;
	int n, pending_out;
	
	/* Initialize the return value. */
	n = 0;
			
	/* Entry conditon. */
	OS_TRAP_IF(buf == NULL || count < 1);

	/* Map the id to the device state. */
	dev = cab_dev_get(dev_id);
	
	/* Get the pointer to the output channel. */
	out = &dev->out;
	
	/* Enter the critical section. */
	os_cs_enter(&dev->write_mutex);

	/* Copy the state of the output buffer. */
	pending_out = atomic_load(&dev->pending_out);

	/* Test the device mode. */
	if (dev->mode != O_NBLOCK) {
		/* Change the output state. */
		atomic_store(&dev->sync_write, 1);
	}
	else {
		/* Test the state of the output buffer. */
		if (pending_out)
			goto l_leave;
	}
	
	/* Test the state of the output buffer. */
	OS_TRAP_IF(out->b_size < count || pending_out);

	/* Fill the output buffer. */
	os_memcpy(out->b_start, out->b_size, buf, count);

	/* Save the size of the payload. */
	atomic_store(&out->p_count, count);
		
	/* Change the output state. */
	atomic_store(&dev->pending_out, 1);
		
	/* Send the control message to the other device. */
	cab_q_add(dev, count, 0);

	/* Suspend the caller until the action is executed. */
	if (dev->mode != O_NBLOCK)
		os_sem_wait(&dev->suspend_writer);

	/* Update the return value. */
	n = count;

l_leave:
	/* Leave the critical section. */
	os_cs_leave(&dev->write_mutex);
		
	return n;
}

/**
 * os_c_aread() - this async. I/O operation triggers the interrup, to start or
 * restart the receive actions.
 *
 * @dev_id:  id of the shared memory device.
 *
 * Return:	None.
 **/
void os_c_aread(int dev_id)
{
	cab_dev_t *dev;

	/* Map the id to the device state. */
	dev = cab_dev_get(dev_id);
	
	/* Enter the critical section. */
	os_cs_enter(&dev->aio_mutex);

	/* Extended entry conditon. */
	OS_TRAP_IF(! dev->aio_use);

	/* Update the read trigger. */
	atomic_store(&dev->aio_rd_trigger, 1);

	/* Resume the interrupt handler. */
	os_sem_release(dev->my_int);

	/* Leave the critical section. */
	os_cs_leave(&dev->aio_mutex);	
}

/**
 * os_c_awrite() - this async. I/O operation triggers the interrupt, to start
 * or restart the send actions.
 *
 * @dev_id:  id of the shared memory device.
 *
 * Return:	None.
 **/
void os_c_awrite(int dev_id)
{
	cab_dev_t *dev;

	/* Map the id to the device state. */
	dev = cab_dev_get(dev_id);
	
	/* Enter the critical section. */
	os_cs_enter(&dev->aio_mutex);

	/* Extended entry conditon. */
	OS_TRAP_IF(! dev->aio_use);

	/* Update the write trigger. */
	atomic_store(&dev->aio_wr_trigger, 1);

	/* Resume the interrupt handler. */
	os_sem_release(dev->my_int);

	/* Leave the critical section. */
	os_cs_leave(&dev->aio_mutex);	
}

/**
 * os_c_action() - install the read and write callback for the asynchronous
 * operations. The reconfiguration of the async. I/O operations is not
 * supported. It is recommended to decide themselves for aio immediately after
 * os_c_open before data transfer starts, in order to avoid deadlocks in the sync.
 * operations.
 *
 * @dev_id:  id of the shared memory device.
 * @cb:      pointer to the async. I/O callbacks.
 *
 * Return:	None.
 **/
void os_c_action(int dev_id, os_aio_cb_t *cb)
{
	cab_dev_t *dev;
	
	/* Entry conditon. */
	OS_TRAP_IF(cb == NULL || cb->read_cb == NULL || cb->write_cb == NULL);

	/* Map the id to the device state. */
	dev = cab_dev_get(dev_id);

	/* Enter the critical section. */
	os_cs_enter(&dev->aio_mutex);

	/* Extended entry conditon. */
	OS_TRAP_IF(dev->aio_use ||
		   dev->pending_out || dev->sync_read || dev->sync_write);

	/* Copy the async. I/O operations. */
	dev->aio_cb = *cb;

	/* Activate the async. I/O transfer. */
	atomic_store(&dev->aio_use, 1);

	/* Leave the critical section. */
	os_cs_leave(&dev->aio_mutex);
}

/**
 * os_c_open() - the cable user shall call this function to request the resources
 * for the shared memory transer.
 *
 * @device_name:  name of the shared memory deivce: "/van_tcl" or "/tcl".
 * @mode:         0: blocking I/O, O_NBLOCK: non blocking I/O.
 * 
 * Return:	the device id.
 **/
int os_c_open(char *device_name, int mode)
{
	os_queue_elem_t msg;
	cab_conf_t *conf;
	cab_dev_t *dev;
	void *addr;
	int i;

	/* Entry condition. */
	OS_TRAP_IF(device_name == NULL);

	/* Search for the shm device configuration. */
	conf = cab_conf_get(cab_conf, device_name);

	/* Allocate the shm device data. */
	for (i = 0; i < CAB_COUNT; i++) {
		if (cab_device[i] == NULL)
			break;
	}
		
	/* Test the allocation conditon. */
	OS_TRAP_IF(i >= CAB_COUNT);

	dev = cab_device[i] = OS_MALLOC(sizeof(cab_dev_t));
	os_memset(dev, 0, sizeof(cab_dev_t));

	/* Save the device identificaton. */
	dev->id = conf->id;
	dev->name = conf->dev_name;

	/* Test the device mode. */
	if (mode == O_NBLOCK)
		dev->mode = O_NBLOCK;
	
	/* Get the reference to my named semaphore. */
	dev->my_int = sem_open(conf->my_int_n, O_CREAT);
	OS_TRAP_IF(dev->my_int == SEM_FAILED);
	
	/* Get the reference to named semaphore of the other device. */
	dev->other_int = sem_open(conf->other_int_n, O_CREAT);
	OS_TRAP_IF(dev->other_int == SEM_FAILED);

	/* Create the semaphore for os_c_write(). */
	os_sem_init(&dev->suspend_writer, 0);
	
	/* Create the semaphore for os_c_zread(). */
	os_sem_init(&dev->suspend_reader, 0);

	/* Create the mutex for the critical sections in write. */
	os_cs_init(&dev->write_mutex);
	
	/* Create the mutex for the critical sections in read. */
	os_cs_init(&dev->read_mutex);

	/* Create the mutex for the critical sections in aio_action and 
	 * aio_write. */	
	os_cs_init(&dev->aio_mutex);

	/* Create the mutex for the critical sections in cab_queue_add. */
	os_cs_init(&dev->q_mutex);

	/* Get the start addres of the shm area. */
	addr = (char *) cab_shell.start + conf->start_idx;

	/* Get the shared memory addresses for the cable. */
	if (conf->is_ctrl)
		cab_io_map(addr, &dev->in, &dev->out);
	else
		cab_io_map(addr, &dev->out, &dev->in);

	/* Install the py interrupt handler/thread. */
	dev->thread = os_thread_create(conf->thr_name, PRIO, Q_SIZE);
	
	/* Start with processing of the interrups. */
	os_memset(&msg, 0, sizeof(msg));
	msg.param = dev;
	msg.cb    = cab_int_exec;
	OS_SEND(dev->thread, &msg, sizeof(msg));

	return dev->id;
}

/**
 * os_cab_ripcord() - release critical device resources.
 *
 * @coverage:  if 0, release critical device resoures.
 *
 * Return:	None.
 **/
void os_cab_ripcord(int coverage)
{
	cab_shell_t *s;
	cab_dev_t *dev;
	char *name;
	int i;
	
	/* Entry conditon. */
	if (coverage)
		return;

	/* Loop thru the device list. */
	for (i = 0; i < CAB_COUNT; i++) {
		if (cab_device[i] == NULL)
			continue;

		/* Get the pointer to the device. */
		dev = cab_device[i];
	
		/* Release my named semaphore. */
		if (dev->my_int)
			sem_close(dev->my_int);

		/* Release the named semaphore of the other device. */
		if (dev->other_int)
			sem_close(dev->other_int);
	}

	/* Get the address of the shm shell. */
	s = &cab_shell;
	
	/* Test the file descriptor of the shared memory file. */
	if (s->fd == 0) {
		/* Remove the mapping of the shared memory area. */
		if (s->start != 0)
			munmap(s->start, s->size);

		/* Close the shared memory file. */
		close(s->fd);
	}
	
	/* Test the creator information. */
	if (! cab_shell.creator)
		return;
	
	/* Remove the shared memory file. */
	remove(CAB_SHM_FILE);

	/* Loop thru the named semaphore names. */
	for (i = 0; i < CAB_COUNT; i++) {
		name = cab_shell.sem_n[i];
		
		/* Delete the named semaphore. */
		sem_unlink(name);
	}
}

/**
 * os_cab_exit() - test the state of the shared memory devices.
 *
 * Return:	None.
 **/
void os_cab_exit(void)
{
	cab_shell_t *s;
	cab_wait_t *w;
	char *name;
	int i, rv;
	
	/* Test the state of all shared memory devices. */
	for (i = 0; i < CAB_COUNT; i++)
		OS_TRAP_IF(cab_device[i] != NULL);

	/* Get the pointer to the wait state. */
	w = &cab_wait;
	
	/* Release the resources for the wait condition. */
	for (i = 0; i < CAB_COUNT; i++) {
		/* Test the state of the wait condition. */
		OS_TRAP_IF(w->elem[i].assigned);
		os_sem_delete(&w->elem[i].suspend);
	}
	
	os_cs_destroy(&w->mutex);

	/* Get the address of the shm shell. */
	s = &cab_shell;
	
	/* Delete the mapping of the shared memory area. */
	rv = munmap(s->start, s->size);
	OS_TRAP_IF(rv != 0);
	
	/* Close the shared memory file. */
	rv = close(s->fd);
	OS_TRAP_IF(rv < 0);

	/* Test the creator information. */
	if (! cab_shell.creator)
		return;
	
	/* Remove the shared memory file. */
	rv = remove(CAB_SHM_FILE);
	OS_TRAP_IF(rv != 0);

	/* Loop thru the named semaphore names. */
	for (i = 0; i < CAB_COUNT; i++) {
		name = cab_shell.sem_n[i];
		
		/* Delete the named semaphore. */
		rv = sem_unlink(name);
		OS_TRAP_IF(rv != 0);
	}
}

/**
 * os_cab_init() - trigger the installation of the shared memory devices.
 *
 * @conf:     pointer to the trace configuration.
 * @creater:  if 1, create the shared memory objects.
 *
 * Return:	None.
 **/
void os_cab_init(os_conf_t *conf, int creator)
{
	struct cab_wait_elem_s *elem;
	cab_wait_t *w;
	int i;

	/* Save the reference to the OS configuration. */
	os_conf_p = conf;

	/* Save the creator information. */
	cab_shell.creator = creator;
	
	/* Create the named semaphores for interrupt simulation and the shared
	 * memory file. */
	if (creator)
		cab_create();

	/* Create the clean shared memory area. */
	cab_map();

	/* Get the pointer to the wait state. */
	w = &cab_wait;
	
	/* Creaate the resources for the wait condition. */
	os_cs_init(&w->mutex);
	
	for (i = 0, elem = w->elem; i < CAB_COUNT; i++, elem++) {
		elem->id = i;
		os_sem_init(&elem->suspend, 0);
	}
}
