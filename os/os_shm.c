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
#define OS_VAN_NAME  "/van"      /* Name of the van shared memory device. */
#define OS_PY_NAME   "/python"   /* Name of the python shared memory device. */
/* XXX */
#if 0
#define OS_SHM_FILE  "shm.txt"   /* Name of the shared memory file. */
#define OS_SHM_FILE  "/home/gerald/van_development/van/os/shm.txt"
#else
#define OS_SHM_FILE  "/home/gerald/github/van/os/shm.txt"
#endif
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
/* XXX */
#if 0
/**
 * os_shm_action_t - is used for asynchronous data transfer. These functions are performed
 * in the py_int or van_int thread context. During the execution of the callback
 * async_write_cb you may send data again. The received data are released after calling up
 * async_read_cb().
 *
 * @async_read_cb:   is invoked, whenever data are avaialble.
 * @async_write_cb:  is invoked, whenever an internal write buffer is available again.
 **/
typdef struct {
	os_async_read_t   async_read_cb;
	os_async_write_t  async_write_cb;
} os_shm_action_t;
#endif

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
	os_shm_msg_t     ring[OS_SHM_Q_SIZE];
	int  tail;
	int  head;
} os_shm_queue_t;

/** 
 * os_shm_top_t - shm topology.
 *
 * @mutex:     critical section in os_queue_add.
 * @dl_queue:  python shm queue: van -> py.
 * @dl_count:  size of the DL payload: van->py.
 * @dl_size:   size of the DL buffer: van->py.
 * @dl_start:  start address of the DL buffer.
 * @dl_end:    end address of the DL buffer.
 * @ul_queue:  van shm queue: py -> van.
 * @ul_count   size of the UL payload: py->van.
 * @ul_size:   size of the UL buffer: py->van.
 * @ul_start:  start address of the UL buffer.
 * @ul_end:    end address of the UL buffer.
 **/
typedef struct {
	pthread_mutex_t  q_mutex;
	os_shm_queue_t  *dl_queue;
	atomic_int       dl_count;
	int    dl_size;
	char  *dl_start;
	char  *dl_end;
	os_shm_queue_t  *ul_queue;
	atomic_int       ul_count;
	int    ul_size;
	char  *ul_start;
	char  *ul_end;
} os_shm_top_t;

/**
 * os_dev_t - shared memory device state.
 *
 * @id:                van or py device.
 * @name:              name of the shared memory device.
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
 * @suspend_writer:    suspend the write caller in sync_write.
 * @suspend_reader:    suspend the read caller in sync_read.
 * @pending_ul:        if 1, pending UL transfer buffer.
 * @pending_dl:        if 1, pending DL transfer buffer.
 * @down:              if 1, ignore the py/van interrupt.
 * @sync_write:        if 1, the user has invoked sync_write.
 * @sync_read:         if 1, the user has invoked sync_read.
 * @write_mutex:       protect the critical sections in sync_write.
 * @read_mutex:        protect the critical sections in sync_read.
 * @top:               topology of the shm area.
 **/
typedef struct {
	os_dev_type_t  id;
	char   *name;
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
	sem_t   suspend_writer;
	sem_t   suspend_reader;
	atomic_int       pending_ul;
	atomic_int       pending_dl;
	atomic_int       down;
	atomic_int       sync_read;
	atomic_int       sync_write;
	pthread_mutex_t  write_mutex;
	pthread_mutex_t  read_mutex;
	os_shm_top_t     top;
} os_dev_t;

/*============================================================================
  LOCAL DATA
  ============================================================================*/
/* Pointer to the OS configuration */
static os_conf_t *os_conf_p;

/* List of the shared memory devices. */
static os_dev_t os_device[OS_DEV_COUNT];

/*============================================================================
  LOCAL FUNCTION PROTOTYPES
  ============================================================================*/
/*============================================================================
  LOCAL FUNCTIONS
  ============================================================================*/
/**
 * os_ulq_add() - send the UL shm message from py to van.
 *
 * dev:       pointer to the shm device.
 * count:     size of the UL payload.
 * consumed:  if 1, the DL buffer has been released.
 *
 * Return:	None.
 **/
static void os_ulq_add(os_dev_t *dev, int count, int consumed)
{
	os_shm_queue_t *q;
	os_shm_msg_t *shm_m;
	int head;
	
	/* Enter the critical section. */
	os_cs_enter(&dev->top.q_mutex);

	/* Get the pointer of the py shm input queue. */
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
 * os_dlq_add() - send the DL shm message from van to py.
 *
 * dev:       pointer to the shm device.
 * count:     size of the DL payload.
 * consumed:  if 1, the UL buffer has been released.
 *
 * Return:	None.
 **/
static void os_dlq_add(os_dev_t *dev, int count, int consumed)
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
 * os_py_exit_exec() - the py_int thread shall not be suspended with named
 * py_int semphore.
 *
 * @msg:  generic input message.
 *
 * Return:	None.
 **/
static void os_py_exit_exec(os_queue_elem_t *msg)
{
	OS_TRACE(("%s [s:ready, m:py-exit] -> [s:ready]\n", PP));
}

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
	os_shm_queue_t *q;
	os_shm_msg_t *shm_m;
	os_shm_top_t *t;
	os_dev_t *p;
	int down, sync_read;

	/* Get the address of the py shared memory state. */
	p = &os_device[OS_DEV_PY];

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
				/* Save the number of the received bytes. */
				atomic_store(&t->dl_count, shm_m->size);
			
				/* XXX */
#if 0
				p->async_read_cb(t->dl_start, shm_m->size);
#endif
				/* Test the read method. */
				sync_read = atomic_exchange(&p->sync_read, 0);

				OS_TRACE(("%s dl_count=%d, sync_read=%d\n", PP, t->dl_count, sync_read));

				if (sync_read)
					os_sem_release(&p->suspend_reader);

			}

			/* Test the state of sent UL playload to van. */
			if (shm_m->consumed) {
				/* XXX */
#if 0
				p->async_write_cb();
#endif
				OS_TRACE(("%s pending:%d, consumed:%d\n", PP,
					  shm_m->consumed, p->pending_ul));
				atomic_store(&p->pending_ul, 0);
			}
		
			/* Increment and test the start of the message list. */
			q->tail++;
			if (q->tail >= OS_SHM_Q_SIZE)
				q->tail = 0;
		}
	}
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
	OS_TRACE(("%s [s:ready, m:van-exit] -> [s:ready]\n", VP));
}

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
	os_shm_queue_t *q;
	os_shm_msg_t *shm_m;
	os_dev_t *v;
	int down, sync_write;

	/* Get the address of the van shared memory state. */
	v = &os_device[OS_DEV_VAN];

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
		q = v->top.ul_queue;

		/* Analyze the van shm input queue. */
		while (q->tail != q->head) {
			shm_m = &q->ring[q->tail];

			/* Test the UL payload state from py. */
			if (shm_m->size > 0) {
				/* XXX */
			}

			/* Test the state of sent DL playload from van to py. */
			if (shm_m->consumed) {
				atomic_store(&v->pending_dl, 0);

				/* Test the write method. */
				sync_write = atomic_exchange(&v->sync_write, 0);
				if (sync_write)
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
 * os_shm_close() - delete the access and the mapping to the shared memory.
 *
 * @s:  pointer to the shared memory state.
 *
 * Return:	None.
 **/
static void os_shm_close(os_dev_t *s)
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

	/* Destroy the mutex for the critical sections in os_queue_add. */
	os_cs_destroy(&s->top.q_mutex);
	
	/* Delete the mutex for the critical sections in os_sync_write. */
	os_cs_destroy(&s->write_mutex);

	/* Delete the mutex for the critical sections in os_sync_pread. */
	os_cs_destroy(&s->read_mutex);

	/* Destroy the semaphore for os_sync_write(). */
	os_sem_delete(&s->suspend_writer);
	
	/* Destroy the semaphore for os_sync_pread(). */
	os_sem_delete(&s->suspend_reader);
}

/**
 * os_shm_open() - map the file into shared memory.
 *
 * @s:  pointer to the shared memory state.
 *
 * Return:	None.
 **/
static void os_shm_open(os_dev_t *s)
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

	/* Create the mutex for the critical sections in os_sync_write. */
	os_cs_init(&s->write_mutex);
	
	/* Create the mutex for the critical sections in os_sync_pread. */
	os_cs_init(&s->read_mutex);
	
	/* Get the reference to the shm topology. */
	t = &s->top;

	/* Copy the pointer to the start of the shm. */
	p = s->start;

	/* Map the data transfer queues to the shm. */
	t->dl_queue = (os_shm_queue_t *) p;
	p = p + sizeof(os_shm_queue_t);
	
	t->ul_queue = (os_shm_queue_t *) p;
	p = p + sizeof(os_shm_queue_t);
	
	/* Map the UL buffer to the shm. */
	t->ul_size  = OS_BUF_SIZE;
	t->ul_start = (char *) p;
	t->ul_end   = (char *) p + OS_BUF_SIZE - 1;
	p = (char *) p + OS_BUF_SIZE;
	
	/* Map the DL buffer to the shm. */
	t->dl_size  = OS_BUF_SIZE;
	t->dl_start = (char *) p;
	t->dl_end   = (char *) p + OS_BUF_SIZE - 1;

	/* Create the mutex for the critical sections in os_queue_add. */
	os_cs_init(&t->q_mutex);
	
	/* Create the semaphore for os_sync_write(). */
	os_sem_init(&s->suspend_writer, 0);
	
	/* Create the semaphore for os_sync_pread(). */
	os_sem_init(&s->suspend_reader, 0);
}

/**
 * os_py_close() - the python process shall call this function to remove the
 * shared memory ressources.
 *
 * @p:  pointer to the py device.
 *
 * Return:	None.
 **/
static void os_py_close(os_dev_t *p)
{
	os_queue_elem_t msg;
	
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
	os_shm_close(p);	
}

/**
 * os_py_open() - the python process shall call this function to create the
 * shared memory device.
 *
 * @p:  pointer to the py device.
 *
 * Return:	None.
 **/
static void os_py_open(os_dev_t *p)
{
	os_queue_elem_t msg;
	
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
	msg.cb    = os_py_int_exec;
	OS_SEND(p->thread, &msg, sizeof(msg));
}

/**
 * os_van_close() - the van process shall call this function to remove the
 * shared memory ressources.
 *
 * @v:  pointer to the van device.
 *
 * Return:	None.
 **/
static void os_van_close(os_dev_t *v)
{
	os_queue_elem_t msg;
	int rv;

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
 * os_van_open() - the van process shall call this function to create the shared
 * memory device.
 *
 * @v:  pointer to the van device.
 *
 * Return:	None.
 **/
static void os_van_open(os_dev_t *v)
{
	os_queue_elem_t msg;
	mode_t mode;
	int oflag;

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

	/* Start with processing of the van_int. */
	os_memset(&msg, 0, sizeof(msg));
	msg.param = v->thread;
	msg.cb    = os_van_int_exec;
	OS_SEND(v->thread, &msg, sizeof(msg));
}

/*============================================================================
  EXPORTED FUNCTIONS
  ============================================================================*/
/**
 * os_open() - the van or py process shall call this function to request the
 * resources for the shared memory transer.
 *
 * @device_name:  name of the shared memory deivce: "/van" or "/python".
 *
 * Return:	the device id.
 **/
int os_open(char *device_name)
{
	os_dev_t *dev;
	int i;
	
	/* Entry condition. */
	OS_TRAP_IF(device_name == NULL);

	/* Search for the shared memory device. */
	for (dev = os_device, i = OS_DEV_VAN; i < OS_DEV_COUNT; i++, dev++) {
		if (os_strcmp(device_name, dev->name) == 0)
			break;
	}

	OS_TRAP_IF(i >= OS_DEV_COUNT);
	
	/* Test the device id. */
	switch(dev->id) {
	case OS_DEV_VAN:
		os_van_open(&os_device[OS_DEV_VAN]);
		break;
	case OS_DEV_PY:
		os_py_open(&os_device[OS_DEV_PY]);
		break;
	default:
		break;
	}

	return dev->id;
}

/**
 * os_close() - the van or py process shall call this function to remove the
 * shared memory ressources.
 *
 * @dev_id:  van or py device id.
 *
 * Return:	None.
 **/
void os_close(int dev_id)
{
	/* Entry condition. */
	OS_TRAP_IF(dev_id < OS_DEV_VAN || dev_id >= OS_DEV_COUNT);
	
	/* Test the device id. */
	switch(dev_id) {
	case OS_DEV_VAN:
		os_van_close(&os_device[OS_DEV_VAN]);
		break;
	case OS_DEV_PY:
		os_py_close(&os_device[OS_DEV_PY]);
		break;
	default:
		break;
	}
}

/**
 * os_sync_write() - send the DL payload to py or the UL payload to van and
 * suspend the caller until the payload has been processed.
 *
 * @dev_id:  id of the shared memory device.
 * @buf:     pointer to the payload.
 * @count:   size of the payload.
 *
 * Return:	None.
 **/
void os_sync_write(int dev_id, char *buf, int count)
{
	os_shm_top_t *top;
	os_dev_t *v;

	/* Entry condition. */
	OS_TRAP_IF(dev_id != OS_DEV_VAN || buf == NULL || count < 1);

	/* Get the address of the van shared memory state. */
	v = &os_device[OS_DEV_VAN];

	/* Entry condition. */
	OS_TRAP_IF(! v->init);

	/* Get the pointer to the shm topology and to the py input queue. */
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
	os_dlq_add(v, count, 0);

	/* Suspend the caller until the action is executed. */
	os_sem_wait(&v->suspend_writer);

	/* Leave the critical section. */
	os_cs_leave(&v->write_mutex);	
}

/**
 * os_sync_pread() - py or van waits for incoming payload. With each successiv call, the reference to the
 * previous call is released automatically.
 *
 * @dev_id:  id of the shared memory device.
 * @buf:     pointer to the received payload.
 * @count:   size of the destination buffer.
 *
 * Return:	number of the received bytes.
 **/
int os_sync_pread(int dev_id, char **buf, int count)
{
	os_shm_top_t *top;
	os_dev_t *p;
	int pending_dl, n;

	/* Entry condition. */	
	OS_TRAP_IF(dev_id != OS_DEV_PY);

	/* Get the address of the py shared memory state. */
	p = &os_device[OS_DEV_PY];

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
		os_ulq_add(p, 0, 1);
	
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
 * os_sync_read() - py or van waits for incoming payload.
 *
 * @dev_id:  id of the shared memory device.
 * @buf:     pointer to the received payload.
 * @count:   size of the destination buffer.
 *
 * Return:	number of the received bytes.
 **/
int os_sync_read(int dev_id, char *buf, int count)
{
	os_shm_top_t *top;
	os_dev_t *p;
	int n;

	/* Entry condition. */	
	OS_TRAP_IF(dev_id != OS_DEV_PY);

	/* Get the address of the py shared memory state. */
	p = &os_device[OS_DEV_PY];

	/* Entry condition. */
	OS_TRAP_IF(! p->init || buf == NULL || count < 1);

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

		/* Test the number of the received bytest. */
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
	os_ulq_add(p, 0, 1);
	
	/* Leave the critical section. */
	os_cs_leave(&p->read_mutex);	

	return n;
}

/**
 * os_shm_init() - define the device id and name.
 *
 * Return:	None.
 **/
void os_shm_init(os_conf_t *conf)
{
	/* Save the reference to the OS configuration. */
	os_conf_p = conf;
	
	/* Save the device id and name. */
	os_device[OS_DEV_VAN].id   = OS_DEV_VAN;
	os_device[OS_DEV_VAN].name = OS_VAN_NAME;
	
	os_device[OS_DEV_PY].id    = OS_DEV_PY;
	os_device[OS_DEV_PY].name  = OS_PY_NAME;
}

/**
 * os_shm_exit() - test the state of the shared memory devices.
 *
 * Return:	None.
 **/
void os_shm_exit(void)
{
	os_dev_t *dev;
	int i;
	
	/* Test the state of the shared memory devices. */	
	for (dev = os_device, i = OS_DEV_VAN; i < OS_DEV_COUNT; i++, dev++) {
		OS_TRAP_IF(dev->init);
	}

}
