// SPDX-License-Identifier: GPL-2.0

/*
 * site - Simultaneous DL and UL date transer experiments from van to py and
 * back.
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

#define DL_F_CHAR   'd'  /* DL fill character. */
#define UL_F_CHAR   'u'  /* UL fill character. */
#define FINAL_CHAR  '#'  /* Contents of the final payload. */

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
 * @IO_SYNC_COPY:  call sync. write and sync. copy read.
 * @IO_SYNC_ZERO:  call sync. write and sync. zero copy read.
 * @IO_ASYNC:      perform read and write asynchronously.
 **/
typedef enum {
	IO_SYNC_COPY,
	IO_SYNC_ZERO,
	IO_ASYNC
} io_t;

/** 
 * site_stat - state of the bidirectional data transfer.
 *
 * @van_io:        van I/O configuration.
 * @python_io:     python I/O configuration.
 * @dl_wr_cycles:  number of the DL write cycles.
 * @ul_wr_cycles:  number of the UL write cycles.
 * @dl_buf_size:   fill the DL transfer buffer with n characters.
 * @ul_buf_size:   fill the UL transfer buffer with n characters.
 * @dl_fill_char:  DL fill character.
 * @ul_fill_char:  UL fill character.
 * @os_trace:      if 1, activate the OS trace.
 * @my_trace:      if 1, activate the site trace.
 *
 * @dl_wr_count:   count the number of the DL van write_cb calls.
 * @dl_rd_count:   count the number of the DL py read_cb calls.
 * @ul_rd_count:   count the number of the UL van read_cb calls.
 * @ul_wr_count:   count the number of the UL py write_cb calls.
 *
 * @dl_wr_done:    if 1, the van DL writer thread has done its job.
 * @dl_rd_done:    if 1, the py DL reader has finished the data receive.
 * @ul_rd_done:    if 1, the van UL read thread has terminted the data analyis.
 * @ul_wr_done:    if 1, the py UL writer has done the data generation.
 * @suspend:       suspend the main process while the test is running.
 * @van_id:        id of the van shm device.
 * @py_id:         id of the py shm deice.
 * @ul_writer:     address of the van DL writer thread.
 * @dl_reader:     address of the py DL reader thread.
 * @ul_reader:     address of the van UL writer thread.
 * @ul_writer:     address of the py UL writer thread.
 *
 * @dl_wr_b:       DL write buffer.
 * @dl_rd_b:       DL read buffer.
 * @ul_rd_b:       UL read buffer.
 * @ul_wr_b:       UL write buffer.
 * @dl_ref_b:      reference buffer to detect DL transfer errors.
 * @ul_ref_b:      reference buffer to detect UL transfer errors.
 **/
static struct site_stat_s {
	io_t  van_io;
	io_t  python_io;
	int   dl_wr_cycles;
	int   ul_wr_cycles;
	int   dl_buf_size;
	int   ul_buf_size;
	int   dl_fill_char;
	int   ul_fill_char;
	int   os_trace;
	int   my_trace;

	int   dl_wr_count;
	int   dl_rd_count;
	int   ul_rd_count;
	int   ul_wr_count;

	atomic_int  dl_wr_done;
	atomic_int  dl_rd_done;
	atomic_int  ul_rd_done;
	atomic_int  ul_wr_done;
	sem_t   suspend;
	int     van_id;
	int     py_id;
	void   *dl_writer;
	void   *dl_reader;
	void   *ul_reader;
	void   *ul_writer;
	
	char    dl_wr_b[OS_BUF_SIZE];
	char    dl_rd_b[OS_BUF_SIZE];
	char    ul_rd_b[OS_BUF_SIZE];
	char    ul_wr_b[OS_BUF_SIZE];
	char    dl_ref_b[OS_BUF_SIZE];
	char    ul_ref_b[OS_BUF_SIZE];
} site_stat;

/*============================================================================
  LOCAL FUNCTION PROTOTYPES
  ============================================================================*/
/*============================================================================
  LOCAL FUNCTIONS
  ============================================================================*/
/**
 * site_resume() - resume the main process.
 *
 * Return:	0 or force a software trap.
 **/
void site_resume(void)
{
	TRACE(("%s [p:nain,s:suspended,m:resume]\n", P));

	/* Resume the main program. */
	os_sem_release(&site_stat.suspend);
}

/**
 * site_aio_dl_rd_cb() - the python irq thread delivers DL data from van.
 *
 * @dev_id:  py device id.
 * @buf:     pointer to the shared memory buffer.
 * @count:   size of the DL payload.
 *
 * Return:	the number of the consumed characters.
 **/
static int site_aio_dl_rd_cb(int dev_id, char *buf, int count)
{
	struct site_stat_s *s;
	int stat;

	/* Get the pointer to the site state. */
	s = &site_stat;

	/* Entry condition. */
	OS_TRAP_IF(s->py_id != dev_id || buf == NULL || count != s->dl_buf_size);

	/* Test the end condition of the test. */
	if (*buf == FINAL_CHAR) {
		TRACE(("dl_reader> received: [i/o:a, c:%d, b:\"%c\", s:%d]\n",
		       s->dl_rd_count, *buf, count));
		
		/* Test the DL read counter */
		OS_TRAP_IF(s->dl_rd_count != s->dl_wr_cycles);
		
		/* Resume the main process. */
		atomic_store(&s->dl_rd_done, 1);
		site_resume();

		return count;
	}
	
	/* Test the DL read counter */
	OS_TRAP_IF(s->dl_rd_count >= s->dl_wr_cycles);
		
	TRACE(("dl_reader> received: [i/o:a, c:%d, b:\"%c...\", s:%d]\n",
	       s->dl_rd_count, *buf, count));
		
	/* Test the contents of the DL buffer. */
	stat = os_memcmp(buf, s->dl_ref_b, count);
	OS_TRAP_IF(stat != 0);

	/* Update the DL read counter. */
	s->dl_rd_count++;
	
	return count;
}

/**
 * site_aio_dl_rd_exec() - the python dl_reader thread is inactiv because of the
 * async. I/O configuration.
 *
 * @msg:  addess of the generic input message.
 *
 * Return:	None.
 **/
static void site_aio_dl_rd_exec(os_queue_elem_t *msg)
{
	struct site_stat_s *s;
	
	/* Get the pointer to the site state. */
	s   = &site_stat;
	
	/* Test the DL counter. */
	if (s->dl_wr_cycles < 1) {
		/* Resume the main process. */
		atomic_store(&s->dl_rd_done, 1);
		site_resume();
		return;
	}
	
	TRACE(("dl_reader> [i/o:a, s:ready, o:inactive]\n"));

	/* Trigger the py interrupt handler to invoke the async. read and write
	 *  callback. */
	os_aio_read(s->py_id);
	os_aio_write(s->py_id);
}

/**
 * site_aio_ul_wr_cb() - the py irq thread requests UL data if available.
 *
 * @dev_id:  py device id.
 * @buf:     pointer to the shared memory buffer.
 * @count:   size of the shared memory buffer.
 *
 * Return:	number of the saved characters.
 **/
static int site_aio_ul_wr_cb(int dev_id, char *buf, int count)
{
	struct site_stat_s *s;

	/* Get the pointer to the site state. */
	s = &site_stat;

	/* Entry condition. */
	OS_TRAP_IF(s->py_id != dev_id || buf == NULL || count != OS_BUF_SIZE);

	/* Test the generation status. */
	if (s->ul_wr_cycles < 1)
		return 0;
	
	/* Test the cycle counter. */
	if (s->ul_wr_count > s->ul_wr_cycles) {
		/* Resume the main process. */
		atomic_store(&s->ul_wr_done, 1);
		site_resume();
		return 0;
	}
	
	/* Fill the send buffer. */
	os_memset(buf, 0, OS_BUF_SIZE);
	os_memset(buf, s->ul_fill_char, s->ul_buf_size);
	
	/* Test the cycle counter. */
	if (s->ul_wr_count == s->ul_wr_cycles)
		*buf = FINAL_CHAR;

	TRACE(("ul_writer> sent: [i/o:a, c:%d, b:\"%c...\", s:%d]\n",
	       s->ul_wr_count, *buf, s->ul_buf_size));

	/* Increment the cycle counter. */
	s->ul_wr_count++;
	
	return s->ul_buf_size;
}

/**
 * site_aio_ul_wr_exec() - the python ul_writer thread triggers the python
 * driver to start the UL transfer.
 *
 * @msg:  addess of the generic input message.
 *
 * Return:	None.
 **/
static void site_aio_ul_wr_exec(os_queue_elem_t *msg)
{
	struct site_stat_s *s;
	
	/* Get the pointer to the site state. */
	s   = &site_stat;
	
	/* Test the UL counter. */
	if (s->ul_wr_cycles < 1) {
		/* Resume the main process. */
		atomic_store(&s->ul_wr_done, 1);
		site_resume();
		return;
	}

	TRACE(("ul_writer> [i/o:a, s:ready, o:trigger]\n"));

	/* Trigger the py interrupt handler to invoke the async. write and read
	 *  callback. */
	os_aio_write(s->py_id);
	os_aio_read(s->py_id);
}

/**
 * site_aio_ul_rd_cb() - the van irq thread delivers UL data from py.
 *
 * @dev_id:  van device id.
 * @buf:     pointer to the shared memory buffer.
 * @count:   size of the DL payload.
 *
 * Return:	the number of the consumed characters.
 **/
static int site_aio_ul_rd_cb(int dev_id, char *buf, int count)
{
	struct site_stat_s *s;
	int stat;

	/* Get the pointer to the site state. */
	s = &site_stat;

	/* Entry condition. */
	OS_TRAP_IF(s->van_id != dev_id || buf == NULL || count != s->ul_buf_size);

	/* Test the end condition of the test. */
	if (*buf == FINAL_CHAR) {
		TRACE(("ul_reader> received: [i/o:a, c:%d, b:\"%c\", s:%d]\n",
		       s->dl_rd_count, *buf, count));
		
		/* Test the UL read counter */
		OS_TRAP_IF(s->ul_rd_count != s->ul_wr_cycles);
		
		/* Resume the main process. */
		atomic_store(&s->ul_rd_done, 1);
		site_resume();

		return count;
	}
	
	/* Test the UL read counter */
	OS_TRAP_IF(s->ul_rd_count >= s->ul_wr_cycles);
		
	TRACE(("ul_reader> received: [i/o:a, c:%d, b:\"%c...\", s:%d]\n",
	       s->ul_rd_count, *buf, count));
		
	/* Test the contents of the UL buffer. */
	stat = os_memcmp(buf, s->ul_ref_b, count);
	OS_TRAP_IF(stat != 0);

	/* Update the UL read counter. */
	s->ul_rd_count++;
	
	return count;
}

/**
 * site_aio_ul_rd_exec() - the van ul_reader thread is inactiv because of the
 * async. I/O configuration.
 *
 * @msg:  addess of the generic input message.
 *
 * Return:	None.
 **/
static void site_aio_ul_rd_exec(os_queue_elem_t *msg)
{
	struct site_stat_s *s;
	
	/* Get the pointer to the site state. */
	s   = &site_stat;
	
	/* Test the UL counter. */
	if (s->ul_wr_cycles < 1) {
		/* Resume the main process. */
		atomic_store(&s->ul_rd_done, 1);
		site_resume();
		return;
	}
	
	TRACE(("ul_reader> [i/o:a, s:ready, o:inactive]\n"));

	/* Trigger the van interrupt handler to invoke the async. write and read
	 * callback. */
	os_aio_write(s->van_id);
	os_aio_read(s->van_id);
}

/**
 * site_aio_dl_wr_cb() - the van irq thread requests DL data if available.
 *
 * @dev_id:  van device id.
 * @buf:     pointer to the shared memory buffer.
 * @count:   size of the shared memory buffer.
 *
 * Return:	number of the saved characters.
 **/
static int site_aio_dl_wr_cb(int dev_id, char *buf, int count)
{
	struct site_stat_s *s;

	/* Get the pointer to the site state. */
	s = &site_stat;

	/* Entry condition. */
	OS_TRAP_IF(s->van_id != dev_id || buf == NULL || count != OS_BUF_SIZE);
	
	/* Test the generation status. */
	if (s->dl_wr_cycles < 1)
		return 0;

	/* Test the cycle counter. */
	if (s->dl_wr_count > s->dl_wr_cycles) {
		/* Resume the main process. */
		atomic_store(&s->dl_wr_done, 1);
		site_resume();
		return 0;
	}
	
	/* Fill the send buffer. */
	os_memset(buf, 0, OS_BUF_SIZE);
	os_memset(buf, s->dl_fill_char, s->dl_buf_size);
	
	/* Test the cycle counter. */
	if (s->dl_wr_count == s->dl_wr_cycles)
		*buf = FINAL_CHAR;

	TRACE(("dl_writer> sent: [i/o:a, c:%d, b:\"%c...\", s:%d]\n",
	       s->dl_wr_count, *buf, s->dl_buf_size));

	/* Increment the cycle counter. */
	s->dl_wr_count++;
	
	return s->dl_buf_size;
}

/**
 * site_aio_dl_wr_exec() - the van dl_writer thread triggers the van driver
 * to start the DL transfer.
 *
 * @msg:  addess of the generic input message.
 *
 * Return:	None.
 **/
static void site_aio_dl_wr_exec(os_queue_elem_t *msg)
{
	struct site_stat_s *s;
	
	/* Get the pointer to the site state. */
	s   = &site_stat;

	/* Test the DL counter. */
	if (s->dl_wr_cycles < 1) {
		/* Resume the main process. */
		atomic_store(&s->dl_wr_done, 1);
		site_resume();
		return;
	}
	
	TRACE(("dl_writer> [i/o:a, s:ready, o:trigger]\n"));

	/* Trigger the van interrupt handler to invoke the async. write and read
	 * callback. */
	os_aio_write(s->van_id);
	os_aio_read(s->van_id);
}

/**
 * site_sync_ul_wr_exec() - the py ul_writer thread generates UL test data for
 * van.
 *
 * @msg:  addess of the generic input message.
 *
 * Return:	None.
 **/
static void site_sync_ul_wr_exec(os_queue_elem_t *msg)
{
	struct site_stat_s *s;
	char *buf;
	int i;

	/* Get the pointer to the site state. */
	s   = &site_stat;
	buf = s->ul_wr_b;
	
	/* Test the UL counter. */
	if (s->ul_wr_cycles < 1)
		goto end;
	
	TRACE(("ul_writer> [i/o:s, s:ready, o:generate]\n"));

	/* Initialize the UL write buffer. */
	os_memset(buf, 0, OS_BUF_SIZE);
	os_memset(buf, s->ul_fill_char, s->ul_buf_size);

	/* The py ul_writer thread generates data for van with the sync write
	 * interface. */
	for (i = 0; i < s->ul_wr_cycles; i++) {
		os_write(s->py_id, buf, s->ul_buf_size);
		TRACE(("ul_writer> sent: [i/o:s, c:%d, b:\"%c...\", s:%d]\n", i, *buf, s->ul_buf_size));
	}

	/* Send the final char to van. */
	*buf = FINAL_CHAR;
	os_write(s->py_id, buf, s->ul_buf_size);
	TRACE(("ul_writer> sent: [i/o:s, c:%d, b:\"%c\", s:%d]\n", i, *buf, s->ul_buf_size));

end:
	/* Resume the main process. */
	atomic_store(&s->ul_wr_done, 1);
	site_resume();
}

/**
 * site_sync_ul_rd_exec() - the van ul_reader thread analyzes the received UL test
 * data from py.
 *
 * @msg:  addess of the generic input message.
 *
 * Return:	None.
 **/
static void site_sync_ul_rd_exec(os_queue_elem_t *msg)
{
	struct site_stat_s *s;
	char *zbuf, *buf, *b;
	int i, n, stat;

	/* Get the pointer to the site state. */
	s    = &site_stat;
	zbuf = NULL;
	buf  = s->ul_rd_b;

	/* Test the UL counter. */
	if (s->ul_wr_cycles < 1)
		goto end;
	
	TRACE(("ul_reader> [i/o:s, s:ready, o:analyze]\n"));

	/* The van ul_writer thread analyzes data from py with the sync zero
	 * copy read interface or with the copy read interface. */
	for(i = 0;; i++) {
		/* Test the read mode. */
		if (s->van_io == IO_SYNC_COPY) {
			/* Receive the UL paylaod with the copy read interface. */
			os_memset(buf, 0, OS_BUF_SIZE);
			n = os_read(s->van_id, buf, OS_BUF_SIZE);
			OS_TRAP_IF(n != s->ul_buf_size);
			b = buf;
		}
		else {
			/* Receive the UL paylaod with the zero copy read interface. */
			zbuf = NULL;
			n = os_zread(s->van_id, &zbuf, OS_BUF_SIZE);
			OS_TRAP_IF(zbuf == NULL || n != s->ul_buf_size);
			b = zbuf;
		}

		/* Test the end condition of the test. */
		if (*b == FINAL_CHAR) {
			TRACE(("ul_reader> received: [i/o:s, c:%d, b:\"%c\", s:%d]\n", i, *b, n));
			break;
		}

		/* Test the contents of the UL buffer. */
		stat = os_memcmp(b, s->ul_ref_b, n);
		OS_TRAP_IF(stat != 0 || n != s->ul_buf_size);
	}

	/* Test the read mode. */
	if (s->van_io == IO_SYNC_ZERO) {
		/* Release the pending UL buffer. */
		n = os_zread(s->van_id, NULL, 0);
		OS_TRAP_IF(n > 0);
	}

	/* Test the UL cycle counter */
	OS_TRAP_IF(i != s->ul_wr_cycles);

end:
	/* Resume the main process. */
	atomic_store(&s->ul_rd_done, 1);
	site_resume();
}

/**
 * site_sync_dl_rd_exec() - the py dl_reader thread analyzes the received DL test
 * data from van.
 *
 * @msg:  addess of the generic input message.
 *
 * Return:	None.
 **/
static void site_sync_dl_rd_exec(os_queue_elem_t *msg)
{
	struct site_stat_s *s;
	char *zbuf, *buf, *b;
	int i, n, stat;

	/* Get the pointer to the site state. */
	s    = &site_stat;
	zbuf = NULL;
	buf  = s->dl_rd_b;
	
	/* Test the DL counter. */
	if (s->dl_wr_cycles < 1)
		goto end;

	TRACE(("dl_reader> [i/o:s, s:ready, o:analyze]\n"));

	/* The py dl_writer thread analyzes data from van with the sync zero
	 * copy read interface or with the copy read interface. */
	for(i = 0;; i++) {
		/* Test the read mode. */
		if (s->python_io == IO_SYNC_COPY) {
			/* Receive the DL paylaod with the copy read interface. */
			os_memset(buf, 0, OS_BUF_SIZE);
			n = os_read(s->py_id, buf, OS_BUF_SIZE);
			OS_TRAP_IF(n != s->dl_buf_size);
			b = buf;
		}
		else {
			/* Receive the DL paylaod with the zero copy read interface. */
			zbuf = NULL;
			n = os_zread(s->py_id, &zbuf, OS_BUF_SIZE);
			OS_TRAP_IF(zbuf == NULL || n != s->dl_buf_size);
			b = zbuf;
		}

		/* Test the end condition of the test. */
		if (*b == FINAL_CHAR) {
			TRACE(("dl_reader> received: [i/o:s, c:%d, b:\"%c\", s:%d]\n", i, *b, n));
			break;
		}
		
		TRACE(("dl_reader> received: [i/o:s, c:%d, b:\"%c...\", s:%d]\n", i, *b, n));

		/* Test the contents of the DL buffer. */
		stat = os_memcmp(b, s->dl_ref_b, n);
		OS_TRAP_IF(stat != 0 || n != s->dl_buf_size);
	}

	/* Test the read mode. */
	if (s->python_io == IO_SYNC_ZERO) {
		/* Release the pending DL buffer. */
		n = os_zread(s->py_id, NULL, 0);
		OS_TRAP_IF(n > 0);
	}

	/* Test the DL cycle counter */
	OS_TRAP_IF(i != s->dl_wr_cycles);

end:
	/* Resume the main process. */
	atomic_store(&s->dl_rd_done, 1);
	site_resume();
}

/**
 * site_sync_dl_wr_exec() - the van dl_writer thread generates DL test data for
 * py.
 *
 * @msg:  addess of the generic input message.
 *
 * Return:	None.
 **/
static void site_sync_dl_wr_exec(os_queue_elem_t *msg)
{
	struct site_stat_s *s;
	char *buf;
	int i;
	
	/* Get the pointer to the site state. */
	s   = &site_stat;
	buf = s->dl_wr_b;
	
	/* Test the DL counter. */
	if (s->dl_wr_cycles < 1)
		goto end;
	
	TRACE(("dl_writer> [i/o:s, s:ready, o:generate]\n"));

	/* Initialize the DL write buffer. */
	os_memset(buf, 0, OS_BUF_SIZE);
	os_memset(buf, s->dl_fill_char, s->dl_buf_size);

	/* The van dl_writer thread generates data for py with the sync write
	 * interface. */
	for (i = 0; i < s->dl_wr_cycles; i++) {
		os_write(s->van_id, buf, s->dl_buf_size);
		TRACE(("dl_writer> sent: [i/o:s, c:%d, b:\"%c...\", s:%d]\n", i, *buf, s->dl_buf_size));
	}

	/* Send the final char to py. */
	*buf = FINAL_CHAR;
	os_write(s->van_id, buf, s->dl_buf_size);
	TRACE(("dl_writer> sent: [i/o:s, c:%d, b:\"%c\", s:%d]\n", i, *buf, s->dl_buf_size));

end:
	/* Resume the main process. */
	atomic_store(&s->dl_wr_done, 1);
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
	
	/* Remove the py and van shm device. */
	os_close(s->py_id);	
	os_close(s->van_id);	

	/* Remove all test threads. */
	os_thread_destroy(s->dl_writer);
	os_thread_destroy(s->dl_reader);
	os_thread_destroy(s->ul_reader);
	os_thread_destroy(s->ul_writer);
	
	/* Release the semaphore for the main process. */
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
	int dl_wr_done, dl_rd_done, ul_wr_done, ul_rd_done;
	
	/* Get the pointer to the site state. */
	s = &site_stat;
	
	/* Test the status for the data transfer threads. */
	for(;;) {
		TRACE(("%s [p:main,s:wait,o:suspend]\n", P));
		/* Suspend the main process. */
		os_sem_wait(&s->suspend);
		TRACE(("%s [p:main,s:test,o:resume]\n", P));
		
		/* Copy the status of all data transfer threads. */
		dl_wr_done = atomic_load(&s->dl_wr_done);
		dl_rd_done = atomic_load(&s->dl_rd_done);
		ul_wr_done = atomic_load(&s->ul_wr_done);
		ul_rd_done = atomic_load(&s->ul_rd_done);
		
		/* Test the status of all data transfer threads. */
		if (dl_wr_done && dl_rd_done && ul_wr_done && ul_rd_done)
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
	
	/* Fill the DL and UL reference buffer. */
	os_memset(s->dl_ref_b, s->dl_fill_char, s->dl_buf_size);
	os_memset(s->ul_ref_b, s->ul_fill_char, s->ul_buf_size);
	
	/* Initialize the operating system. */
	os_init(1);

	/* Configure the OS trace. */
	os_trace_button(s->os_trace);

	/* Install the van and py shm device. */
	s->van_id = os_open("/van_py");
	s->py_id  = os_open("/python");

	/* Install all test threads. */
	s->dl_writer = os_thread_create("dl_writer", PRIO, Q_SIZE);
	s->dl_reader = os_thread_create("dl_reader", PRIO, Q_SIZE);
	s->ul_reader = os_thread_create("ul_reader", PRIO, Q_SIZE);
	s->ul_writer = os_thread_create("ul_writer", PRIO, Q_SIZE);

	/* Control the lifetime of the loop test. */
	os_sem_init(&s->suspend, 0);

	/* Activate all data transfer threads. */
	os_memset(&msg, 0, sizeof(msg));

	/* Test the van I/O configuration */
	if (s->van_io == IO_ASYNC) {
		/* Install the van read and write callback for the asynchronous actions. */
		aio.write_cb = site_aio_dl_wr_cb;
		aio.read_cb  = site_aio_ul_rd_cb;
		os_aio_action(s->van_id, &aio);

		msg.cb = site_aio_dl_wr_exec;
		OS_SEND(s->dl_writer, &msg, sizeof(msg));
	
		msg.cb = site_aio_ul_rd_exec;
		OS_SEND(s->ul_reader, &msg, sizeof(msg));
	}
	else {
		msg.cb = site_sync_dl_wr_exec;
		OS_SEND(s->dl_writer, &msg, sizeof(msg));
	
		msg.cb = site_sync_ul_rd_exec;
		OS_SEND(s->ul_reader, &msg, sizeof(msg));
	}
	
	/* Test the python I/O configuration */
	if (s->python_io == IO_ASYNC) {
		/* Install the python read and write callback for the asynchronous actions. */
		aio.write_cb = site_aio_ul_wr_cb;
		aio.read_cb  = site_aio_dl_rd_cb;
		os_aio_action(s->py_id, &aio);

		msg.cb = site_aio_dl_rd_exec;
		OS_SEND(s->dl_reader, &msg, sizeof(msg));
	
		msg.cb = site_aio_ul_wr_exec;
		OS_SEND(s->ul_writer, &msg, sizeof(msg));
	}
	else {
		msg.cb = site_sync_dl_rd_exec;
		OS_SEND(s->dl_reader, &msg, sizeof(msg));
	
		msg.cb = site_sync_ul_wr_exec;
		OS_SEND(s->ul_writer, &msg, sizeof(msg));
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
	case IO_SYNC_ZERO: return "z";
	case IO_ASYNC:     return "a";
	default:           return "c";
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
	printf("  -v x  van I/O configuration: substiute x with:\n");
	printf("        a  asynchronous read and write\n");
	printf("        c  sync. copy read and write\n");
	printf("        z  sync. zero copy read and sync. copy write\n");
	printf("  -p x  python I/O configuration: substiute x with:\n");
	printf("        a  asynchronous read and write\n");
	printf("        c  sync. copy read and write\n");
	printf("        z  sync. zero copy read and sync. copy write\n");
	printf("  -d n  number of DL cycles\n");
	printf("  -u n  number of UL cycles\n");
	printf("  -l n  fill the DL transfer buffer with n characters: [%d, %d]\n",
	       MIN_SIZE, MAX_SIZE);
	printf("  -s n  fill the UL transfer buffer with n characters: [%d, %d]\n",
	       MIN_SIZE, MAX_SIZE);
	printf("  -f c  DL fill character\n");
	printf("  -r c  UL fill character\n");
	printf("  -o    activate the OS trace\n");
	printf("  -t    activate the site trace\n");
	printf("  -h    show this usage\n");
	printf("\nDefault settings:\n");
	printf("  van I/O:      %s\n", io_to_string(s->van_io));
	printf("  python I/O:   %s\n", io_to_string(s->python_io));
	printf("  DL cycles:    %d\n", s->dl_wr_cycles);
	printf("  UL cycles:    %d\n", s->ul_wr_cycles);
	printf("  DL buf size:  %d\n", s->dl_buf_size);
	printf("  UL buf size:  %d\n", s->ul_buf_size);
	printf("  DL fill char: %c\n", s->dl_fill_char);	
	printf("  UL fill char: %c\n", s->ul_fill_char);
	printf("  Final char:   %c\n", FINAL_CHAR);	
	printf("  OS trace:     %d\n", s->os_trace);
	printf("  site trace:   %d\n", s->my_trace);
}

/**
 * site_fill_char() - analyze the requested DL or UL fill character
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
 * site_buf_size() - define the character number of a DL or UL transfer
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
 * site_wr_cycles() - define the number of the DL or UL cycles
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
 * @arg:  pointer to the digit string.
 * @io:   pointer to I/O setting.
 *
 * Return:	0 or force a software trap.
 **/
static void site_io(char *arg, io_t *io)
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
	case 'a':
		*io = IO_ASYNC;
		break;
	case 'c':
		*io = IO_SYNC_COPY;
		break;
	case 'z':
		*io = IO_SYNC_ZERO;
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

	s->van_io       = IO_SYNC_COPY;
	s->python_io    = IO_SYNC_COPY;
	s->dl_wr_cycles = 0;
	s->ul_wr_cycles = 0;
	s->dl_buf_size  = MIN_SIZE;
	s->ul_buf_size  = MIN_SIZE;
	s->dl_fill_char = DL_F_CHAR;
	s->ul_fill_char = UL_F_CHAR;
	s->os_trace     = 0;
	s->my_trace     = 0;
}

/*============================================================================
  EXPORTED FUNCTIONS
  ============================================================================*/
/**
 * main() - start function of the simultaneous data transfer experiments.
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
	while ((opt = getopt(argc, argv, "d:f:hl:op:r:s:tu:v:")) != -1) {
		/* Analyze the current argument. */
		switch (opt) {
		case 'd':
			site_wr_cycles(optarg, &s->dl_wr_cycles);
			break;
		case 'f':
			site_fill_char(optarg, &s->dl_fill_char);
			break;
		case 'h':
			site_usage();
			exit(0);
			break;
		case 'l':
			site_buf_size(optarg, &s->dl_buf_size);
			break;
		case 'o':
			s->os_trace = 1;
			break;
		case 'p':
			site_io(optarg, &s->python_io);
			break;
		case 'r':
			site_fill_char(optarg, &s->ul_fill_char);
			break;
		case 's':
			site_buf_size(optarg, &s->ul_buf_size);
			break;
		case 't':
			s->my_trace = 1;
			break;
		case 'u':
			site_wr_cycles(optarg, &s->ul_wr_cycles);
			break;
		case 'v':
			site_io(optarg, &s->van_io);
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
	printf("  van I/O:      %s\n", io_to_string(s->van_io));
	printf("  python I/O:   %s\n", io_to_string(s->python_io));
	printf("  DL cycles:    %d\n", s->dl_wr_cycles);
	printf("  UL cycles:    %d\n", s->ul_wr_cycles);
	printf("  DL buf size:  %d\n", s->dl_buf_size);
	printf("  UL buf size:  %d\n", s->ul_buf_size);
	printf("  DL fill char: %c\n", s->dl_fill_char);	
	printf("  UL fill char: %c\n", s->ul_fill_char);
	printf("  Final char:   %c\n", FINAL_CHAR);	
	printf("  OS trace:     %d\n", s->os_trace);
	printf("  site trace:   %d\n", s->my_trace);
	
	return (0);
}
