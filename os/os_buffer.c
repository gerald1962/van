// SPDX-License-Identifier: GPL-2.0

/*
 * Cable operations about I/O buffering.
 *
 * Copyright (C) 2022 Gerald Schueller <gerald.schueller@web.de>
 */

/*============================================================================
  IMPORTED INCLUDE REFERENCES
  ============================================================================*/
#include "os.h"          /* Operating system: os_bopen() */
#include "os_private.h"  /* Local interfaces of the OS: os_buf_init() */

/*============================================================================
  EXPORTED INCLUDE REFERENCES
  ============================================================================*/
/*============================================================================
  LOCAL NAME CONSTANTS DEFINITIONS
  ============================================================================*/
#define BUF_EP_COUNT    1  /* Number of the available buffer entry points. */
#define BUF_Q_SIZE   2048  /* Size of the I/O queue buffer. */

/*============================================================================
  MACROS
  ============================================================================*/
/*============================================================================
  LOCAL TYPE DEFINITIONS
  ============================================================================*/
/**
 * buf_q_t - buffer queue for the I/O operations.
 *
 * @mutex          protect the critical section of the queue operations.
 * @trigger:       if 1, the driver expects the read or write trigger.
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
	atomic_int       trigger;
	char  *buf;
	int    size;
	int    _1st_idx;
	int    _1st_size;
	int    _2nd_idx;
	int    _2nd_size;
	int    lock_idx;
	int    lock_size;  
} buf_q_t;

/*============================================================================
  LOCAL DATA
  ============================================================================*/
/**
 * buf_list_s - list of all available entry points names.
 *
 * @name:  list of the enty point names.
 * @mutex: protext the access to the name list.
 **/
static struct buf_list_s {
	const char  *name[BUF_EP_COUNT + 1];
	pthread_mutex_t  mutex;
} buf_list = {
	{ "/display", NULL }, {{ 0 }}
};

/**
 * buf_data_s - list of the entry point states.
 *
 * @u_id:      user entry point id.
 * @c_id:      cable driver entry point id.
 * @name:      pointer to the entry point name.
 * @in:        input queue, written from the cable driver with buf_write_cb().
 * @out:       output queue, read from the cable driver with buf_read_cb().
 **/
static struct buf_data_s {
	int    u_id;
	int    c_id;
	const char  *name;
	buf_q_t  *in;
	buf_q_t  *out;
} *buf_data[BUF_EP_COUNT];

/*============================================================================
  LOCAL FUNCTION PROTOTYPES
  ============================================================================*/

/*============================================================================
  LOCAL FUNCTIONS
  ============================================================================*/
/**
 * buf_q_buffered() - get the fill level of the queue buffer.
 *
 * @q:     pointer to the I/O queue.
 *
 * Return:	the fill level of the queue buffer.
 **/
static int buf_q_buffered(buf_q_t *q)
{
	int size;
	
	/* Enter the critical section. */
	os_cs_enter(&q->mutex);

	/* Calculate the fill level of the queue buffer. */
	size = q->_1st_size + q->_2nd_size;
		
	/* Leave the critical section. */
	os_cs_leave(&q->mutex);

	return size;
}

/**
 * buf_q_remove() - remove the message from the I/O queue.
 *
 * @q:     pointer to the I/O queue.
 * @size:  size of the consumed message.
 *
 * Return:	None.
 **/
static void buf_q_remove(buf_q_t *q, int size)
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
 * buf_q_get() - get the pointer to the next message bufffer.
 *
 * @q:     pointer to the I/O queue.
 * @size:  return the size of the the I/O queue.
 *
 * Return:	pointer to the message buffer.
 **/
static char *buf_q_get(buf_q_t *q, int *size)
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
 * buf_q_add() - save the new message in the 1st or 2nd queue buffer.
 *
 * @q:    pointer to the I/O queue.
 * size:  size of the message buffer.
 *
 * Return:	pointer to the message buffer.
 **/
static void buf_q_add(buf_q_t *q, int size)
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
 * buf_q_alloc() - reserve a queue buffer.
 *
 * @q:    pointer to the I/O queue.
 * size:  size of the wanted message buffer.
 *
 * Return:	pointer to the message buffer.
 **/
static char *buf_q_alloc(buf_q_t *q, int size)
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

/**
 * buf_q_read() - read from an I/O queue. buf_q_read() attempts to read up to
 * count bytes from the queue into the buffer starting at buf.
 *
 * @q:      pointer to the I/O queue.
 * @buf:    pointer to the destination queue element buffer.
 * @count:  size of the queue element buffer.
 *
 * Return:	the number of bytes read.
 **/
static int buf_q_read(buf_q_t *q, char *buf, int count)
{
	char *src, *end;
	int rv, size;

	/* Enter the critical section. */
	os_cs_enter(&q->mutex);

	/* Initialize the return value. */
	rv = 0;
	
	/* Get the pointer to the next message. */
	size = 0;
	src = buf_q_get(q, &size);
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
	buf_q_remove(q, size);

	/* Copy the size of the queue element. */
	rv = size - 1;

l_end:
	/* Leave the critical section. */
	os_cs_leave(&q->mutex);

	return rv;
}

/**
 * buf_q_write() - write to an I/O queue. buf_q_add() writes up to count
 * bytes from the buffer starting at buf to the queue referred to by q.
 *
 * @q:      pointer to the I/O queue.
 * @buf:    pointer to the queue element.
 * @count:  size of the queue element.
 *
 * Return:	1, if buf has been added, otherwise 0.
 **/
static int buf_q_write(buf_q_t *q, char *buf, int count)
{
	int rv;
	char *dest;

	/* Enter the critical section. */
	os_cs_enter(&q->mutex);

	/* Entry condition. */
	OS_TRAP_IF(q->size < count);

	/* Initialize the return value. */
	rv = 0;
	
	/* Allocate a message buffer. */
	dest = buf_q_alloc(q, count);
	if (dest == NULL)
		goto l_end;
	
	/* XXX Replace end of string with the message delimiter. */
	buf[count - 1] = '#';

	/* Save the message. */
	os_memcpy(dest, count, buf, count);
	
	/* Release the message buffer. */
	buf_q_add(q, count);
	
	/* Update the return value. */
	rv = 1;

l_end:
	/* Leave the critical section. */
	os_cs_leave(&q->mutex);

	return rv;
}

/**
 * buf_q_delete() - frees the buffer queue object, which must have been returned
 * by a previous call to buf_q_init().
 *
 * @q:  pointer to the queue object.
 *
 * Return:	None.
 **/
static void buf_q_delete(buf_q_t *q)
{
	/* Free the queue buffer. */
	OS_FREE(q->buf);
	
	/* Release the mutex for the critical sections in the queue
	 * operations. */
	os_cs_destroy(&q->mutex);

	/* Free the queue object. */
	OS_FREE(q);

}

/**
 * buf_q_init() - creates the buffer queue object for the I/O operations.
 *
 * @size:  size of the queue memory.
 *
 * Return:	the pointer to the queue object.
 **/
static buf_q_t *buf_q_init(int size)
{
	buf_q_t *q;
	
	/* Allocate the queue object. */
	q = OS_MALLOC(sizeof(buf_q_t));
	os_memset(q, 0, sizeof(buf_q_t));

	/* Create the mutex for the critical sections in the queue
	 * operations. */
	os_cs_init(&q->mutex);

	/* Allocate the queue buffer. */
	q->buf = OS_MALLOC(size);
	os_memset(q->buf, 0, size);

	/* Save the queue buffer size. */
	q->size = size;

	/* Initialize the driver trigger. */
	atomic_store(&q->trigger, 1);
	
	return q;
}
	
/**
 * buf_map_cid() - map the driver cable entry id to the user ep state.
 *
 * @c_id:  cable driver entry point id e.g. controller.
 *
 * Return:	the pointer to the user cable end point.
 **/
static struct buf_data_s *buf_map_cid(int c_id)
{
	struct buf_data_s **b;
	int i;

	/* Loop through the entry point list. */
	for (i = 0, b = buf_data; i < BUF_EP_COUNT; i++, b++) {
		if (*b == NULL)
			continue;

		/* Test the device id. */
		if ((*b)->c_id == c_id)
			break;
	}

	/* Final condition. */
	OS_TRAP_IF(i >= BUF_EP_COUNT);

	return *b;
}

/**
 * buf_read_cb() -  asyncronous input operation, which is executed in the
 * context of the controller interrupt thread.
 *
 * @c_id:   cable driver entry point id e.g. controller.
 * @buf:    pointer to the rx wire memory of the cable.
 * @count:  fill level of the rx wire buffer.
 *
 * Return:	number of the processed rx characters.
 **/
static int buf_read_cb(int c_id, char *buf, int count)
{
	struct buf_data_s *b;
	int rv;
	
	/* Entry condition. */
	OS_TRAP_IF(buf == NULL || count < 1);

	/* Map the driver cable entry id to the user ep state. */
	b = buf_map_cid(c_id);

	/* Write to the input queue. */
	rv = buf_q_write(b->in, buf, count);

	/* Test the input state. */
	if (! rv) {
		/* The drive waits for the read trigger. */
		atomic_store(&b->in->trigger, 1);
	}
	
	/* Inform the driver, whether the input message has been saved. */
	return rv ? count : 0;
}

/**
 * buf_write_cb() -  asyncronous output operation, which is executed in the
 * context of the controller interrupt thread.
 *
 * @c_id:   cable driver entry point id e.g. controller.
 * @buf:    pointer to the tx wire memory of the cable.
 * @count:  size of the tx wire buffer.
 *
 * Return:	number of the sent output characters.
 **/
static int buf_write_cb(int c_id, char *buf, int count)
{
	struct buf_data_s *b;
	int size;

	/* Entry condition. */
	OS_TRAP_IF(buf == NULL || count < 1);

	/* Map the driver cable entry id to the user ep state. */
	b = buf_map_cid(c_id);
	
	/* Copy the next queue element. */
	size = buf_q_read(b->out, buf, count);

	/* Test the output state. */
	if (size < 1) {
		/* The drive waits for the write trigger. */
		atomic_store(&b->out->trigger, 1);
	}

	/* Inform the driver, whether an output message is present. */
	return size;
}

/*============================================================================
  EXPORTED FUNCTIONS
  ============================================================================*/
/**
 * os_bsync() - get the fill level of the output queue buffer.
 *
 * @u_id:  id of the entry point.
 *
 * Return:	the fill level of the output queue.
 **/
int os_bsync(int u_id)
{
	struct buf_data_s *b;
	
	/* Entry conditon. */
	OS_TRAP_IF(u_id < 0 || u_id >= BUF_EP_COUNT);

	/* Map the ep id to the ep state. */
	b = buf_data[u_id];

	/* Test the ep state. */
	OS_TRAP_IF(b == NULL);

	return buf_q_buffered(b->out);
}

/**
 * os_bwritable() - get the size of the free output message buffer.
 * buffer.
 *
 * @u_id:  id of the entry point.
 *
 * Return:	the size of the free output message buffer.
 **/
int os_bwritable(int u_id)
{
	struct buf_data_s *b;
	buf_q_t *q;
	int free;
		
	/* Entry conditon. */
	OS_TRAP_IF(u_id < 0 || u_id >= BUF_EP_COUNT);

	/* Map the ep id to the ep state. */
	b = buf_data[u_id];

	/* Test the ep state. */
	OS_TRAP_IF(b == NULL);

	/* Get the pointer to the output queue. */
	q = b->out;

	/* Enter the critical section. */
	os_cs_enter(&q->mutex);

	/* Entry condition. */
	OS_TRAP_IF(q->lock_size > 0);
	
        /* Test the state of the second queue buffer. */
        if (q->_2nd_size > 0) {
		/* Calculate the free space of the second queue buffer. */
		free = q->_1st_idx - q->_2nd_idx - q->_2nd_size;
        }
        else {
		/* Calculate the free space of the first queue buffer. */
		free = q->size - q->_1st_idx - q->_1st_size;
        }
	
	/* Leave the critical section. */
	os_cs_leave(&q->mutex);

	return free;

}

/**
 * os_bwrite() - write to a cable end point. os_bwrite() writes up to count
 * bytes from the buffer starting at buf to the end point referred to by the
 * user end point descriptor u_id.
 * The number of bytes written may be less than count if, for example, there is
 * insufficient space on the underlying buffer.
 * On success, the number of bytes written is returned.
 * On error, -1 is returned.
 * Note that a successful os_bwrite() may transfer fewer than count bytes. Such
 * partial writes can occur for example, because there was insufficient space on
 * the buffer to write all of the requested bytes.
 * If no errors are detected, or error detection is not performed, 0 will be
 * returned without causing any other effect.
 *
 * @u_id:   id of the entry point.
 * @buf:    pointer to the source buffer.
 * @count:  fill level of the source buffer.
 *
 * Return:	the number the number of bytes written.
 **/
int os_bwrite(int u_id, char *buf, int count)
{
	struct buf_data_s *b;
	int rv, trigger;
	
	/* Entry conditon. */
	OS_TRAP_IF(u_id < 0 || u_id >= BUF_EP_COUNT ||
		   buf == NULL || count < 0);

	/* Test the size of the destination buffer. */
	if (count < 1)
		return 0;
	
	/* Map the ep id to the ep state. */
	b = buf_data[u_id];

	/* Test the ep state. */
	OS_TRAP_IF(b == NULL);

	/* Write to the output queue. */
	rv = buf_q_write(b->out, buf, count);

	/* Test the state of the output buffer. */
	trigger = atomic_exchange(&b->out->trigger, 0);
	if (trigger) {
		/* Trigger the cable driver to invoke the async. write
		 * callback. */
		os_c_awrite(b->c_id);
	}
	
	/* Inform the user, that the output message has been saved or none. */
	return rv ? count : 0;
}

/**
 * os_bread() - read from a cable end point. os_bread() attempts to read up to
 * count bytes from the cable end point into the buffer starting at buf. If
 * count is less than zero, os_bread() may detect the errors. A read() with a
 * count of 0 returns zero and has no other effects.
 * On success, the number of bytes read is returned. It is not an error if this
 * number is smaller than the number of bytes requested.
 * On error, -1 is returned.
 *
 * @u_id:   id of the enty point.
 * @buf:    pointer to the destination buffer.
 * @count:  size of the destination buffer.
 *
 * Return:	the number of bytes read.
 **/
int os_bread(int u_id, char *buf, int count)
{
	struct buf_data_s *b;
	int size, trigger;
	
	/* Entry conditon. */
	OS_TRAP_IF(u_id < 0 || u_id >= BUF_EP_COUNT ||
		   buf == NULL || count < 0);

	/* Test the size of the destination buffer. */
	if (count < 1)
		return 0;
	
	/* Map the ep id to the ep state. */
	b = buf_data[u_id];

	/* Test the ep state. */
	OS_TRAP_IF(b == NULL);

	/* Copy the next queue element. */
	size = buf_q_read(b->in, buf, count);

	/* Test the state of the input buffer*/
	trigger = atomic_exchange(&b->in->trigger, 0);
	if (trigger) {
		/* Trigger the cable driver to invoke the async. read
		 * callback. */
		os_c_aread(b->c_id);
	}

	return size;
}

/**
 * os_bclose() -  close the buffered display entry point.
 *
 * @u_id:  id of the enty point.
 *
 * Return:	None.
 **/
void os_bclose(int u_id)
{
	struct buf_data_s *b;
	
	/* Enter the critical section. */
	os_cs_enter(&buf_list.mutex);

	/* Entry condition. */
	OS_TRAP_IF(u_id < 0 || u_id >= BUF_EP_COUNT);
	
	/* Map the ep id to the ep state. */
	b = buf_data[u_id];

	/* Test the ep state. */
	OS_TRAP_IF(b == NULL);

	/* Release the driver cable entry point. */
	os_c_close(b->c_id);

	/* Free the input queue. */
	buf_q_delete(b->in);
	
	/* Free the output queue. */
	buf_q_delete(b->out);
	
	/* Free the user entry point state. */	
	OS_FREE(buf_data[u_id]);

	/* Leave the critical section. */
	os_cs_leave(&buf_list.mutex);
}

/**
 * os_bopen() - open the display entry point with I/O buffering of the cable
 *  between controller and display e.g.
 *
 * @ep_name:  name of the entry point: "/display" ...
 * @mode:     is not yet used.
 * 
 * Return:	the user entry point id.
 **/
int os_bopen(const char *ep_name, int mode)
{
	struct buf_data_s *b;
	os_aio_cb_t aio;
	const char **n;
	int c_id, i;

	/* Enter the critical section. */
	os_cs_enter(&buf_list.mutex);

	/* Search for the entry point name. */
	for (n = buf_list.name; *n != NULL; n++) {
		/* Test the entry point name. */
		if (os_strcmp(*n, ep_name) == 0)
			break;
	}

	/* Final condition. */
	OS_TRAP_IF(n == NULL);

	/* Create the driver entry point of the shared memory cable. */
	c_id = os_c_open(ep_name, 0);

	/* Allocate the entry point state. */
	for (i = 0; i < BUF_EP_COUNT; i++) {
		if (buf_data[i] == NULL)
			break;
	}

	/* Test the allocation conditon. */
	OS_TRAP_IF(i >= BUF_EP_COUNT);

	b = buf_data[i] = OS_MALLOC(sizeof(struct buf_data_s));
	os_memset(b, 0, sizeof(struct buf_data_s));

	/* Save the entry point identificaton. */
	b->u_id = i;
	b->c_id = c_id;
	b->name = ep_name;

	/* Create the input queue. */
	b->in = buf_q_init(BUF_Q_SIZE);
	
	/* Create the output queue. */
	b->out = buf_q_init(BUF_Q_SIZE);

	/* Use the async. I/O operations of the cable driver. */
	aio.write_cb = buf_write_cb;
	aio.read_cb  = buf_read_cb;
	os_c_action(c_id, &aio);

	/* Leave the critical section. */
	os_cs_leave(&buf_list.mutex);

	return 0;
}

/**
 * os_buf_exit() - test the state of the entry point data.
 *
 * Return:	None.
 **/
void os_buf_exit(void)
{
	int i;
	
	/* Test the state of all user entry point states. */
	for (i = 0; i < BUF_EP_COUNT; i++)
		OS_TRAP_IF(buf_data[i] != NULL);

	/* Release the mutex for the critical section. */
	os_cs_destroy(&buf_list.mutex);
}

/**
 * os_buf_init() - prepare the access to the entry points.
 *
 * Return:	None.
 **/
void os_buf_init(void)
{
	/* Protect the access to the entry point names. */
	os_cs_init(&buf_list.mutex);
}
