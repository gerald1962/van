// SPDX-License-Identifier: GPL-2.0

/*
 * site - Simultaneous C and N date transer experiments from the c or van
 * to a n - py or tcl - and back.
 *
 * Copyright (C) 2021 Gerald Schueller <gerald.schueller@web.de>
 */

/*============================================================================
  IMPORTED INCLUDE REFERENCES
  ============================================================================*/
#include <unistd.h>  /* Common Unix interfaces: getopt().*/
#include "os.h"      /* Operating system: os_sem_create(). */

/*============================================================================
  EXPORTED INCLUDE REFERENCES
  ============================================================================*/
/*============================================================================
  LOCAL NAME CONSTANTS DEFINITIONS
  ============================================================================*/
/* Prompt of the loop program. */
#define P  "S>"

#define PRIO       OS_THREAD_PRIO_FOREG  /* Thread foreground priority. */
#define Q_SIZE     4                     /* Size of the thread input queue. */
#define MIN_SIZE   1                     /* Minimum of the fill level. */
#define MAX_SIZE   OS_BUF_SIZE           /* Fill level of a transfer buffer. */

#define C_F_CHAR   'd'  /* C fill character. */
#define N_F_CHAR   'u'  /* N fill character. */
#define FINAL_CHAR '#' /* Contents of the final payload. */

/*============================================================================
  MACROS
  ============================================================================*/
/* site trace control */
#define TRACE(info_)  do { \
		if (site_stat.my_trace) \
			printf info_; \
} while (0)

/*============================================================================
  LOCAL TYPE DEFINITIONS
  ============================================================================*/
/*============================================================================
  LOCAL DATA
  ============================================================================*/
/**
 * iot_t - I/O configuration.
 *
 * @IO_SYNC_BL_COPY:  call blocking sync. write and sync. copy read.
 * @IO_SYNC_BL_ZERO:  call blocking sync. write and sync. zero copy read.
 * @IO_SYNC_NB_COPY:  call non blocking sync. write and sync. copy read.
 * @IO_SYNC_NB_ZERO:  call non blocking sync. write and sync. zero copy read.
 * @IO_ASYNC:         perform read and write asynchronously.
 **/
typedef enum {
	IO_SYNC_BL_COPY,
	IO_SYNC_BL_ZERO,
	IO_SYNC_NB_COPY,
	IO_SYNC_NB_ZERO,
	IO_ASYNC
} io_t;

/**
 * ct_t - cable type.
 *
 * @CT_VAN_PY:   van-python cable.
 * @CT_VAN_TCL:  van-tcl cable.
 **/
typedef enum {
	CT_VAN_PY,
	CT_VAN_TCL
} ct_t;

/**
 * cc_t - cable configuration.
 *
 * @type:    van-python or van-tcl cable.
 * @l_name:  c or van shm device name.
 * @f_name:  n - python or tcl - device name.
 **/
typedef struct {
	ct_t  type;
	char *l_name;
	char *f_name;
} cc_t;

/** 
 * site_stat - state of the bidirectional data transfer.
 *
 * @cc:           cable configuration: van-py or van-tcl.
 * @c_io:         ctrl. tech. / van I/O configuration.
 * @n_io:         neighbour - python or tcl - I/O configuration.
 * @c_wr_cycles:  number of the C write cycles.
 * @n_wr_cycles:  number of the N write cycles.
 * @c_buf_size:   fill the C transfer buffer with n characters.
 * @n_buf_size:   fill the N transfer buffer with n characters.
 * @c_fill_char:  C fill character.
 * @n_fill_char:  N fill character.
 * @os_trace:     if 1, activate the OS trace.
 * @my_trace:     if 1, activate the site trace.
 *
 * @c_wr_count:   count the number of the C van write_cb calls.
 * @c_rd_count:   count the number of the C py read_cb calls.
 * @n_rd_count:   count the number of the N van read_cb calls.
 * @n_wr_count:   count the number of the N py write_cb calls.
 *
 * @c_wr_done:    if 1, the van C writer thread has done its job.
 * @c_rd_done:    if 1, the py C reader has finished the data receive.
 * @n_rd_done:    if 1, the van N read thread has terminted the data analyis.
 * @n_wr_done:    if 1, the py N writer has done the data generation.
 * @suspend:      suspend the main process while the test is running.
 * @c_id:         id of the ctrl. tech. or van shm device.
 * @n_id:         id of the py or tcl shm deice.
 * @n_writer:     address of the ctrl. tech. writer thread.
 * @c_reader:     address of the neighbour reader thread.
 * @n_reader:     address of the ctrl. tech. reader thread.
 * @n_writer:     address of the neighbour writer thread.
 * @ctrl_tech:    address of the non blocking sync. ctrl. tech. thread.
 * @neighbour:    address of the non blocking sync. neighbour thread.
 *
 * @c_wr_b:       C write buffer.
 * @c_rd_b:       C read buffer.
 * @n_rd_b:       N read buffer.
 * @n_wr_b:       N write buffer.
 * @c_ref_b:      reference buffer to detect C transfer errors.
 * @n_ref_b:      reference buffer to detect N transfer errors.
 **/
static struct site_stat_s {
	cc_t  cc;
	io_t  c_io;
	io_t  n_io;
	int   c_wr_cycles;
	int   n_wr_cycles;
	int   c_buf_size;
	int   n_buf_size;
	int   c_fill_char;
	int   n_fill_char;
	int   os_trace;
	int   my_trace;

	int   c_wr_count;
	int   c_rd_count;
	int   n_rd_count;
	int   n_wr_count;

	atomic_int  c_wr_done;
	atomic_int  c_rd_done;
	atomic_int  n_rd_done;
	atomic_int  n_wr_done;
	sem_t   suspend;
	int     c_id;
	int     n_id;
	
	void   *c_writer;
	void   *c_reader;
	void   *n_reader;
	void   *n_writer;
	void   *ctrl_tech;
	void   *neighbour;
	
	char    c_wr_b[OS_BUF_SIZE];
	char    c_rd_b[OS_BUF_SIZE];
	char    n_rd_b[OS_BUF_SIZE];
	char    n_wr_b[OS_BUF_SIZE];
	char    c_ref_b[OS_BUF_SIZE];
	char    n_ref_b[OS_BUF_SIZE];
} site_stat;

/*============================================================================
  LOCAL FUNCTION PROTOTYPES
  ============================================================================*/
static char *io_to_string(io_t io);

/*============================================================================
  LOCAL FUNCTIONS
  ============================================================================*/
/**
 * site_resume() - resume the main process.
 *
 * Return:	None.
 **/
void site_resume(void)
{
	TRACE(("%s [p:nain,s:suspended,m:resume]\n", P));

	/* Resume the main program. */
	os_sem_release(&site_stat.suspend);
}

/**
 * site_nb_sync_n_write() - the neighbour sends data with the non
 * blocking synchronous write operation to the ctrl. tech.
 *
 * Return:	0, if the write operation is complete.
 **/
static int site_nb_sync_n_write(void)
{
	struct site_stat_s *s;
	char *mode, *buf;
	int done, size, n;

	/* Get the pointer to the site state. */
	s = &site_stat;

	/* Test the write state. */
	done = atomic_load(&s->n_wr_done);
	if (done)
		return 0;

	/* Test the write cycle counter of the neighbour. */
	if (s->n_wr_cycles < 1) {
		/* Resume the main process. */
		atomic_store(&s->n_wr_done, 1);
		site_resume();
		return 0;
	}

	/* Test the cycle counter. */
	if (s->n_wr_count > s->n_wr_cycles) {
		/* Resume the main process. */
		atomic_store(&s->n_wr_done, 1);
		site_resume();
		return 0;
	}
	
	/* Get the I/O mode. */
	mode = io_to_string(s->n_io);

	/* Initialize the N write buffer. */
	buf  = s->n_wr_b;
	size = s->n_buf_size;
	os_memset(buf, 0, OS_BUF_SIZE);
	os_memset(buf, s->n_fill_char, size);

	/* Test the cycle counter. */
	if (s->n_wr_count == s->n_wr_cycles)
		*buf = FINAL_CHAR;

	/* Send the buffer. */
	n = os_write(s->n_id, buf, size);
	if (n < 1)
		return 1;

	OS_TRAP_IF(n != size);
	TRACE(("neighbour> sent: [i/o:%s, c:%d, b:\"%c...\", s:%d]\n",
	       mode, s->n_wr_count, *buf, size));
	
	/* Increment the cycle counter. */
	s->n_wr_count++;
	return 1;
}

/**
 * site_nb_sync_n_read() - the neighbour thread analyzes data from the ctrl_tech
 * with the sync zero copy read interface or with the copy read interface.
 *
 * Return:	0, if the read operation is complete.
 **/
static int site_nb_sync_n_read(void)
{
	struct site_stat_s *s;
	char *zbuf, *b, *mode;
	int done, size, n, stat;

	/* Get the pointer to the site state. */
	s = &site_stat;
	
	/* Test the read state. */
	done = atomic_load(&s->n_rd_done);
	if (done)
		return 0;

	/* Test the C activity. */
	if (s->c_wr_cycles < 1) {
		/* Resume the main process. */
		atomic_store(&s->n_rd_done, 1);
		site_resume();
		return 0;
	}

	/* Initialize the local state. */
	size = s->c_buf_size;

	/* Get the I/O mode. */
	mode = io_to_string(s->n_io);
	
	/* Test the read mode. */
	if (s->n_io == IO_SYNC_NB_COPY) {
		/* Receive the C paylaod with the copy read interface. */
		os_memset(s->n_rd_b, 0, OS_BUF_SIZE);

		/* Wait for data from the ctrl_tech. */
		n = os_read(s->n_id, s->n_rd_b, OS_BUF_SIZE);
		if (n < 1)
			return 1;
			
		OS_TRAP_IF(n != size);
		b = s->n_rd_b;
	}
	else {
		/* Receive the N paylaod with the zero copy read interface. */
		zbuf = NULL;
		
		/* Wait for data from the ctrl_tech. */
		n = os_zread(s->n_id, &zbuf, OS_BUF_SIZE);
		if (n < 1)
			return 1;
			
		OS_TRAP_IF(zbuf == NULL || n != size);
		b = zbuf;
	}

	/* Test the end condition of the test. */
	if (*b == FINAL_CHAR) {
		TRACE(("neighbour> received: [i/o:%s, c:%d, b:\"%c..\", s:%d]\n",
		       mode, s->n_rd_count, *b, n));

		/* Test the read mode. */
		if (s->n_io == IO_SYNC_NB_ZERO) {
			/* Release the pending N buffer. */
			n = os_zread(s->n_id, NULL, 0);
			OS_TRAP_IF(n > 0);
		}

		/* Test the N read counter */
		OS_TRAP_IF(s->n_rd_count != s->c_wr_cycles);
		
		/* Resume the main process. */
		atomic_store(&s->n_rd_done, 1);
		site_resume();
		return 0;
	}
	
	/* Test the N read counter */
	OS_TRAP_IF(s->n_rd_count >= s->c_wr_cycles);

	/* Update the N read counter. */
	s->n_rd_count++;

	TRACE(("neighbour> received: [i/o:%s, c:%d, b:\"%c...\", s:%d]\n",
	       mode, s->n_rd_count, *b, n));
		
	/* Test the contents of the C buffer. */
	stat = os_memcmp(b, s->c_ref_b, n);
	OS_TRAP_IF(stat != 0);
	return 1;
}

/**
 * site_nb_sync_n_exec() - the neighbour of the ctrl. tech. sends and receives data with the non
 * blocking synchronous operations.
 *
 * @msg:  addess of the generic input message.
 *
 * Return:	None.
 **/
static void site_nb_sync_n_exec(os_queue_elem_t *msg)
{
	int busy_read, busy_write;

	/* Initialize the return values. */
	busy_read  = 1;
	busy_write = 1;
	
	/* The neighbour thread analyzes data from the ctrl_tech with the non
	 * blocking sync zero copy or with the copy read interface or sends data
	 * with the non blocking write operation. */
	while (busy_read || busy_write) {
		busy_read  = site_nb_sync_n_read();
		busy_write = site_nb_sync_n_write();
	}
}

/**
 * site_nb_sync_c_read() - the ctrl. tech. thread analyzes data from the ctrl_tech
 * with the non blocking sync zero copy or with the copy read interface.
 *
 * Return:	0, if the read operation is complete.
 **/
static int site_nb_sync_c_read(void)
{
	struct site_stat_s *s;
	char *zbuf, *b, *mode;
	int done, size, n, stat;

	/* Get the pointer to the site state. */
	s = &site_stat;
	
	/* Test the read state. */
	done = atomic_load(&s->c_rd_done);
	if (done)
		return 0;

	/* Test the N activity. */
	if (s->n_wr_cycles < 1) {
		/* Resume the main process. */
		atomic_store(&s->c_rd_done, 1);
		site_resume();
		return 0;
	}

	/* Initialize the local state. */
	size = s->n_buf_size;

	/* Get the I/O mode. */
	mode = io_to_string(s->c_io);
	
	/* Test the read mode. */
	if (s->c_io == IO_SYNC_NB_COPY) {
		/* Receive the N paylaod with the copy read interface. */
		os_memset(s->c_rd_b, 0, OS_BUF_SIZE);

		/* Wait for data from the neighbour. */
		n = os_read(s->c_id, s->c_rd_b, OS_BUF_SIZE);
		if (n < 1)
			return 1;
			
		OS_TRAP_IF(n != size);
		b = s->c_rd_b;
	}
	else {
		/* Receive the N paylaod with the zero copy read interface. */
		zbuf = NULL;
		
		/* Wait for data from the neighbour. */
		n = os_zread(s->c_id, &zbuf, OS_BUF_SIZE);
		if (n < 1)
			return 1;
			
		OS_TRAP_IF(zbuf == NULL || n != size);
		b = zbuf;
	}

	/* Test the end condition of the test. */
	if (*b == FINAL_CHAR) {
		TRACE(("ctrl_tech> received: [i/o:%s, c:%d, b:\"%c...\", s:%d]\n",
		       mode, s->c_rd_count, *b, n));
		
		/* Test the read mode. */
		if (s->c_io == IO_SYNC_NB_ZERO) {
			/* Release the pending C buffer. */
			n = os_zread(s->c_id, NULL, 0);
			OS_TRAP_IF(n > 0);
		}

		/* Test the C read counter */
		OS_TRAP_IF(s->c_rd_count != s->n_wr_cycles);
		
		/* Resume the main process. */
		atomic_store(&s->c_rd_done, 1);
		site_resume();
		return 0;
	}
	
	/* Test the C read counter */
	OS_TRAP_IF(s->c_rd_count >= s->n_wr_cycles);

	/* Update the C read counter. */
	s->c_rd_count++;

	TRACE(("ctrl_tech> received: [i/o:%s, c:%d, b:\"%c...\", s:%d]\n",
	       mode, s->c_rd_count, *b, n));
		
	/* Test the contents of the N buffer. */
	stat = os_memcmp(b, s->n_ref_b, n);
	OS_TRAP_IF(stat != 0);
	return 1;
}

/**
 * site_nb_sync_c_write() - the ctrl. tech. sends data with the non
 * blocking synchronous write operation to the neighbour: either python or tcl/tk.
 *
 * Return:	0, if the write operation is complete.
 **/
static int site_nb_sync_c_write(void)
{
	struct site_stat_s *s;
	char *mode, *buf;
	int done, size, n;

	/* Get the pointer to the site state. */
	s = &site_stat;

	/* Test the write state. */
	done = atomic_load(&s->c_wr_done);
	if (done)
		return 0;

	/* Test the write cycle counter of the ctrl. tech. */
	if (s->c_wr_cycles < 1) {
		/* Resume the main process. */
		atomic_store(&s->c_wr_done, 1);
		site_resume();
		return 0;
	}

	/* Test the cycle counter. */
	if (s->c_wr_count > s->c_wr_cycles) {
		/* Resume the main process. */
		atomic_store(&s->c_wr_done, 1);
		site_resume();
		return 0;
	}
	
	/* Get the I/O mode. */
	mode = io_to_string(s->c_io);

	/* Initialize the C write buffer. */
	buf  = s->c_wr_b;
	size = s->c_buf_size;
	os_memset(buf, 0, OS_BUF_SIZE);
	os_memset(buf, s->c_fill_char, size);

	/* Test the cycle counter. */
	if (s->c_wr_count == s->c_wr_cycles)
		*buf = FINAL_CHAR;

	/* Send the buffer. */
	n = os_write(s->c_id, buf, size);
	if (n < 1)
		return 1;

	OS_TRAP_IF(n != size);
	TRACE(("ctrl_tech> sent: [i/o:%s, c:%d, b:\"%c...\", s:%d]\n",
	       mode, s->c_wr_count, *buf, size));
	
	/* Increment the cycle counter. */
	s->c_wr_count++;
	return 1;
}

/**
 * site_nb_sync_c_exec() - the ctrl. tech. sends and receives data with the non
 * blocking synchronous operations to the neighbour: either python or tcl/tk.
 *
 * @msg:  addess of the generic input message.
 *
 * Return:	None.
 **/
static void site_nb_sync_c_exec(os_queue_elem_t *msg)
{
	int busy_write, busy_read;

	/* Initialize the return values. */
	busy_write = 1;
	busy_read  = 1;

	/* The C ctrl_tech thread generates data for the py or tcl neighbour or
	 * receives data from the neighbour. */
	while (busy_write || busy_read) {
		busy_write = site_nb_sync_c_write();
		busy_read = site_nb_sync_c_read();
	}
}

/**
 * site_aio_n_rd_cb() - the neighbour analyzes data from the ctrl_tech.
 *
 * @dev_id:  py device id.
 * @buf:     pointer to the shared memory buffer.
 * @count:   size of the C payload.
 *
 * Return:	the number of the consumed characters.
 **/
static int site_aio_n_rd_cb(int dev_id, char *buf, int count)
{
	struct site_stat_s *s;
	int stat;

	/* Get the pointer to the site state. */
	s = &site_stat;

	/* Entry condition. */
	OS_TRAP_IF(s->n_id != dev_id || buf == NULL || count != s->c_buf_size);

	/* Test the end condition of the test. */
	if (*buf == FINAL_CHAR) {
		TRACE(("c_reader> received: [i/o:a, c:%d, b:\"%c...\", s:%d]\n",
		       s->n_rd_count, *buf, count));
		
		/* Test the C read counter */
		OS_TRAP_IF(s->n_rd_count != s->c_wr_cycles);
		
		/* Resume the main process. */
		atomic_store(&s->n_rd_done, 1);
		site_resume();

		return count;
	}
	
	/* Test the C read counter */
	OS_TRAP_IF(s->n_rd_count >= s->c_wr_cycles);
		
	TRACE(("c_reader> received: [i/o:a, c:%d, b:\"%c...\", s:%d]\n",
	       s->n_rd_count, *buf, count));
		
	/* Test the contents of the C buffer. */
	stat = os_memcmp(buf, s->c_ref_b, count);
	OS_TRAP_IF(stat != 0);

	/* Update the C read counter. */
	s->n_rd_count++;
	
	return count;
}

/**
 * site_aio_c_rd_exec() - the ctrl_tech thread is inactiv because of the async.
 * I/O configuration.
 *
 * @msg:  addess of the generic input message.
 *
 * Return:	None.
 **/
static void site_aio_c_rd_exec(os_queue_elem_t *msg)
{
	struct site_stat_s *s;
	
	/* Get the pointer to the site state. */
	s   = &site_stat;
	
	/* Test the C counter. */
	if (s->n_wr_cycles < 1) {
		/* Resume the main process. */
		atomic_store(&s->c_rd_done, 1);
		site_resume();
		return;
	}
	
	TRACE(("c_reader> [i/o:a, s:ready, o:inactive]\n"));

	/* Trigger the py interrupt hancer to invoke the async. read and write
	 *  callback. */
	os_aio_read(s->c_id);
	os_aio_write(s->c_id);
}

/**
 * site_aio_n_wr_cb() - the py irq thread requests N data if available.
 *
 * @dev_id:  py device id.
 * @buf:     pointer to the shared memory buffer.
 * @count:   size of the shared memory buffer.
 *
 * Return:	number of the saved characters.
 **/
static int site_aio_n_wr_cb(int dev_id, char *buf, int count)
{
	struct site_stat_s *s;

	/* Get the pointer to the site state. */
	s = &site_stat;

	/* Entry condition. */
	OS_TRAP_IF(s->n_id != dev_id || buf == NULL || count != OS_BUF_SIZE);

	/* Test the generation status. */
	if (s->n_wr_cycles < 1)
		return 0;
	
	/* Test the cycle counter. */
	if (s->n_wr_count > s->n_wr_cycles) {
		/* Resume the main process. */
		atomic_store(&s->n_wr_done, 1);
		site_resume();
		return 0;
	}
	
	/* Fill the send buffer. */
	os_memset(buf, 0, OS_BUF_SIZE);
	os_memset(buf, s->n_fill_char, s->n_buf_size);
	
	/* Test the cycle counter. */
	if (s->n_wr_count == s->n_wr_cycles)
		*buf = FINAL_CHAR;

	TRACE(("n_writer> sent: [i/o:a, c:%d, b:\"%c...\", s:%d]\n",
	       s->n_wr_count, *buf, s->n_buf_size));

	/* Increment the cycle counter. */
	s->n_wr_count++;
	
	return s->n_buf_size;
}

/**
 * site_aio_n_wr_exec() - the python n_writer thread triggers the python
 * driver to start the N transfer.
 *
 * @msg:  addess of the generic input message.
 *
 * Return:	None.
 **/
static void site_aio_n_wr_exec(os_queue_elem_t *msg)
{
	struct site_stat_s *s;
	
	/* Get the pointer to the site state. */
	s = &site_stat;
	
	/* Test the N counter. */
	if (s->n_wr_cycles < 1) {
		/* Resume the main process. */
		atomic_store(&s->n_wr_done, 1);
		site_resume();
		return;
	}

	TRACE(("n_writer> [i/o:a, s:ready, o:trigger]\n"));

	/* Trigger the py interrupt hancer to invoke the async. write and read
	 *  callback. */
	os_aio_write(s->n_id);
	os_aio_read(s->n_id);
}

/**
 * site_aio_c_rd_cb() - the controller thread analyzes data from the neighbour.
 *
 * @dev_id:  van device id.
 * @buf:     pointer to the shared memory buffer.
 * @count:   size of the C payload.
 *
 * Return:	the number of the consumed characters.
 **/
static int site_aio_c_rd_cb(int dev_id, char *buf, int count)
{
	struct site_stat_s *s;
	int stat;

	/* Get the pointer to the site state. */
	s = &site_stat;

	/* Entry condition. */
	OS_TRAP_IF(s->c_id != dev_id || buf == NULL || count != s->n_buf_size);

	/* Test the end condition of the test. */
	if (*buf == FINAL_CHAR) {
		TRACE(("n_reader> received: [i/o:a, c:%d, b:\"%c...\", s:%d]\n",
		       s->c_rd_count, *buf, count));
		
		/* Test the C read counter */
		OS_TRAP_IF(s->c_rd_count != s->n_wr_cycles);
		
		/* Resume the main process. */
		atomic_store(&s->c_rd_done, 1);
		site_resume();

		return count;
	}
	
	/* Test the C read counter */
	OS_TRAP_IF(s->c_rd_count >= s->n_wr_cycles);
		
	TRACE(("n_reader> received: [i/o:a, c:%d, b:\"%c...\", s:%d]\n",
	       s->c_rd_count, *buf, count));
		
	/* Test the contents of the N buffer. */
	stat = os_memcmp(buf, s->n_ref_b, count);
	OS_TRAP_IF(stat != 0);

	/* Update the C read counter. */
	s->c_rd_count++;
	
	return count;
}

/**
 * site_aio_n_rd_exec() - the neighbour thread is inactiv because of the
 * async. I/O configuration.
 *
 * @msg:  addess of the generic input message.
 *
 * Return:	None.
 **/
static void site_aio_n_rd_exec(os_queue_elem_t *msg)
{
	struct site_stat_s *s;
	
	/* Get the pointer to the site state. */
	s = &site_stat;
	
	/* Test the C counter. */
	if (s->c_wr_cycles < 1) {
		/* Resume the main process. */
		atomic_store(&s->n_rd_done, 1);
		site_resume();
		return;
	}
	
	TRACE(("n_reader> [i/o:a, s:ready, o:inactive]\n"));

	/* Trigger the van interrupt hancer to invoke the async. write and read
	 * callback. */
	os_aio_write(s->n_id);
	os_aio_read(s->n_id);
}

/**
 * site_aio_c_wr_cb() - the van irq thread requests C data if available.
 *
 * @dev_id:  van device id.
 * @buf:     pointer to the shared memory buffer.
 * @count:   size of the shared memory buffer.
 *
 * Return:	number of the saved characters.
 **/
static int site_aio_c_wr_cb(int dev_id, char *buf, int count)
{
	struct site_stat_s *s;

	/* Get the pointer to the site state. */
	s = &site_stat;

	/* Entry condition. */
	OS_TRAP_IF(s->c_id != dev_id || buf == NULL || count != OS_BUF_SIZE);
	
	/* Test the generation status. */
	if (s->c_wr_cycles < 1)
		return 0;

	/* Test the cycle counter. */
	if (s->c_wr_count > s->c_wr_cycles) {
		/* Resume the main process. */
		atomic_store(&s->c_wr_done, 1);
		site_resume();
		return 0;
	}
	
	/* Fill the send buffer. */
	os_memset(buf, 0, OS_BUF_SIZE);
	os_memset(buf, s->c_fill_char, s->c_buf_size);
	
	/* Test the cycle counter. */
	if (s->c_wr_count == s->c_wr_cycles)
		*buf = FINAL_CHAR;

	TRACE(("c_writer> sent: [i/o:a, c:%d, b:\"%c...\", s:%d]\n",
	       s->c_wr_count, *buf, s->c_buf_size));

	/* Increment the cycle counter. */
	s->c_wr_count++;
	
	return s->c_buf_size;
}

/**
 * site_aio_c_wr_exec() - the van c_writer thread triggers the van driver
 * to start the C transfer.
 *
 * @msg:  addess of the generic input message.
 *
 * Return:	None.
 **/
static void site_aio_c_wr_exec(os_queue_elem_t *msg)
{
	struct site_stat_s *s;
	
	/* Get the pointer to the site state. */
	s = &site_stat;

	/* Test the C counter. */
	if (s->c_wr_cycles < 1) {
		/* Resume the main process. */
		atomic_store(&s->c_wr_done, 1);
		site_resume();
		return;
	}
	
	TRACE(("c_writer> [i/o:a, s:ready, o:trigger]\n"));

	/* Trigger the van interrupt hancer to invoke the async. write and read
	 * callback. */
	os_aio_write(s->c_id);
	os_aio_read(s->c_id);
}

/**
 * site_sync_n_wr_exec() - the py n_writer thread generates N test data for
 * van.
 *
 * @msg:  addess of the generic input message.
 *
 * Return:	None.
 **/
static void site_sync_n_wr_exec(os_queue_elem_t *msg)
{
	struct site_stat_s *s;
	char *buf;
	int i;

	/* Get the pointer to the site state. */
	s   = &site_stat;
	buf = s->n_wr_b;
	
	/* Test the N counter. */
	if (s->n_wr_cycles < 1)
		goto end;
	
	TRACE(("n_writer> [i/o:s, s:ready, o:generate]\n"));

	/* Initialize the N write buffer. */
	os_memset(buf, 0, OS_BUF_SIZE);
	os_memset(buf, s->n_fill_char, s->n_buf_size);

	/* The py n_writer thread generates data for van with the sync write
	 * interface. */
	for (i = 0; i < s->n_wr_cycles; i++) {
		os_write(s->n_id, buf, s->n_buf_size);
		TRACE(("n_writer> sent: [i/o:s, c:%d, b:\"%c...\", s:%d]\n", i, *buf, s->n_buf_size));
	}

	/* Send the final char to van. */
	*buf = FINAL_CHAR;
	os_write(s->n_id, buf, s->n_buf_size);
	TRACE(("n_writer> sent: [i/o:s, c:%d, b:\"%c\", s:%d]\n", i, *buf, s->n_buf_size));

end:
	/* Resume the main process. */
	atomic_store(&s->n_wr_done, 1);
	site_resume();
}

/**
 * site_sync_c_rd_exec() - the van n_reader thread analyzes the received N test
 * data from the neighbour.
 *
 * @msg:  addess of the generic input message.
 *
 * Return:	None.
 **/
static void site_sync_c_rd_exec(os_queue_elem_t *msg)
{
	struct site_stat_s *s;
	char *zbuf, *buf, *b;
	int i, n, stat;

	/* Get the pointer to the site state. */
	s    = &site_stat;
	zbuf = NULL;
	buf  = s->c_rd_b;

	/* Test the N counter. */
	if (s->n_wr_cycles < 1)
		goto l_end;
	
	TRACE(("c_reader> [i/o:s, s:ready, o:analyze]\n"));

	/* The van n_writer thread analyzes data from py with the sync zero
	 * copy read interface or with the copy read interface. */
	for(i = 0;; i++) {
		/* Test the read mode. */
		if (s->c_io == IO_SYNC_BL_COPY) {
			/* Receive the N paylaod with the copy read interface. */
			os_memset(buf, 0, OS_BUF_SIZE);
			n = os_read(s->c_id, buf, OS_BUF_SIZE);
			OS_TRAP_IF(n != s->n_buf_size);
			b = buf;
		}
		else {
			/* Receive the N paylaod with the zero copy read interface. */
			zbuf = NULL;
			n = os_zread(s->c_id, &zbuf, OS_BUF_SIZE);
			OS_TRAP_IF(zbuf == NULL || n != s->n_buf_size);
			b = zbuf;
		}

		TRACE(("c_reader> received: [i/o:s, c:%d, b:\"%c...\", s:%d]\n", i, *b, n));
			
		/* Test the end condition of the test. */
		if (*b == FINAL_CHAR)
			break;

		/* Test the contents of the N buffer. */
		stat = os_memcmp(b, s->n_ref_b, n);
		OS_TRAP_IF(stat != 0 || n != s->n_buf_size);
	}

	/* Test the read mode. */
	if (s->c_io == IO_SYNC_BL_ZERO) {
		/* Release the pending N buffer. */
		n = os_zread(s->c_id, NULL, 0);
		OS_TRAP_IF(n > 0);
	}

	/* Test the N cycle counter */
	OS_TRAP_IF(i != s->n_wr_cycles);

l_end:
	/* Resume the main process. */
	atomic_store(&s->c_rd_done, 1);
	site_resume();
}

/**
 * site_sync_n_rd_exec() - the py c_reader thread analyzes the received C test
 * data from van.
 *
 * @msg:  addess of the generic input message.
 *
 * Return:	None.
 **/
static void site_sync_n_rd_exec(os_queue_elem_t *msg)
{
	struct site_stat_s *s;
	char *zbuf, *buf, *b;
	int i, n, stat;

	/* Get the pointer to the site state. */
	s    = &site_stat;
	zbuf = NULL;
	buf  = s->n_rd_b;
	
	/* Test the C counter. */
	if (s->c_wr_cycles < 1)
		goto end;

	TRACE(("c_reader> [i/o:s, s:ready, o:analyze]\n"));

	/* The py c_writer thread analyzes data from van with the sync zero
	 * copy read interface or with the copy read interface. */
	for(i = 0;; i++) {
		/* Test the read mode. */
		if (s->n_io == IO_SYNC_BL_COPY) {
			/* Receive the C paylaod with the copy read interface. */
			os_memset(buf, 0, OS_BUF_SIZE);
			n = os_read(s->n_id, buf, OS_BUF_SIZE);
			OS_TRAP_IF(n != s->c_buf_size);
			b = buf;
		}
		else {
			/* Receive the C paylaod with the zero copy read interface. */
			zbuf = NULL;
			n = os_zread(s->n_id, &zbuf, OS_BUF_SIZE);
			OS_TRAP_IF(zbuf == NULL || n != s->c_buf_size);
			b = zbuf;
		}

		/* Test the end condition of the test. */
		if (*b == FINAL_CHAR) {
			TRACE(("c_reader> received: [i/o:s, c:%d, b:\"%c...\", s:%d]\n", i, *b, n));
			break;
		}
		
		TRACE(("c_reader> received: [i/o:s, c:%d, b:\"%c...\", s:%d]\n", i, *b, n));

		/* Test the contents of the C buffer. */
		stat = os_memcmp(b, s->c_ref_b, n);
		OS_TRAP_IF(stat != 0 || n != s->c_buf_size);
	}

	/* Test the read mode. */
	if (s->n_io == IO_SYNC_BL_ZERO) {
		/* Release the pending C buffer. */
		n = os_zread(s->n_id, NULL, 0);
		OS_TRAP_IF(n > 0);
	}

	/* Test the C cycle counter */
	OS_TRAP_IF(i != s->c_wr_cycles);

end:
	/* Resume the main process. */
	atomic_store(&s->n_rd_done, 1);
	site_resume();
}

/**
 * site_sync_c_wr_exec() - the van c_writer thread generates C test data for
 * py.
 *
 * @msg:  addess of the generic input message.
 *
 * Return:	None.
 **/
static void site_sync_c_wr_exec(os_queue_elem_t *msg)
{
	struct site_stat_s *s;
	char *buf;
	int i;
	
	/* Get the pointer to the site state. */
	s = &site_stat;
	
	/* Test the C counter. */
	if (s->c_wr_cycles < 1)
		goto l_end;
	
	TRACE(("c_writer> [i/o:s, s:ready, o:generate]\n"));

	/* Initialize the C write buffer. */
	buf = s->c_wr_b;
	os_memset(buf, 0, OS_BUF_SIZE);
	os_memset(buf, s->c_fill_char, s->c_buf_size);

	/* The C c_writer thread generates data for py or tcl neighbour with the
	 * sync write interface. */
	for (i = 0; i < s->c_wr_cycles; i++) {
		os_write(s->c_id, buf, s->c_buf_size);
		
		TRACE(("c_writer> sent: [i/o:s, c:%d, b:\"%c...\", s:%d]\n", i, *buf, s->c_buf_size));
	}

	/* Send the final char to py. */
	*buf = FINAL_CHAR;
	os_write(s->c_id, buf, s->c_buf_size);
	TRACE(("c_writer> sent: [i/o:s, c:%d, b:\"%c\", s:%d]\n", i, *buf, s->c_buf_size));

l_end:
	/* Resume the main process. */
	atomic_store(&s->c_wr_done, 1);
	site_resume();
}

/**
 * site_cleanup() - free all borrowed resources for the data transfer experiments.
 *
 * Return:	None.
 **/
static void site_cleanup(void)
{
	struct site_stat_s *s;
	
	TRACE(("%s [p:main,s:ready,o:cleanup]\n", P));
	
	/* Get the pointer to the site state. */
	s = &site_stat;
	
	/* Remove all test threads. */
	os_thread_destroy(s->c_writer);
	os_thread_destroy(s->c_reader);
	os_thread_destroy(s->n_reader);
	os_thread_destroy(s->n_writer);
	os_thread_destroy(s->ctrl_tech);
	os_thread_destroy(s->neighbour);
	
	/* Remove the n and the c shm device. */
	os_close(s->n_id);	
	os_close(s->c_id);	

	/* Release the control semaphore for the main process. */
	os_sem_delete(&s->suspend);

	/* Release the OS resources. */
	os_exit();
}

/**
 * site_wait() - the main process is waiting for the end of the data transfer.
 *
 * Return:	None.
 **/
static void site_wait(void)
{
	struct site_stat_s *s;
	int c_wr_done, c_rd_done, n_wr_done, n_rd_done;
	
	/* Get the pointer to the site state. */
	s = &site_stat;
	
	/* Test the status for the data transfer threads. */
	for(;;) {
		TRACE(("%s [p:main,s:wait,o:suspend]\n", P));
		
		/* Suspend the main process. */
		os_sem_wait(&s->suspend);
		TRACE(("%s [p:main,s:test,o:resume]\n", P));
		
		/* Copy the status of all data transfer threads. */
		c_wr_done = atomic_load(&s->c_wr_done);
		c_rd_done = atomic_load(&s->c_rd_done);
		n_wr_done = atomic_load(&s->n_wr_done);
		n_rd_done = atomic_load(&s->n_rd_done);
		
		/* Test the status of all data transfer threads. */
		if (c_wr_done && c_rd_done && n_wr_done && n_rd_done)
			break;
	}
}

/**
 * site_init() - lend the test resources for a short time.
 *
 * Return:	None.
 **/
static void site_init(void)
{
	struct site_stat_s *s;
	os_queue_elem_t msg;
	os_aio_cb_t aio;

	TRACE(("%s [p:main,s:boot,o:init]\n", P));

	/* Get the pointer to the site state. */
	s = &site_stat;
	
	/* Fill the C and N reference buffer. */
	os_memset(s->c_ref_b, s->c_fill_char, s->c_buf_size);
	os_memset(s->n_ref_b, s->n_fill_char, s->n_buf_size);
	
	/* Initialize the operating system. */
	os_init(1);

	/* Configure the OS trace. */
	os_trace_button(s->os_trace);

	/* Install the cable from the ctrl. tech. to the neighbour. */
	if (s->c_io == IO_SYNC_NB_COPY || s->c_io == IO_SYNC_NB_ZERO)
		s->c_id = os_open(s->cc.l_name, O_NBLOCK);
	else
		s->c_id = os_open(s->cc.l_name, 0);
		
	if (s->n_io == IO_SYNC_NB_COPY || s->n_io == IO_SYNC_NB_ZERO)
		s->n_id  = os_open(s->cc.f_name, O_NBLOCK);
	else
		s->n_id  = os_open(s->cc.f_name, 0);
	
	/* Install all test threads. */
	s->c_writer = os_thread_create("c_writer", PRIO, Q_SIZE);
	s->c_reader = os_thread_create("c_reader", PRIO, Q_SIZE);
	s->n_reader = os_thread_create("n_reader", PRIO, Q_SIZE);
	s->n_writer = os_thread_create("n_writer", PRIO, Q_SIZE);

	/* Syncronous non blocking I/O threads. */
	s->ctrl_tech = os_thread_create("ctrl_tech", PRIO, Q_SIZE);
	s->neighbour = os_thread_create("neighbour", PRIO, Q_SIZE);

	/* Control the lifetime of the loop test. */
	os_sem_init(&s->suspend, 0);

	/* Activate all data transfer threads. */
	os_memset(&msg, 0, sizeof(msg));

	/* Test the van I/O configuration of the the ctrl. tech. */
	switch(s->c_io) {
	case IO_SYNC_BL_COPY:
	case IO_SYNC_BL_ZERO:
		msg.cb = site_sync_c_wr_exec;
		OS_SEND(s->c_writer, &msg, sizeof(msg));
	
		msg.cb = site_sync_c_rd_exec;
		OS_SEND(s->c_reader, &msg, sizeof(msg));
		break;
		
	case IO_SYNC_NB_COPY:
	case IO_SYNC_NB_ZERO:
		msg.cb = site_nb_sync_c_exec;
		OS_SEND(s->ctrl_tech, &msg, sizeof(msg));
		break;

	default:
		/* Install the van read and write callback for the asynchronous actions. */
		aio.write_cb = site_aio_c_wr_cb;
		aio.read_cb  = site_aio_c_rd_cb;
		os_aio_action(s->c_id, &aio);

		msg.cb = site_aio_c_wr_exec;
		OS_SEND(s->c_writer, &msg, sizeof(msg));
	
		msg.cb = site_aio_c_rd_exec;
		OS_SEND(s->c_reader, &msg, sizeof(msg));
		break;
	}

	/* Test the I/O configuration of the ctrl. tech. neighbour. */
	switch (s->n_io) {
	case IO_SYNC_BL_COPY:
	case IO_SYNC_BL_ZERO:
		msg.cb = site_sync_n_rd_exec;
		OS_SEND(s->n_reader, &msg, sizeof(msg));
	
		msg.cb = site_sync_n_wr_exec;
		OS_SEND(s->n_writer, &msg, sizeof(msg));
		break;
		
	case IO_SYNC_NB_COPY:
	case IO_SYNC_NB_ZERO:
		msg.cb = site_nb_sync_n_exec;
		OS_SEND(s->neighbour, &msg, sizeof(msg));
		break;

	default:
		/* Install the python read and write callback for the asynchronous actions. */
		aio.write_cb = site_aio_n_wr_cb;
		aio.read_cb  = site_aio_n_rd_cb;
		os_aio_action(s->n_id, &aio);

		msg.cb = site_aio_n_wr_exec;
		OS_SEND(s->n_writer, &msg, sizeof(msg));
		
		msg.cb = site_aio_n_rd_exec;
		OS_SEND(s->n_reader, &msg, sizeof(msg));	
		break;
	}
	
	/* Wait for the resume trigger. */
	site_wait();
}

/**
 * io_to_string() - convert the I/O configuration to string.
 *
 * @io:  van or python I/O configuration.
 *
 * Return:	the I/O string.
 **/
static char *io_to_string(io_t io)
{
	switch(io) {
	case IO_SYNC_BL_COPY: return "bc";
	case IO_SYNC_BL_ZERO: return "bz";
	case IO_SYNC_NB_COPY: return "nc";
	case IO_SYNC_NB_ZERO: return "nz";
	default:              return "a";
	}
}

/**
 * ct_to_string() - convert the cable type to string.
 *
 * Return:	the cable type string.
 **/
static char *ct_to_string(void)
{
	switch(site_stat.cc.type) {
	case CT_VAN_PY: return "p";
	default:        return "t";
	}
}

/**
 * site_usage() - provide information aboute the site configuration.
 *
 * Return:	None.
 **/
static void site_usage(void)
{
	struct site_stat_s *s;

	/* Get the pointer to the site state. */
	s = &site_stat;

	printf("site - simultaneous van<->python data transfer experiments\n");
	printf("  -c x  cable configuration: substiute x with:\n");
	printf("        p  van-python cable: \"/van_py\"  <-> \"/python\"\n");
	printf("        t  van-tcl cable:    \"/van_tcl\" <-> \"/tcl\"\n");
	printf("  -v x  van or ctr.-tech. I/O configuration: substitute x with:\n");
	printf("        a   asynchronous read and write\n");
	printf("        bc  blocking sync. copy read and write\n");
	printf("        bz  blocking sync. zero copy read and sync. copy write\n");
	printf("        nc  non blocking sync. copy read and write\n");
	printf("        nz  non blocking sync. zero copy read and sync. copy write\n");
	printf("  -n x  neighbour I/O configuration - python or tcl/tk: substiute x with:\n");
	printf("        a   asynchronous read and write\n");
	printf("        bc  blocking sync. copy read and write\n");
	printf("        bz  blocking sync. zero copy read and sync. copy write\n");
	printf("        nc  non blocking sync. copy read and write\n");
	printf("        nz  non blocking sync. zero copy read and sync. copy write\n");
	printf("  -d n  number of C->N cycles\n");
	printf("  -u n  number of N->C cycles\n");
	printf("  -l n  fill the C transfer buffer with n characters: [%d, %d]\n",
	       MIN_SIZE, MAX_SIZE);
	printf("  -s n  fill the N transfer buffer with n characters: [%d, %d]\n",
	       MIN_SIZE, MAX_SIZE);
	printf("  -f c  C fill character\n");
	printf("  -r c  N fill character\n");
	printf("  -o    activate the OS trace\n");
	printf("  -t    activate the site trace\n");
	printf("  -h    show this usage\n");
	printf("\nDefault settings:\n");
	printf("  Cabling:                 %s\n", ct_to_string());
	printf("  Van or ctrl. tech. I/O:  %s\n", io_to_string(s->c_io));
	printf("  Neighbour I/O:           %s\n", io_to_string(s->n_io));
	printf("  C->N cycles:             %d\n", s->c_wr_cycles);
	printf("  N->C cycles:             %d\n", s->n_wr_cycles);
	printf("  C buf size:              %d\n", s->c_buf_size);
	printf("  N buf size:              %d\n", s->n_buf_size);
	printf("  C fill char:             %c\n", s->c_fill_char);	
	printf("  N fill char:             %c\n", s->n_fill_char);
	printf("  Final char:              %c\n", FINAL_CHAR);	
	printf("  OS trace:                %d\n", s->os_trace);
	printf("  Site trace:              %d\n", s->my_trace);
}

/**
 * site_fill_char() - analyze the requested C or N fill character
 *
 * @arg:        pointer to the digit string.
 * @fill_char:  pointer to the fill character.
 *
 * Return:	0 or force a software trap.
 **/
static void site_fill_char(char *arg, int *fill_char)
{
	int len;

	if (arg == NULL) {
		site_usage();
		exit(1);
	}

	/* Test the argument. */
	len = os_strlen(arg);
	if (len != 1) {
		site_usage();
		exit(1);
	}

	/* Save and test the fill char. */
	*fill_char = *arg;
	if (*fill_char == FINAL_CHAR) {
		site_usage();
		exit(1);
	}
}

/**
 * site_buf_size() - define the character number of a C or N transfer
 * buffer.
 *
 * @arg:   pointer to the digit string.
 * @size:  pointer to the buffer size.
 *
 * Return:	0 or force a software trap.
 **/
static void site_buf_size(char *arg, int *size)
{
	if (arg == NULL) {
		site_usage();
		exit(1);
	}

	/* Convert the digit string and test the number. */
	*size = strtol(arg, NULL, 10);
	if (*size < MIN_SIZE || *size > MAX_SIZE) {
		site_usage();
		exit(1);
	}
}

/**
 * site_wr_cycles() - define the number of the C or N cycles
 *
 * @arg:     pointer to the digit string.
 * @cycles:  pointer to the cycle setting.
 *
 * Return:	0 or force a software trap.
 **/
static void site_wr_cycles(char *arg, int *cycles)
{
	if (arg == NULL) {
		site_usage();
		exit(1);
	}
	
	*cycles = strtol(arg, NULL, 10);
}

/**
 * site_io() - van or python I/O configuration.
 *
 * @arg:  pointer to the char string.
 * @io:   pointer to I/O setting.
 *
 * Return:	0 or force a software trap.
 **/
static void site_io(char *arg, io_t *io)
{
	if (arg == NULL) {
		site_usage();
		exit(1);
	}
	
	/* Analyze the argument. */
	if (os_strcmp(arg, "a") == 0)
		*io = IO_ASYNC;
	
	else if (os_strcmp(arg, "bc") == 0)
		*io = IO_SYNC_BL_COPY;

	else if (os_strcmp(arg, "bz") == 0)
		*io = IO_SYNC_BL_ZERO;

	else if (os_strcmp(arg, "nc") == 0)
		*io = IO_SYNC_NB_COPY;

	else if (os_strcmp(arg, "nz") == 0)
		*io = IO_SYNC_NB_ZERO;

	else {
		site_usage();
		exit(1);
	}
}

/**
 * site_cable_conf() - test and convert the cable configuration.
 *
 * @arg:  pointer to the char string.
 * @cc:   pointer to the cable configuration.
 *
 * Return:	0 or force a software trap.
 **/
static void site_cable_conf(char *arg, cc_t *cc)
{
	int len;
	
	if (arg == NULL) {
		site_usage();
		exit(1);
	}
	
	/* Test the argument. */
	len = os_strlen(arg);
	if (len != 1) {
		site_usage();
		exit(1);
	}

	/* Analyze the argument. */
	switch(*arg) {
	case 'p':
		cc->type   = CT_VAN_PY;
		cc->l_name = "/van_py";
		cc->f_name = "/python";
		break;
	case 't':
		cc->type   = CT_VAN_TCL;
		cc->l_name = "/van_tcl";
		cc->f_name = "/tcl";
		break;
	default:
		site_usage();
		exit(1);
		break;
	}
}

/**
 * site_default() - define the site default settings.
 *
 * Return:	0 or force a software trap.
 **/
static void site_default(void)
{
	struct site_stat_s *s;
	
	/* Get the pointer to the site state. */
	s = &site_stat;

	s->cc.type      = CT_VAN_PY;
	s->cc.l_name    = "/van_py";
	s->cc.f_name    = "/python";
	s->c_io      = IO_SYNC_BL_COPY;
	s->n_io       = IO_SYNC_BL_COPY;
	s->c_wr_cycles = 0;
	s->n_wr_cycles = 0;
	s->c_buf_size  = MIN_SIZE;
	s->n_buf_size  = MIN_SIZE;
	s->c_fill_char = C_F_CHAR;
	s->n_fill_char = N_F_CHAR;
	s->os_trace     = 0;
	s->my_trace     = 0;
}

/*============================================================================
  EXPORTED FUNCTIONS
  ============================================================================*/
/**
 * main() - start function of the simultaneous data transfer experiments.
 *
 * @argc:  argument counter.
 * @argv:  list of the arguments.
 *
 * Return:	0 or force a software trap.
 **/
int main(int argc, char *argv[])
{
	struct site_stat_s *s;
	int opt;

	/* Get the pointer to the site state. */
	s = &site_stat;

	/* Define the site default settings. */
	site_default();
	
	/* Analyze the site arguments. */
	while ((opt = getopt(argc, argv, "c:d:f:hl:n:or:s:tu:v:")) != -1) {
		/* Analyze the current argument. */
		switch (opt) {
		case 'c':
			site_cable_conf(optarg, &s->cc);
			break;
		case 'd':
			site_wr_cycles(optarg, &s->c_wr_cycles);
			break;
		case 'f':
			site_fill_char(optarg, &s->c_fill_char);
			break;
		case 'h':
			site_usage();
			exit(0);
			break;
		case 'l':
			site_buf_size(optarg, &s->c_buf_size);
			break;
		case 'n':
			site_io(optarg, &s->n_io);
			break;
		case 'o':
			s->os_trace = 1;
			break;
		case 'r':
			site_fill_char(optarg, &s->n_fill_char);
			break;
		case 's':
			site_buf_size(optarg, &s->n_buf_size);
			break;
		case 't':
			s->my_trace = 1;
			break;
		case 'u':
			site_wr_cycles(optarg, &s->n_wr_cycles);
			break;
		case 'v':
			site_io(optarg, &s->c_io);
			break;
		default:
			site_usage();
			exit(1);
			break;
		}
	}

	/* Lend the test resources for a short time. */
	site_init();

	/* Free all borrowed resources needed for the data transfer experiments. */
	site_cleanup();

	printf("\nTest settings:\n");
	printf("  Cabling:                 %s\n", ct_to_string());
	printf("  Van or ctrl. tech. I/O:  %s\n", io_to_string(s->c_io));
	printf("  Neighbour I/O:           %s\n", io_to_string(s->n_io));
	printf("  C->N cycles:             %d\n", s->c_wr_cycles);
	printf("  N->C cycles:             %d\n", s->n_wr_cycles);
	printf("  C buf size:              %d\n", s->c_buf_size);
	printf("  N buf size:              %d\n", s->n_buf_size);
	printf("  C fill char:             %c\n", s->c_fill_char);	
	printf("  N fill char:             %c\n", s->n_fill_char);
	printf("  Final char:              %c\n", FINAL_CHAR);	
	printf("  OS trace:                %d\n", s->os_trace);
	printf("  Site trace:              %d\n", s->my_trace);
	
	return (0);
}
