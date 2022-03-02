// SPDX-License-Identifier: GPL-2.0

/*
 * Operating system interfaces.
 *
 * Copyright (C) 2022 Gerald Schueller <gerald.schueller@web.de>
 */

/*============================================================================
  IMPORTED INCLUDE REFERENCES
  ============================================================================*/
#include "os.h"          /* Operating system: OS_MALLOC() */
#include "os_private.h"  /* Local interfaces of the OS: os_trap_init() */

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
/**
 * os_mq_t - buffer queue for the I/O operations.
 *
 * @mutex          protect the critical section of the queue operations.
 * @buf:           pointer to the queue memory.
 * @size:          size of the queue buffer.
 * @_1st_idx:      start index of the first queue buffer.
 * @_1st_size      size of the first queue buffer.
 * @_2nd_idx:      start index of the second queue buffer.
 * @_2nd_size:     size of the second queue buffer.
 * @lock_idx:      start index of the reseved queue buffer.
 * @lock_size:     size of the reseved queue buffer.
 **/
typedef struct {
	pthread_mutex_t  mutex;
	char  *buf;
	int    size;
	int    _1st_idx;
	int    _1st_size;
	int    _2nd_idx;
	int    _2nd_size;
	int    lock_idx;
	int    lock_size;  
} os_mq_t;

/*============================================================================
  LOCAL DATA
  ============================================================================*/
/*============================================================================
  LOCAL FUNCTION PROTOTYPES
  ============================================================================*/
/**
 * os_mq_remove() - remove the message from the I/O queue.
 *
 * @q:     pointer to the I/O queue.
 * @size:  size of the consumed message.
 *
 * Return:	None.
 **/
static void os_mq_remove(os_mq_t *q, int size)
{
	/* Test the fill level of the first mesage queue buffer. */
	if (size >= q->_1st_size) {
		/* The first mesage queue buffer is empty. */

		/* Switch to the second queue buffer. */
		q->_1st_idx  = q->_2nd_idx;
		q->_1st_size = q->_2nd_size;
		
		/* Reset the second queue buffer. */
		q->_2nd_idx  = 0;
		q->_2nd_size = 0;
        }
        else {
		/* Remove the message from the first queue buffer. */
		q->_1st_idx  += size;
		q->_1st_size -= size;
        }
}

/**
 * os_mq_get() - get the pointer to the next message bufffer.
 *
 * @q:     pointer to the I/O queue.
 * @size:  return the size of the the I/O queue.
 *
 * Return:	pointer to the message buffer.
 **/
static char *os_mq_get(os_mq_t *q, int *size)
{
	/* Test the fill level of the first mesage queue buffer. */
	if (q->_1st_size < 1)
		return NULL;

	/* Save the size of the message queue. */
	*size = q->_1st_size;

	/* Return the start of the message buffer. */
	return (q->buf + q->_1st_idx);
}

/**
 * os_mq_add() - save the new message in the 1st or 2nd queue buffer.
 *
 * @q:    pointer to the I/O queue.
 * size:  size of the message buffer.
 *
 * Return:	pointer to the message buffer.
 **/
static void os_mq_add(os_mq_t *q, int size)
{
	/* Entry condition. */
	OS_TRAP_IF(size < 1 || size != q->lock_size);
	
	/* Test the initial state of the queue buffer. */
        if (q->_1st_size == 0 && q->_2nd_size == 0) {
		/* Start with the first queue buffer. */
		q->_1st_idx  = q->lock_idx;
		q->_1st_size = q->lock_size;

		/* Unlock the first queue buffer. */
		q->lock_idx  = 0;
		q->lock_size = 0;
		return;
        }

	/* Test the state of the first queue buffer. */
        if (q->lock_idx == q->_1st_idx + q->_1st_size) {
		/* The current message shall be saved in the first queue. */
		q->_1st_size += q->lock_size;
        }
        else {
		/* The current message shall be saved in the second queue. */
		q->_2nd_size += q->lock_size;
        }

	/* Unlock the queue buffer. */
	q->lock_idx  = 0;
	q->lock_size = 0;
}

/**
 * os_mq_alloc() - reserve a queue buffer.
 *
 * @q:    pointer to the I/O queue.
 * size:  size of the wanted message buffer.
 *
 * Return:	pointer to the message buffer.
 **/
static char *os_mq_alloc(os_mq_t *q, int size)
{
	int free;
	
	/* Entry condition. */
	OS_TRAP_IF(size < 1 || q->lock_size > 0);
	
        /* Test the state of the second queue buffer. */
        if (q->_2nd_size > 0) {
		/* Calculate the free space of the second queue buffer. */
		free = q->_1st_idx - q->_2nd_idx - q->_2nd_size;

		/* Compare the gotten and wanted memory. */
		if (free < 1 || size > free)
			return NULL;

		/* Reserve the message buffer from the second queue. */
		q->lock_idx  = q->_2nd_idx + q->_2nd_size;
		q->lock_size = size;
		return q->buf + q->lock_idx;
        }
        else {
		/* Calculate the free space of the first queue buffer. */
		free = q->size - q->_1st_idx - q->_1st_size;
		
		/* Compare the gotten and wanted memory. */
		if (free < 1 || size > free)
			return NULL;
		
		/* Test the state of the first queue buffer. */
		if (free >= q->_1st_idx) {
			/* Reserve the message buffer from the first queue. */
			q->lock_idx  = q->_1st_idx + q->_1st_size;
			q->lock_size = size;
			return q->buf + q->lock_idx;
		}
		else {
			/* Switch to the second queue buffer: free < 1st_index */

			/* Reserve the message buffer from the second queue. */
			q->lock_idx  = 0;
			q->lock_size = size;
			return q->buf;
		}
        }
}

/*============================================================================
  LOCAL FUNCTIONS
  ============================================================================*/
/*============================================================================
  EXPORTED FUNCTIONS
  ============================================================================*/
/**
 * os_mq_rmem() - get the fill level of the queue buffer for reading.
 *
 * @queue:  generic pointer to the I/O queue.
 *
 * Return:	the fill level of the queue buffer.
 **/
int os_mq_rmem(void *queue)
{	
	os_mq_t *q;
	int size;
	
	/* Entry condition. */
	OS_TRAP_IF(queue == NULL);
	
	/* Decode the reference to the queue. */
	q = queue;
	
	/* Enter the critical section. */
	os_cs_enter(&q->mutex);

	/* Calculate the fill level of the queue buffer. */
	size = q->_1st_size + q->_2nd_size;
		
	/* Leave the critical section. */
	os_cs_leave(&q->mutex);

	return size;
}

/**
 * os_mq_wmem() - get the size of the free output message buffer for writing. 
 *
 * @queue:  generic pointer to the I/O queue.
 *
 * Return:	the free queue buffer space.
 **/
int os_mq_wmem(void *queue)
{
	os_mq_t *q;
	int size;
	
	/* Entry condition. */
	OS_TRAP_IF(queue == NULL);
	
	/* Decode the reference to the queue. */
	q = queue;
	
	/* Enter the critical section. */
	os_cs_enter(&q->mutex);

	/* Entry condition. */
	OS_TRAP_IF(q->lock_size > 0);
	
        /* Test the state of the second queue buffer. */
        if (q->_2nd_size > 0) {
		/* Calculate the free space of the second queue buffer. */
		size = q->_1st_idx - q->_2nd_idx - q->_2nd_size;
        }
        else {
		/* Calculate the free space of the first queue buffer. */
		size = q->size - q->_1st_idx - q->_1st_size;
        }
	
	/* Leave the critical section. */
	os_cs_leave(&q->mutex);

	return size;
}

/**
 * os_mq_write() - write to an I/O queue. os_mq_add() writes up to count
 * bytes from the buffer starting at buf to the queue referred to by q.
 *
 * @queue:  generic pointer to the I/O queue.
 * @buf:    pointer to the queue element.
 * @count:  size of the queue element.
 *
 * Return:	1, if buf has been added, otherwise 0.
 **/
int os_mq_write(void *queue, char *buf, int count)
{
	os_mq_t *q;
	int rv;
	char *dest;

	/* Entry condition. */
	OS_TRAP_IF(queue == NULL);
	
	/* Decode the reference to the queue. */
	q = queue;
	
	/* Enter the critical section. */
	os_cs_enter(&q->mutex);

	/* Entry condition. */
	OS_TRAP_IF(q->size < count);

	/* Initialize the return value. */
	rv = 0;
	
	/* Allocate a message buffer. */
	dest = os_mq_alloc(q, count);
	if (dest == NULL)
		goto l_end;
	
	/* XXX Replace end of string with the message delimiter. */
	buf[count - 1] = '#';

	/* Save the message. */
	os_memcpy(dest, count, buf, count);
	
	/* Release the message buffer. */
	os_mq_add(q, count);
	
	/* Update the return value. */
	rv = 1;

l_end:
	/* Leave the critical section. */
	os_cs_leave(&q->mutex);

	return rv;
}

/**
 * os_mq_read() - read from an I/O queue. os_mq_read() attempts to read up to
 * count bytes from the queue into the buffer starting at buf.
 *
 * @queue:  generic pointer to the I/O queue.
 * @buf:    pointer to the destination queue element buffer.
 * @count:  size of the queue element buffer.
 *
 * Return:	the number of bytes read.
 **/
int os_mq_read(void *queue, char *buf, int count)
{
	os_mq_t *q;
	char *src, *end;
	int rv, size;
	
	/* Entry condition. */
	OS_TRAP_IF(queue == NULL);
	
	/* Decode the reference to the queue. */
	q = queue;
	
	/* Enter the critical section. */
	os_cs_enter(&q->mutex);

	/* Initialize the return value. */
	rv = 0;
	
	/* Get the pointer to the next message. */
	size = 0;
	src = os_mq_get(q, &size);
	if (src == NULL)
		goto l_end;

	/* XXX Search for the end of the queue element. */
	end = os_memchr(src, src + size, '#', size);
	OS_TRAP_IF(end == NULL);

	/* Calculate the lenght of the queue element. */
	end++;
	size = end - src;
	OS_TRAP_IF(size < 2 || size > count);

	/* Copy the queue element. */
	os_memcpy (buf, count, src, size);

	/* XXX Replace the queue element delimter with EOS. */
	buf[size - 1] = '\0';

	/* Remove the message from the I/O queue. */
	os_mq_remove(q, size);

	/* Copy the size of the queue element. */
	rv = size - 1;

l_end:
	/* Leave the critical section. */
	os_cs_leave(&q->mutex);

	return rv;
}

/**
 * os_mq_delete() - frees the buffer queue object, which must have been returned
 * by a previous call to os_mq_init().
 *
 * @queue:  generic pointer to the queue object.
 *
 * Return:	None.
 **/
void os_mq_delete(void *queue)
{
	os_mq_t *q;
	
	/* Entry condition. */
	OS_TRAP_IF(queue == NULL);
	
	/* Decode the reference to the queue. */
	q = queue;
	
	/* Free the queue buffer. */
	OS_FREE(q->buf);
	
	/* Release the mutex for the critical sections in the queue
	 * operations. */
	os_cs_destroy(&q->mutex);

	/* Free the queue object. */
	OS_FREE(q);

}

/**
 * os_mq_init() - creates the buffer queue object for the I/O operations.
 *
 * @size:  size of the queue memory.
 *
 * Return:	the generic pointer to the queue object.
 **/
void *os_mq_init(int size)
{
	os_mq_t *q;
	
	/* Allocate the queue object. */
	q = OS_MALLOC(sizeof(os_mq_t));
	os_memset(q, 0, sizeof(os_mq_t));

	/* Create the mutex for the critical sections in the queue
	 * operations. */
	os_cs_init(&q->mutex);

	/* Allocate the queue buffer. */
	q->buf = OS_MALLOC(size);
	os_memset(q->buf, 0, size);

	/* Save the queue buffer size. */
	q->size = size;

	return q;
}
