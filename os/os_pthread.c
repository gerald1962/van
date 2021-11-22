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
 * @suspend:     os_thread control semaphore.
 * @anchor:      first empty queue element.
 * @stopper:     last empty queue element.
 * @limit:       max. number of queue elements.
 * @count:       current number of queue elements.
 * @is_running:  the thread analyzes the message queue.
 * @busy_send:   if 1, a client executes os_queue_send.
 **/
typedef struct {
	spinlock_t      protect;
	sem_t           suspend;
	os_queue_elem_t anchor;
	os_queue_elem_t stopper;
	int             limit;
	int             count;
	volatile int    is_running;
	atomic_int      busy_send;
} os_queue_t;

/* XXX */
/**
 * os_thread_state_t- states of an os_thread.
 *
 * @OS_THREAD_INIT:     initialisation phase of the thread.
 * @OS_THREAD_BOOT:     created but not started.
 * @OS_THREAD_READY:    ready to process messages.
 * @OS_THREAD_FROZEN:   disabled input queue: effect(os_queue_disable(t)).
 * @OS_THREAD_FNISHED:  inactive thread callback.
 * @OS_THREAD_INVALID:  undefined.
 **/
typedef enum {
	OS_THREAD_INIT,
	OS_THREAD_BOOT,
	OS_THREAD_READY,
	OS_THREAD_FROZEN,
	OS_THREAD_FINISHED,
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
 * @prio:       thread priority.
 * @door:       mutex for the critical sections.
 * @attr:       thread attribute like SCHED_RR.
 * @pthread:    pthread object.
 * @suspend_p:  suspend the caller - parent thread - in os_thread_create().
 * @suspend_c:  resume the child thread in os_thread_start().
 * @queue:      input queue of the os_thread.
 **/
typedef struct {
	int                idx;
	char               name[OS_MAX_NAME_LEN + 1];
	_Atomic os_thread_state_t  state;
	os_thread_prio_t   prio;
	pthread_attr_t     attr;
	/* XXX */
#if 0
	pthread_mutex_t    door;
#endif
	pthread_t          pthread;
	os_sync_t          suspend_p;
	os_sync_t          suspend_c;
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
 * @key:      data key visible to all threads.
 * @once_c:   
 **/
static struct os_thread_list_s {
	os_thread_elem_t  elem[OS_THREAD_LIMIT];
	unsigned int      count;
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
 * @q:      pointer to the message queue.
 * @count:  fill level of the message queue.
 *
 * Return:	none.
 **/
static void os_queue_loop (os_queue_t *q, int count)
{
	os_queue_elem_t *elem;
	
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

		/* Free the queue element and the message parameter. */
		OS_FREE(elem);

		/* Enter the critical section. */
		os_spin_lock(&q->protect);

		/* Copy the filling level of the queue. */
		count = q->count;

		/* Leave the critical section. */
		os_spin_unlock(&q->protect);
	}
}

/**
 * os_thread_suspend() - suspend the thread, if the message queue is empty.
 *
 * @q:     pointer to the message queue.
 * @name:  name of the thread.
 *
 * Return:	fill level of the message queue.
 **/
static int os_thread_suspend (os_queue_t *q, char *name)
{
	int count;

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
		/* Print the suspend information. */
		OS_TRACE(("%s [t=%s,s=ready,o=suspend]\n", OS, name));

		/* Suspend the thread. */
		os_sem_wait (&q->suspend);

		/* Print the resume information. */
		OS_TRACE(("%s [t=%s,s=ready,o=resume]\n", OS, name));
	}

	/* Enter the critical section. */
	os_spin_lock(&q->protect);

	/* Change the thread state to running. */
	q->is_running = 1;


	/* Copy the filling level of the queue. */
	count = q->count;

	/* Leave the critical section. */
	os_spin_unlock(&q->protect);
		
	return count;
}

/**
 * os_thread_cb() - callback for the thread context.
 *
 * @thread:  reference to os_thread.
 *
 * Return:	None.
 **/
static void os_thread_cb (os_thread_t *thread)
{
	os_queue_t *q;
	int  count;

	/* Entry condition. */
	OS_TRAP_IF(thread == NULL);

	/* Get the reference to the thread input queue. */
	q = &thread->queue;

	OS_TRACE(("%s [t=%s,s=ready,o=boot]\n", OS, thread->name));

	/* Loop thru the thread callback. */
	for (;;)  {
		/* Suspend the thread, if the message queue is empty. */
		count = os_thread_suspend(q, thread->name);

		/* Loop over the message queue. */
		os_queue_loop(q, count);
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
	os_spin_init(&q->protect);

	/* Create the thread control semaphore. */
	os_sem_init(&q->suspend, 0);

	/* Initialize the input queue. */
	q->anchor.next  = &q->stopper;
	q->stopper.next = &q->anchor;

	/* Test the queue size. */
	OS_TRAP_IF(q_size >= OS_QUEUE_LIMIT);
	
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
	OS_TRAP_IF(ret != 0);

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
		OS_TRAP_IF(ret != 0);
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
	OS_TRAP_IF(sync == NULL);
	
	/* Initialize the mutex for the critical section. */
	os_cs_init(&sync->mutex);

	/* Initialize the condition variable. */
	ret = pthread_cond_init(&sync->cond, NULL);
	OS_TRAP_IF(ret != 0);

	/* Reset the synchronization status. */
	sync->done = 0;
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

/* XXX */
#if 0
/**
 * os_thread_state_get() - provide current thread state.
 *
 * @thread: pointer to the os_thread.
 * Return:	the thread state.
 **/
static os_thread_state_t os_thread_state_get(os_thread_t *thread)
{
	os_thread_state_t state;

	/* Enter the critical section. */
	os_cs_enter(&thread.door);

	/* Copy the thread state. */
	state = thread->state;

	/* Leave the critical section. */
	os_cs_leave(&thread.door);

	return state;
}

/**
 * os_thread_state_set() - change the thread state.
 *
 * @thread: pointer to the os_thread.
 * @state:  new thread state.
 * Return:	None.
 **/
static void os_thread_state_set(os_thread_t *thread, os_thread_state_t state)
{
	/* Enter the critical section. */
	os_cs_enter(&thread.door);

	/* Update the thread state. */
	thread->state = state;

	/* Leave the critical section. */
	os_cs_leave(&thread.door);

}
#endif

/**
 * os_thread_start_cb() - callback of the thread.
 *
 * @arg:  pointer to the OS thread.
 *
 * Return:	the NULL pointer.
 **/
static void *os_thread_start_cb(void *arg)
{
	os_thread_t    *thread;
	pthread_key_t   key;
	int ret;
	
	/* Decode the reference to the os_thread. */
	thread = arg;

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

	/* Resume the caller of os_thread_create. */
	os_sync_complete(&thread->suspend_p);

	/* Suspend this thread and resume it os_thread_start. */
	os_sync_wait(&thread->suspend_c);

	/* Start the message queue handler. */
	os_thread_cb(thread);

	/* Change the thread state. */
	atomic_store(&thread->state, OS_THREAD_FINISHED);
	
	/* This statement can never be executed. */
	OS_TRAP();
	
	return NULL;
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

	/* Allocate a thread list element. */
	elem->idx       = i;
	elem->is_in_use = 1;
	
	list->count++;

	/* Save the index. */
	elem->thread.idx = i;

	/* Initialize the input queue of the thread. */
	os_queue_init(&elem->thread, q_size);
	
	/* Leave the critical section. */
	os_cs_leave(&list->protect);

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
	OS_TRAP_IF(name == NULL);

	/* Allocate the os_thread. */
	thread = os_thread_alloc(q_size);
	
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

	/* Save the thread name. */
	os_memset(thread->name, 0, sizeof(thread->name));
	os_strcpy(thread->name, sizeof(thread->name), name);

	/* Initialize the thread state. */
	atomic_store(&thread->state, OS_THREAD_INIT);
	
	/* Save the thread prioriiy. */
	thread->prio  = prio;

	/* Initialize the synchronization elements. */
	os_sync_init(&thread->suspend_p);
	os_sync_init(&thread->suspend_c);

	/* Set the cancelability type. */
	c_type = PTHREAD_CANCEL_ENABLE;
	ret = pthread_setcancelstate(c_type, &orig_c);
	OS_TRAP_IF(ret != 0);

	/* Create the pthread. */
	ret = pthread_create(&thread->pthread, NULL, os_thread_start_cb, thread);
	OS_TRAP_IF(ret != 0);
	
	/* Suspend the caller and wait for the thread start. */
	os_sync_wait(&thread->suspend_p);

	/* Update the thread state. */
	atomic_store(&thread->state, OS_THREAD_BOOT);

	return thread;
}

/**
 * os_thread_start() - activate the callback for the message processing.
 *
 * @g_thread:  generic address of the os_thread.
 *
 * Return:	None.
 **/
void os_thread_start(void *g_thread)
{
	os_thread_state_t state;
	os_thread_t *thread;
	
	/* Entry condition. */
	OS_TRAP_IF(g_thread == NULL);

	/* Decode the reference to the os_thread. */
	thread = g_thread;

	/* Get the thread state. */
	state = atomic_load(&thread->state);
	
	/* Analyze the thread state. */
	switch (state) {
	case OS_THREAD_BOOT:
		/* Resume the child thread. */
		os_sync_complete(&thread->suspend_c);
		
		/* Change the os_thread state. */
		atomic_store(&thread->state, OS_THREAD_READY);
		break;

	default:
		/* Unexpected thread state. */
		OS_TRAP();
		break;
    }
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
		os_sem_release (&q->suspend);
	}
	
	/* Change the state of this operation. */
	atomic_store(&q->busy_send, 0);	
}

/**
 * os_thread_delete() - delete a thread. Be carefull when deleting a thread,
 *  as it may hold some resources which has not been released.
 *
 * @g_thread:  generic address of the os_thread.
 *
 * Return:	None.
 **/
void os_thread_delete(void *g_thread)
{
	os_thread_state_t state;
	os_thread_t *thread;
	
	/* Entry condition. */
	OS_TRAP_IF(g_thread == NULL);

	/* Decode the reference to the thread state. */
	thread = g_thread;
	
	/* Get, modify and test the thread state. */
	state = atomic_load(&thread->state);
	atomic_store(&thread->state, OS_THREAD_FROZEN);

	/* Verirfy the thread state. */
	OS_TRAP_IF(state != OS_THREAD_READY);	
}

	/* XXX */
#if 0
/* Test the queue state of all threads. */
stat = os_thread_is_inactive();

/**
 * os_thread_is_inactive() - i
 *
 * Return:	1, if the thread state is OS_THREAD_FROZEN triggered by ???  and
 * the input queues are empty.
 **/
int os_thread_is_inactive(void)
{
	struct os_thread_list_s *list;
	os_thread_elem_t *elem;
	os_thread_state_t state;
	int i, stat;

	/* Initialize the return value. */
	stat = 0;
	
	/* Get the address of the thread list. */
	list = &os_thread_list;
	
	/* Get the address of the first list element. */
	elem = list->elem;

	/* Enter the critical section. */
	os_cs_enter(&list->protect);
	
	/* Search for a free table entry. */
	for (i = 0; i < OS_THREAD_LIMIT; i++, elem++) {
		/* Test the element state. */
		if (! elem->is_in_use)
			continue;

		/* Test the thread state. */
		state = os_thread_state_get(elem->thread);
		if (state != OS_THREAD_FROZEN)
			break;
	}

	/* Leave the critical section. */
	os_cs_leave(&list->protect);

	return stat;

	/* Test the table state. */
	OS_TRAP_IF(i >= OS_THREAD_LIMIT);

	/* Allocate a thread list element. */
	elem->idx       = i;
	elem->is_in_use = 1;
	
	list->count++;

	/* Save the index. */
	elem->thread.idx = i;

	/* Initialize the input queue of the thread. */
	os_queue_init(&elem->thread, q_size);
	
	/* Leave the critical section. */
	os_cs_leave(&list->protect);
}
#endif

/**
 * os_thread_init() - initialize the thread list.
 *
 * Return:	None.
 **/
void os_thread_init(void)
{
	/* Create the mutex for the critical section. */
	os_cs_init(&os_thread_list.protect);

	/* Initialize the control for pthread_once. */
	os_thread_list.once_c = PTHREAD_ONCE_INIT;
}
