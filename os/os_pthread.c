// SPDX-License-Identifier: GPL-2.0

/*
 * Pthread interfaces.
 *
 * Copyright (C) 2021 Gerald Schueller <gerald.schueller@web.de>
 */

/*============================================================================
  IMPORTED INCLUDE REFERENCES
  ============================================================================*/
#include <string.h>      /* String operations. */
#include <pthread.h>     /* POSIX thread. */
#include "os.h"          /* Operating system: os_sem_create() */

/*============================================================================
  EXPORTED INCLUDE REFERENCES
  ============================================================================*/
#include "os_private.h"  /* Local interfaces of the OS: os_trap_init() */

/*============================================================================
  LOCAL NAME CONSTANTS DEFINITIONS
  ============================================================================*/
/*============================================================================
  MACROS
  ============================================================================*/
/*============================================================================
  LOCAL TYPE DEFINITIONS
  ============================================================================*/
/**
 * os_queue_t - os_thread queue.
 *
 * @protect:     protect the access to the message queue.
 * @suspend_c:   client os_thread control semaphore.
 * @anchor:      first empty queue element.
 * @stopper:     last empty queue element.
 * @limit:       max. number of queue elements.
 * @count:       current number of queue elements.
 * @is_running:  the thread analyzes the message queue.
 * @busy_send:   if 1, a client executes os_queue_send.
 **/
typedef struct {
	spinlock_t       protect;
	sem_t            suspend_c;
	os_queue_elem_t  anchor;
	os_queue_elem_t  stopper;
	int              limit;
	int              count;
	volatile int     is_running;
	atomic_int       busy_send;
} os_queue_t;

/**
 * os_thread_state_t - states of an os_thread.
 *
 * @OS_THREAD_BOOT:     create the thread and the input queue.
 * @OS_THREAD_READY:    ready to process messages.
 * @OS_THREAD_KILL:     destory the thread and the input queue.
 * @OS_THREAD_INVALID:  undefined.
 **/
typedef enum {
	OS_THREAD_BOOT,    /* 0 */
	OS_THREAD_READY,   /* 1 */
	OS_THREAD_KILL,    /* 2 */
	OS_THREAD_INVALID  /* 3 */
} os_thread_state_t;

/**
 * os_thread_t - data for all thread interfaces. The caller must not modify any
 * data.
 *
 * @idx:        index of the thread list.
 * @name:       name of the thread.
 * @state:      current state of the pthread.
 * @prio:       thread priority.
 * @attr:       thread attribute like SCHED_RR.
 * @pthread:    pthread object.
 * @queue:      input queue of the os_thread.
 * @suspend_p:  parent control semaphore for create and destroy.
 **/
typedef struct {
	_Atomic os_thread_state_t  state;
	int               idx;
	char              name[OS_MAX_NAME_LEN + 1];
	os_thread_prio_t  prio;
	pthread_attr_t    attr;
	pthread_t         pthread;
	os_queue_t        queue;
	sem_t             suspend_p;
} os_thread_t;

/**
 * os_thread_elem_t - entry of the thread table.
 *
 * @thread:     os_thread definition.
 * @idx:        index of the list element.
 * @is_in_use:  1, if the table entry is used.
 **/
typedef struct {
	os_thread_t  thread;
	int          idx;
	int          is_in_use;
} os_thread_elem_t;

/*============================================================================
  LOCAL DATA
  ============================================================================*/

/* Pointer to the OS configuration */
static os_conf_t *os_conf_p;

/**
 * os_thread_list_s - list of the installed thread.
 *
 * @list:     list of the installed thread.
 * @count:    number of the installed threads.
 * @protect:  protect the access to the thread list.
 * @key:      data key visible to all threads.
 * @once_c:   
 **/
static struct os_thread_list_s {
	os_thread_elem_t  elem[OS_THREAD_LIMIT];
	int               count;
	pthread_mutex_t   protect;
	pthread_key_t     key;
	pthread_once_t    once_c;

} os_thread_list;

/*============================================================================
  LOCAL FUNCTION PROTOTYPES
  ============================================================================*/
/*============================================================================
  LOCAL FUNCTIONS
  ============================================================================*/

/**
 * os_queue_loop() - process the received messages.
 *
 * @thread:  reference to os_thread.
 * @count:   fill level of the message queue.
 *
 * Return:	0, if the thread state != OS_THREAD_READY.
 **/
static int os_queue_loop(os_thread_t *thread, int count)
{
	os_thread_state_t state;
	os_queue_elem_t *elem;
	os_queue_t *q;
	
	/* Get the reference to the thread input queue. */
	q = &thread->queue;

	/* Loop over the message queue. */
	while (count > 0) {
		/* Enter the critical section. */
		os_spin_lock(&q->protect);
			
		/* Decrement the element counter. */
		q->count--;

		/* Get the first queue element. */
		elem = q->anchor.next;

		/* Calculate the new queue start. */
		q->anchor.next = elem->next;

		/* Test the fill level of the input queue. */
		if (q->anchor.next == &q->stopper) {
			/* The input queue is empty. */
			q->stopper.next = &q->anchor;
		}

		/* Leave the critical section. */
		os_spin_unlock(&q->protect);

		/* Process the current message. */
		elem->cb(elem);

		/* Free the message buffer. */
		OS_FREE(elem);

		/* Enter the critical section. */
		os_spin_lock(&q->protect);

		/* Copy the filling level of the queue. */
		count = q->count;

		/* Leave the critical section. */
		os_spin_unlock(&q->protect);

		/* Test the thread state. */
		state = atomic_load(&thread->state);
		if (state != OS_THREAD_READY)
			return 0;
	}

	return 1;
}

/**
 * os_thread_suspend() - suspend the thread, if the message queue is empty.
 *
 * @thread:  reference to os_thread.
 * @count:   pointer to the fill level of the input queue.
 *
 * Return:	0, if the thread state != OS_THREAD_READY.
 **/
static int os_thread_suspend(os_thread_t *thread, int *count_p)
{
	os_thread_state_t state;
	os_queue_t *q;
	int count;

	/* Initialize the return value. */
	*count_p = 0;

	/* Get the reference to the thread input queue. */
	q = &thread->queue;

	/* Enter the critical section. */
	os_spin_lock(&q->protect);

	/* If the input queue is empty, suspend the thread. */
	q->is_running = 0;

	/* Copy the filling level of the queue. */
	count = q->count;

	/* Leave the critical section. */
	os_spin_unlock(&q->protect);

	/* If the input queue is empty, suspend the thread. */
	if (count < 1) {
		/* Test the thread state. */
		state = atomic_load(&thread->state);
		if (state != OS_THREAD_READY)
			return 0;

		/* Print the suspend information. */
		OS_TRACE(("%s [t=%s,s=%d,o=suspend]\n", OS, thread->name, state));

		/* Suspend the thread. */
		os_sem_wait (&q->suspend_c);

		/* Print the resume information. */
		OS_TRACE(("%s [t=%s,s=%d,o=resume]\n", OS, thread->name, state));
	}

	/* Enter the critical section. */
	os_spin_lock(&q->protect);

	/* Change the thread state to running. */
	q->is_running = 1;

	/* Copy the filling level of the queue. */
	count = q->count;

	/* Leave the critical section. */
	os_spin_unlock(&q->protect);

	/* Update the fill level of the queue. */
	*count_p = count;

	/* Test the thread state. */
	state = atomic_load(&thread->state);
	if (state != OS_THREAD_READY)
		return 0;
		
	return 1;
}

/**
 * os_pthread_once_init() - init_routine for pthread_once.
 *
 * Return:	None.
 **/
static void os_pthread_once_init(void)
{
	int ret;
	
	/* Create a data key visible to all theads in the process. */
	ret = pthread_key_create(&os_thread_list.key, NULL);
	OS_TRAP_IF(ret != 0);
}

/**
 * os_thread_save() - save the thread pointer.
 *
 * @thread:  reference to os_thread.
 *
 * Return:	None.
 **/
static void os_thread_save(os_thread_t *thread)
{
	pthread_key_t key;
	int ret;
	
	/* Initialze the os_thread_list.key once. */
	ret = pthread_once(&os_thread_list.once_c, os_pthread_once_init);
	OS_TRAP_IF(ret != 0);

	/* Copy the key. */
	key = os_thread_list.key;

	/* Get the reference to the os_thread. */
	if (pthread_getspecific(key) == NULL)
	{
		/* Associate the os_thread with key. */
		ret = pthread_setspecific(key, thread);
		OS_TRAP_IF(ret != 0);
	}
}

/**
 * os_thread_cb() - callback for the thread context.
 *
 * @arg:  generic reference to os_thread.
 *
 * Return:	NULL.
 **/
static void *os_thread_cb(void *arg)
{
	os_thread_t *thread;
	int count;

	/* Entry condition. */
	OS_TRAP_IF(arg == NULL);
	
	/* Decode the reference to the os_thread. */
	thread = arg;
	
	/* Save the thread pointer. */
	os_thread_save(thread);

	/* Change the os_thread state. */
	OS_TRACE(("%s [t=%s,s=ready,o=create]\n", OS, thread->name));
	atomic_store(&thread->state, OS_THREAD_READY);

	/* Resume the caller of os_thread_create. */
	os_sem_release(&thread->suspend_p);

	/* Loop thru the thread callback. */
	for (;;)  {
		/* Suspend the thread, if the message queue is empty. */
		count = 0;
		if (! os_thread_suspend(thread, &count))
			break;

		/* Loop over the message queue. */
		if (! os_queue_loop(thread, count))
			break;
	}
	
	/* Release the parent thread in os_thread_destroy. */
	os_sem_release(&thread->suspend_p);
	
	return NULL;
}

/**
 * os_queue_free() - release the queue resources.
 *
 * @q:  pointer to the OS thread queue.
 *
 * Return:	None.
 **/
static void os_queue_free(os_queue_t *q)
{
	os_queue_elem_t *elem, *next;
	int i;

	/* Get the first queue element. */
	elem = q->anchor.next;

	/* Free pending messages. */
	for (i = 0; elem != &q->stopper; i++) {
		/* Save the reference to the successor. */
		next = elem->next;
		
		/* Free the message buffer. */
		OS_FREE(elem);

		/* Continue with the next element. */
		elem = next;
	}

	/* Test the queue state. */
	OS_TRAP_IF(i != q->count);

	/* Release the queue OS resources. */
	os_spin_destroy(&q->protect);
	os_sem_delete(&q->suspend_c);
}

/**
 * os_thread_free() - release the queue and thread resources.
 *
 * @thread:  pointer to the os_thread.
 *
 * Return:	None.
 **/
static void os_thread_free(os_thread_t *thread)
{
	struct os_thread_list_s *list;
	os_thread_elem_t *elem;
	int ret;
	
	/* Release the queue resources. */
	os_queue_free(&thread->queue);
	
	/* Free the thread resources. */
	ret = pthread_attr_destroy(&thread->attr);
	OS_TRAP_IF(ret != 0);
	
	os_sem_delete(&thread->suspend_p);

	/* Release the thread element. */
	list = &os_thread_list;

	/* Get the address of the thread list element. */
	elem = &list->elem[thread->idx];

	/* Enter the critical section. */
	os_cs_enter(&list->protect);

	/* Test the thread state. */
	OS_TRAP_IF(! elem->is_in_use);

	/* Reset the thread index. */
	thread->idx = -1;
	
	/* Update the list state. */
	elem->is_in_use = 0;
	list->count--;

	/* Leave the critical section. */
	os_cs_leave(&list->protect);
}

/**
 * os_queue_init() - initialize the input queue of the thread.
 *
 * @thread:  pointer to the os_thread.
 * @q_size:  max. number of the input queue messages.
 *
 * Return:	None.
 **/
static void os_queue_init(os_thread_t *thread, int q_size)
{
	os_queue_t  *q;

	/* Entry condition. */
	OS_TRAP_IF(q_size >= OS_QUEUE_LIMIT);
	
	/* Get the reference to the  os_queue. */
	q = &thread->queue;

	/* Initialize the spinlock. */
	os_spin_init(&q->protect);

	/* Create the thread control semaphore. */
	os_sem_init(&q->suspend_c, 0);

	/* Initialize the input queue. */
	q->anchor.next  = &q->stopper;
	q->stopper.next = &q->anchor;

	/* Initialize the queue element counter. */
	q->limit = q_size;
	q->count = 0;

	/* Reset the queue state. */
	q->is_running = 0;
	q->busy_send  = 0;
}

/**
 * os_thread_alloc() - allocate a os_thread.
 *
 * @q_size:  max. number of the input queue messages.
 *
 * Return:	the pointer to the allocated thread.
 **/
static os_thread_t *os_thread_alloc(int q_size)
{
	struct os_thread_list_s *list;
	os_thread_elem_t *elem;
	int i;

	/* Get the address of the thread list. */
	list = &os_thread_list;
	
	/* Get the address of the first list element. */
	elem = list->elem;

	/* Enter the critical section. */
	os_cs_enter(&list->protect);
	
	/* Search for a free table entry. */
	for (i = 0; i < OS_THREAD_LIMIT && elem->is_in_use; i++, elem++)
		;

	/* Test the table state. */
	OS_TRAP_IF(i >= OS_THREAD_LIMIT);

	/* Reset the thread state. */
	os_memset(elem, 0, sizeof(os_thread_elem_t));
	
	/* Allocate a thread list element. */
	elem->idx       = i;
	elem->is_in_use = 1;
	
	/* Save the index. */
	elem->thread.idx = i;

	list->count++;

	/* Leave the critical section. */
	os_cs_leave(&list->protect);

	/* Initialize the input queue of the thread. */
	os_queue_init(&elem->thread, q_size);
	
	return &elem->thread;
}

/**
 * os_thread_prio() - calculate the thread priority.
 *
 * @prio:  thread priority.
 *
 * Return:	the updated thread priority.
 **/
static os_thread_prio_t os_thread_prio(os_thread_prio_t prio)
{
	/* Test the thread priority. */
	switch(prio) {
	case OS_THREAD_PRIO_HARDRT:
	case OS_THREAD_PRIO_SOFTRT:
	case OS_THREAD_PRIO_BACKG:
	case OS_THREAD_PRIO_FOREG:
		return prio;
	default:
		return OS_THREAD_PRIO_DEFAULT;
	}
}

/*============================================================================
  EXPORTED FUNCTIONS
  ============================================================================*/
/**
 * os_thread_create() - create a thread and the input queue.
 *
 * @thread:  address of the pthread data.
 * @name:    name of the thread.
 * @prio:    thread priority.
 * @q_size:  max. number of the input queue messages.
 *
 * Return:     the generic pointer to the os_thread.
 **/
void *os_thread_create(const char *name, os_thread_prio_t prio, int q_size)
{
	struct sched_param p;
	os_thread_t  *thread;
	int len, ret, c_type, orig_c;

	/* Entry condition. */
	OS_TRAP_IF(name == NULL);

	/* Allocate the os_thread. */
	thread = os_thread_alloc(q_size);
	
	/* Save the thread name. */
	len = os_strlen(name);
	OS_TRAP_IF(len >= OS_MAX_NAME_LEN);
	os_memset(thread->name, 0, OS_MAX_NAME_LEN);
	os_strcpy(thread->name, OS_MAX_NAME_LEN, name);

	/* Initialize the thread state. */
	OS_TRACE(("%s [t=%s,s=boot,o=create]\n", OS, thread->name));
	atomic_store(&thread->state, OS_THREAD_BOOT);
	
	/* Calculate the thread priority. */
	prio = os_thread_prio(prio);
	
	/* Initialize the thread attributes with default values. */
	ret = pthread_attr_init(&thread->attr);
	OS_TRAP_IF(ret != 0);

	/* Set the scheduling policy attribute. */
	ret = pthread_attr_setschedpolicy(&thread->attr, SCHED_RR);
	OS_TRAP_IF(ret != 0);

	/* Define the thread priority. */
	p.sched_priority = prio;
	ret = pthread_attr_setschedparam(&thread->attr, &p);
	OS_TRAP_IF(ret != 0);

	/* Save the thread prioriiy. */
	thread->prio  = prio;

	/* Create the control semaphore for os_thread_delete. */
	os_sem_init(&thread->suspend_p, 0);
	
	/* Set the cancelability type. */
	c_type = PTHREAD_CANCEL_ENABLE;
	ret = pthread_setcancelstate(c_type, &orig_c);
	OS_TRAP_IF(ret != 0);

	/* Create the pthread. */
	ret = pthread_create(&thread->pthread, NULL, os_thread_cb, thread);
	OS_TRAP_IF(ret != 0);

	/* Wait for the start of the client. os_thread. */
	os_sem_wait(&thread->suspend_p);
	
	return thread;
}

/**
 * os_queue_send() - save the message in the os_thread queue.
 *
 * @g_thread:  generic address of the os_thread.
 * @msg:       reference to the message.
 * size:       size of the message.
 *
 * Return:	None.
 **/
void os_queue_send(void *g_thread, os_queue_elem_t *msg, int size)
{
	os_thread_t        *thread;
	os_thread_state_t   state;
	os_queue_elem_t    *elem, *stopper;
	os_queue_t         *q;
	int                 is_running;
	
	/* Entry condition. */
	OS_TRAP_IF(g_thread == NULL || msg == NULL ||
		   size < sizeof(os_queue_elem_t) || msg->cb == NULL);

	/* Decode the reference to the os_thread. */
	thread = g_thread;

	/* Get the reference to the message queue. . */
	q = &thread->queue;

	/* Change the state of this operation. */
	atomic_store(&q->busy_send, 1);
	
	/* Test the thread state. */
	state = atomic_load(&thread->state);
	OS_TRAP_IF(state != OS_THREAD_READY);
	
	/* Request memory for the os_message. */
	elem = OS_MALLOC(size);

	/* Copy the user message. */
	os_memcpy (elem, size, msg, size);

	/* Get the reference to the last queue element. */
	stopper = &q->stopper;
	
	/* Enter the critical section. */
	os_spin_lock(&q->protect);
	
	/* Increment the number of the queue elements. */
	q->count++;

	/* Test the state of the message queueu. */
	if (q->count > q->limit) {
		OS_TRACE(("> %s: \"%s\": count=%d, limit=%d", F, thread->name,
			  q->count, q->limit));
		OS_TRAP();
	}

	/* Insert the new message at the end of the queue. */
	stopper->next->next = elem;
	elem->next = stopper;
	stopper->next = elem;

	/* Change the thread state. */
	is_running = q->is_running;
	q->is_running = 1;

	/* Leave the critical section. */
	os_spin_unlock (&q->protect);

	/* Test the thread state. */
	if (! is_running) {
		/* Resume the os_thread. */
		os_sem_release (&q->suspend_c);
	}
	
	/* Change the state of this operation. */
	atomic_store(&q->busy_send, 0);	
}

/**
 * os_thread_destroy() - delete a thread and its input queue. Be carefull when
 * deleting a thread, as it may hold some resources which has not been released.
 *
 * @g_thread:  generic address of the os_thread.
 *
 * Return:	None.
 **/
void os_thread_destroy(void *g_thread)
{
	os_thread_state_t state;
	os_thread_t  *thread, *current;
	os_queue_t   *q;
	int busy_send, ret;
	void *status;
	
	/* Entry condition. */
	OS_TRAP_IF(g_thread == NULL);

	/* Decode the reference to the thread state. */
	thread = g_thread;
	
	/* Test the list index. */
	OS_TRAP_IF(thread->idx < 0 || thread->idx >= OS_THREAD_LIMIT);

	/* The current thread may not delete itself. */
	current = pthread_getspecific(os_thread_list.key);
	OS_TRAP_IF(thread == current);

	/* Get, modify and test the thread state. */
	OS_TRACE(("%s [t=%s,s=kill,o=destroy]\n", OS, thread->name));
	state = atomic_load(&thread->state);
	atomic_store(&thread->state, OS_THREAD_KILL);
	OS_TRAP_IF(state != OS_THREAD_READY);	

	/* Get the reference to the message queue. . */
	q = &thread->queue;
	
	/* Fatal interworking error because of busy os_queue_send. */
	busy_send = atomic_load(&q->busy_send);
	OS_TRAP_IF(busy_send != 0);

	/* Resume the os_thread in os_thread_cb. */
	os_sem_release (&q->suspend_c);

	/* Wait for the leave of the thread callback. */
	os_sem_wait(&thread->suspend_p);

	/* Join with the terminated thread. */
	ret = pthread_join(thread->pthread, &status);
	OS_TRAP_IF(ret != 0);

	/* Release the os_thread and queue resources. */
	os_thread_free(thread);
}

/**
 * os_thread_name() - return the pointer to the thread name string.
 *
 * @g_thread:  generic address of the os_thread.
 *
 * Return:	start address of the thread name.
 **/
char *os_thread_name(void *g_thread)
{
	os_thread_t  *thread;
	
	/* Entry condition. */
	OS_TRAP_IF(g_thread == NULL);

	/* Decode the reference to the thread state. */
	thread = g_thread;
	
	/* Test the list index. */
	OS_TRAP_IF(thread->idx < 0 || thread->idx >= OS_THREAD_LIMIT);

	return thread->name;
}

/**
 * os_thread_statistics() - provide data on the thread state.
 *
 * @stat:  address of the status information.
 *
 * Return:	None.
 **/
void os_thread_statistics(os_statistics_t *stat)
{
	struct os_thread_list_s *list;

	/* Get the address of the thread list. */
	list = &os_thread_list;

	/* Enter the critical section. */
	os_cs_enter(&list->protect);

	/* Provide the number of the installed threads. */
	stat->thread_c = list->count;
	
	/* Leave the critical section. */
	os_cs_leave(&list->protect);
}

/**
 * os_thread_init() - initialize the thread list.
 *
 * conf:  OS configuration.
 *
 * Return:	None.
 **/
void os_thread_init(os_conf_t *conf)
{
	/* Save the reference to the OS configuration. */
	os_conf_p = conf;
	
	/* Create the mutex for the critical section. */
	os_cs_init(&os_thread_list.protect);

	/* Initialize the control for pthread_once. */
	os_thread_list.once_c = PTHREAD_ONCE_INIT;
}

/**
 * os_thread_exit() - release the thread list resources.
 *
 * Return:	None.
 **/
void os_thread_exit(void)
{
	struct os_thread_list_s *list;

	/* Release the thread list resources. */
	list = &os_thread_list;

	/* Enter the critical section. */
	os_cs_enter(&list->protect);

	OS_TRAP_IF(list->count != 0);

	/* Leave the critical section. */
	os_cs_leave(&list->protect);

	/* Create the mutex for the critical section. */
	os_cs_destroy(&list->protect);
}
