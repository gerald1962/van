// SPDX-License-Identifier: GPL-2.0

/*
 * Pthread interfaces.
 *
 * Copyright (C) 2021 Gerald Schueller <gerald.schueller@web.de>
 */

/*============================================================================
  IMPORTED INCLUDE REFERENCES
  ============================================================================*/
#include <string.h>  /* String operations. */
#include <pthread.h> /* POSIX thread. */
#include "os.h"      /* Operating system: os_sem_create() */

/*============================================================================
  EXPORTED INCLUDE REFERENCES
  ============================================================================*/
/*============================================================================
  LOCAL NAME CONSTANTS DEFINITIONS
  ============================================================================*/
/*============================================================================
  MACROS
  ============================================================================*/
/*============================================================================
  LOCAL TYPE DEFINITIONS
  ============================================================================*/
/* Forward declaration of the generic message. */
typedef struct os_queue_elem_s os_queue_elem_t;

/* Callback for the received message. */
typedef void os_queue_cb_t(os_queue_elem_t *msg);

/**
 * OS_QUEUE_MSG_HEAD - generic message header.
 *
 * @next:   pointer to the next queue element.
 * @param:  generic message parameter.
 * @cb:     callback for the message processing.
 **/
#define OS_QUEUE_MSG_HEAD \
    struct os_queue_elem_s  *next; \
    void           *param; \
    os_queue_cb_t  *cb;

struct os_queue_elem_s {
    OS_QUEUE_MSG_HEAD;
};

/**
 * os_queue_t - os_thread queue.
 *
 * @protect:     protect the access to the message queue.
 * @suspend:     os_thread control semaphore.
 * @anchor:      first empty queue element.
 * @stopper:     last empty queue element.
 * @limit:       max. number of queue elements.
 * @count:       current number of queue elements.
 * @is_running:  the thread analyzes the message queue.
 **/
typedef struct {
    spinlock_t       protect;
    sem_t            suspend;
    os_queue_elem_t  anchor;
    os_queue_elem_t  stopper;
    int              limit;
    int              count;
    volatile int     is_running;
} os_queue_t;

/* States of a pthread. */
typedef enum {
	OS_THREAD_SUSPENDED,
	OS_THREAD_RUNNING,
	OS_THREAD_TERMINATED,
	OS_THREAD_FINISHED,
	OS_THREAD_DELETED,
	OS_THREAD_INVALID
} os_thread_state_t;

/**
 * os_sync_t - wait condition for thread create.
 *
 * @done:   status of the wait condition.
 * @cond:   release signal for wait operation.
 * @mutex:  protect the critical section.
 **/
typedef struct {
	int             done;
	pthread_cond_t  cond;
	pthread_mutex_t mutex;
} os_sync_t;

/**
 * os_thread_t - data for all thread interfaces. The caller must not modify any
 * data.
 *
 * @idx:        index of the thread list.
 * @name:       name of the thread.
 * @state:      current state of the pthread.
 * @attr:       thread attribute like SCHED_RR.
 * @prio:       thread priority.
 * @suspend_c:  suspend the caller - parent thread - in os_thread_create().
 * @suspend_t:  resume the child thread in os_thread_start().
 * @queue:      input queue of the os_thread.
 **/
typedef struct {
	int                idx;
	char               name[OS_MAX_NAME_LEN + 1];
	os_thread_state_t  state;
	os_thread_prio_t   prio;
	pthread_attr_t     attr;
	os_sync_t          suspend_c; /* 0 */
	os_sync_t          suspend_t; /* 1 */
	os_queue_t         queue;
} os_thread_t;

/**
 * os_thread_elem_t - entry of the thread table.
 *
 * @thread:     os_thread definition.
 * @idx:        index of the list element.
 * @is_in_use:  1, if the table entry is used.
 **/
typedef struct {
	os_thread_t thread;
	int         idx;
	int         is_in_use;
} os_thread_elem_t;

/*============================================================================
  LOCAL DATA
  ============================================================================*/

/**
 * os_thread_list_s - list of the installed thread.
 *
 * @list:     list of the installed thread.
 * @count:    number of the installed threads.
 * @protect:  protect the access to the thread list.
 **/
static struct os_thread_list_s {
	os_thread_elem_t  elem[OS_THREAD_LIMIT];
	unsigned int      count;
	pthread_mutex_t   protect;
} os_thread_list;

/*============================================================================
  LOCAL FUNCTION PROTOTYPES
  ============================================================================*/
/*============================================================================
  LOCAL FUNCTIONS
  ============================================================================*/

/**
 * os_thread_cb() - callback for the thread context.
 *
 * @arg:  generic callback argument.
 *
 * Return:	None.
 **/
static void os_thread_cb (void *arg)
{
	os_thread_t      *thread;
	os_queue_t       *q;
	os_queue_elem_t  *anchor, *stopper, *elem;
	int  count;

	/* Entry condition. */
	TRAP_IF(arg == NULL);

	/* Decode the reference to the os_thead and os_queue. */
	thread = arg;
	q = &thread->queue;

	printf ("> %s: start[%s]\n", F, thread->name);

	/* Get the reference to the anchor and stopper element. */
	anchor  = &q->anchor;
	stopper = &q->stopper;

	/* Initialize the reference to a message. */
	elem = NULL;

	/* Loop thru the thread callback. */
	for (;;)  {
		/* Enter the critical section. */
		os_spin_lock (&q->protect);

		/* If the input queue is empty, suspend the thread. */
		q->is_running = 0;

		/* Copy the filling level of the queue. */
		count = q->count;

		/* Leave the critical section. */
		os_spin_unlock (&q->protect);

		/* If the input queue is empty, suspend the thread. */
		if (count < 1) {
			/* Print the suspend information. */
			printf ("%s: suspend [%s]\n", F, thread->name);

			/* Suspend the thread. */
			os_sem_wait (&q->suspend);

			/* Print the resume information. */
			printf ("%s: resume [%s]\n", F, thread->name);
		}

		/* Enter the critical section. */
		os_spin_lock (&q->protect);

		/* Change the thread state to running. */
		q->is_running = 1;

		/* Copy the filling level of the queue. */
		count = q->count;

		/* Leave the critical section. */
		os_spin_unlock (&q->protect);

		/* Loop over the message queue. */
		while (count > 0) {
			/* Enter the critical section. */
			os_spin_lock (&q->protect);
			
			/* Decrement the element counter. */
			q->count--;

			/* Get the first queue element. */
			elem = anchor->next;

			/* Calculate the new queue start. */
			anchor->next = elem->next;

			/* Test the fill level of the input queue. */
			if (anchor->next == stopper) {
				/* The input queue is empty. */
				stopper->next = anchor;
			}

			/* Leave the critical section. */
			os_spin_unlock (&q->protect);

			/* Process the current message. */
			elem->cb (elem);

			/* Free the queue element and the message parameter. */
			os_free (&elem);

			/* Enter the critical section. */
			os_spin_lock (&q->protect);

			/* Copy the filling level of the queue. */
			count = q->count;

			/* Leave the critical section. */
			os_spin_unlock (&q->protect);
		}
	}
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

	/* Get the reference to the  os_queue. */
	q = &thread->queue;

	/* Initialize the spinlock. */
	os_spin_init (&q->protect);

	/* Create the thread control semaphore. */
	os_sem_init (&q->suspend, 0);

	/* Initialize the input queue. */
	q->anchor.next  = &q->stopper;
	q->stopper.next = &q->anchor;

	/* Test the queue size. */
	TRAP_IF(q_size >= OS_QUEUE_LIMIT);
	
	/* Initialize the queue element counter. */
	q->limit = q_size;
	q->count = 0;

	/* Reset the thread state. */
	q->is_running = 0;
}

/**
 * os_sync_complete() - finalize the synchronization between the parent and child
 * thread.
 *
 * @sync:       address of the pthread synchronization conditions.
 *
 * Return:	None.
 **/
static void os_sync_complete(os_sync_t *sync)
{
	int ret;

	/* Enter the critical section. */
	os_cs_enter(&sync->mutex);

	/* Update the synchronization status. */
	sync->done = 1;

	/* Resume the suspended thread in os_sync_poll(). */
	ret = pthread_cond_signal(&sync->cond);
	TRAP_IF(ret != 0);

	/* Leave the critical section. */
	os_cs_leave(&sync->mutex);
}

/**
 * os_sync_wait() - synchronize the parent and child thread.
 *
 * @sync:       address of the pthread synchronization conditions.
 *
 * Return:	None.
 **/
static void os_sync_wait(os_sync_t *sync)
{
	int ret;

	/* Enter the critical section. */
	os_cs_enter(&sync->mutex);

	/* Wait for the call of os_sync_end(). */
	while (! sync->done) {
		/* Release the mutex atomically and block the calling thread on cond. */
		ret = pthread_cond_wait(&sync->cond, &sync->mutex);
		TRAP_IF(ret != 0);
	}

	/* Leave the critical section. */
	os_cs_leave(&sync->mutex);
}

/**
 * os_sync_init() - initialize the pthread wait conditions.
 *
 * @sync:       address of the pthread synchronization conditions.
 *
 * Return:	None.
 **/
static void os_sync_init(os_sync_t *sync)
{
	int ret = -1;

	/* Entry condition. */
	TRAP_IF(sync == NULL);
	
	/* Initialize the mutex for the critical section. */
	os_cs_init (&sync->mutex);

	/* Initialize the condition variable. */
	ret = pthread_cond_init(&sync->cond, NULL);
	TRAP_IF(ret != 0);

	/* Reset the synchronization status. */
	sync->done = 0;
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
	os_cs_enter (&list->protect);
	
	/* Search for a free table entry. */
	for (i = 0; i < OS_THREAD_LIMIT && elem->is_in_use; i++, elem++)
		;

	/* Test the table state. */
	TRAP_IF(i >= OS_THREAD_LIMIT);

	/* Allocate a thread list element. */
	elem->idx       = i;
	elem->is_in_use = 1;
	
	list->count++;

	/* Save the index. */
	elem->thread.idx = i;

	/* Initialize the input queue of the thread. */
	os_queue_init(&elem->thread, q_size);
	
	/* Leave the critical section. */
	os_cs_leave (&list->protect);

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
 * os_thread_create() - create a thread.
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
	int                  ret, c_type, orig_c;
	struct sched_param   p;
	os_thread_t         *thread;

	/* Entry condition. */
	TRAP_IF(name == NULL);

	/* Allocate the os_thread. */
	thread = os_thread_alloc(q_size);
	
	/* Calculate the thread priority. */
	prio = os_thread_prio(prio);
	
	/* Initialize the thread attributes with default values. */
	ret = pthread_attr_init(&thread->attr);
	TRAP_IF(ret != 0);

	/* Set the scheduling policy attribute. */
	ret = pthread_attr_setschedpolicy(&thread->attr, SCHED_RR);
	TRAP_IF(ret != 0);

	/* Define the thread priority. */
	p.sched_priority = prio;
	ret = pthread_attr_setschedparam(&thread->attr, &p);
	TRAP_IF(ret != 0);

	/* Save the thread name. */
	os_memset(thread->name, 0, sizeof(thread->name));
	os_strcpy(thread->name, sizeof(thread->name), name);

	/* Complete the thread state. */
	thread->state = OS_THREAD_SUSPENDED;
	thread->prio  = prio;

	/* Initialize the synchronization elements. */
	os_sync_init(&thread->suspend_c);
	os_sync_init(&thread->suspend_t);

	/* Set the cancelability type. */
	c_type = PTHREAD_CANCEL_ENABLE;
	ret = pthread_setcancelstate(c_type, &orig_c);
	TRAP_IF(ret != 0);

#if 0 /* XXX */
	/* Create the pthread. */
	ret = pthread_create(&thread->p_id, NULL, os_thread_start_cb, thread);
	TRAP_IF(ret != 0);
#endif
	
	/* Suspend the caller and wait for the thread start. */
	os_sync_wait(&thread->suspend_c);

	/* Update the thread state. */
	thread->state = OS_THREAD_SUSPENDED;

	return thread;
}

/**
 * os_thread_start() - start the thread.
 *
 * @thread:  address of the pthread data.
 *
 * Return:	None.
 **/
void os_thread_start(void *thread)
{
	/* Entry condition. */
	TRAP_IF(thread == NULL);
}

/**
 * os_thread_init() - initialize the thread table.
 *
 * Return:	None.
 **/
void os_thread_init(void)
{
	/* Create the mutex for the critical section. */
	os_cs_init(&os_thread_list.protect);
}
