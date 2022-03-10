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
/*============================================================================
  LOCAL DATA
  ============================================================================*/
/**
 * buf_list_s - list of all available entry points names.
 *
 * @name:  list of the entry point names.
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
 * @u_id:         user entry point id.
 * @c_id:         cable driver entry point id.
 * @name:         pointer to the entry point name.
 * @in:           input queue, written from the cable driver with buf_write_cb().
 * @out:          output queue, read from the cable driver with buf_read_cb().
 * @in_trigger:   if 1, the driver expects the read trigger.
 * @out_trigger:  if 1, the driver expects the write trigger.
 **/
static struct buf_data_s {
	int    u_id;
	int    c_id;
	const char  *name;
	void  *in;
	void  *out;
	atomic_int  in_trigger;
	atomic_int  out_trigger;
} *buf_data[BUF_EP_COUNT];

/*============================================================================
  LOCAL FUNCTION PROTOTYPES
  ============================================================================*/

/*============================================================================
  LOCAL FUNCTIONS
  ============================================================================*/
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
	rv = os_mq_write(b->in, buf, count);

	/* Test the input state. */
	if (! rv) {
		/* The drive waits for the read trigger. */
		atomic_store(&b->in_trigger, 1);
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
	size = os_mq_read(b->out, buf, count);

	/* Test the output state. */
	if (size < 1) {
		/* The drive waits for the write trigger. */
		atomic_store(&b->out_trigger, 1);
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
	int n;
	
	/* Entry conditon. */
	OS_TRAP_IF(u_id < 0 || u_id >= BUF_EP_COUNT);

	/* Map the ep id to the ep state. */
	b = buf_data[u_id];

	/* Test the ep state. */
	OS_TRAP_IF(b == NULL);

	/* Get the fill level of the output queue. */
	n = os_mq_rmem(b->out);
	
	return n;
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
	int free;
		
	/* Entry conditon. */
	OS_TRAP_IF(u_id < 0 || u_id >= BUF_EP_COUNT);

	/* Map the ep id to the ep state. */
	b = buf_data[u_id];

	/* Test the ep state. */
	OS_TRAP_IF(b == NULL);

	/* Get the size of the free output message buffer for writing. */
	free = os_mq_wmem(b->out);
	
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
 * Return:	the number of bytes written.
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
	rv = os_mq_write(b->out, buf, count);

	/* Test the state of the output buffer. */
	trigger = atomic_exchange(&b->out_trigger, 0);
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
	size = os_mq_read(b->in, buf, count);

	/* Test the state of the input buffer. */
	trigger = atomic_exchange(&b->in_trigger, 0);
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
	void *q;
	
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
	q = b->in;
	b->in = NULL;
	os_mq_delete(q);
	
	/* Free the output queue. */
	q = b->out;
	b->out = NULL;
	os_mq_delete(q);
	
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
	b->in = os_mq_init(BUF_Q_SIZE);
	
	/* Create the output queue. */
	b->out = os_mq_init(BUF_Q_SIZE);

	/* Initialize the driver trigger for reading. */
	atomic_store(&b->in_trigger, 1);

	/* Initialize the driver trigger for writing. */
	atomic_store(&b->out_trigger, 1);

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
