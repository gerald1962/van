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
#include <stdlib.h>  /* Common C interfaces: exit().*/
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
 * site_stat - state of the bidirectional data transfer.
 *
 * @copy_rd:       if 1, call sync. write and sync. copy read.
 * @dl_wr_cycles:  number of the DL write cycles.
 * @ul_wr_cycles:  number of the UL write cycles.
 * @dl_buf_size:   fill the DL transfer buffer with n characters.
 * @ul_buf_size:   fill the UL transfer buffer with n characters.
 * @dl_fill_char:  DL fill character.
 * @ul_fill_char:  UL fill character.
 * @os_trace:      if 1, activate the OS trace.
 * @my_trace:      if 1, activate the site trace.
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
	int  copy_rd;
	int  dl_wr_cycles;
	int  ul_wr_cycles;
	int  dl_buf_size;
	int  ul_buf_size;
	int  dl_fill_char;
	int  ul_fill_char;
	int  os_trace;
	int  my_trace;
	
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
 * site_ul_writer_exec() - the py ul_writer thread generates UL test data for
 * van.
 *
 * @msg:  addess of the generic input message.
 *
 * Return:	None.
 **/
static void site_ul_writer_exec(os_queue_elem_t *msg)
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
	
	TRACE(("ul_writer> [s:ready,o:generate]\n"));

	/* Initialize the UL write buffer. */
	os_memset(buf, 0, OS_BUF_SIZE);
	os_memset(buf, s->ul_fill_char, s->ul_buf_size);

	/* The py ul_writer thread generates data for van with the sync write
	 * interface. */
	for (i = 0; i < s->ul_wr_cycles; i++) {
		os_sync_write(s->py_id, buf, s->ul_buf_size);
		TRACE(("ul_writer> sent: [c:%d, b:\"%c...\", s:%d]\n", i, *buf, s->ul_buf_size));
	}

	/* Send the final char to van. */
	*buf = FINAL_CHAR;
	os_sync_write(s->py_id, buf, s->ul_buf_size);
	TRACE(("ul_writer> sent: [c:%d, b:\"%c\", s:%d]\n", i, *buf, s->ul_buf_size));

end:
	/* Resume the main process. */
	atomic_store(&s->ul_wr_done, 1);
	site_resume();
}

/**
 * site_ul_reader_exec() - the van ul_reader thread analyzes the received UL test
 * data from py.
 *
 * @msg:  addess of the generic input message.
 *
 * Return:	None.
 **/
static void site_ul_reader_exec(os_queue_elem_t *msg)
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
	
	TRACE(("ul_reader> [s:ready,o:analyze]\n"));

	/* The van ul_writer thread analyzes data from py with the sync zero
	 * copy read interface or with the copy read interface. */
	for(i = 0;; i++) {
		/* Test the read mode. */
		if (s->copy_rd) {
			/* Receive the UL paylaod with the copy read interface. */
			os_memset(buf, 0, OS_BUF_SIZE);
			n = os_sync_read(s->van_id, buf, OS_BUF_SIZE);
			OS_TRAP_IF(n != s->ul_buf_size);
			b = buf;
		}
		else {
			/* Receive the UL paylaod with the zero copy read interface. */
			zbuf = NULL;
			n = os_sync_zread(s->van_id, &zbuf, OS_BUF_SIZE);
			OS_TRAP_IF(zbuf == NULL || n != s->ul_buf_size);
			b = zbuf;
		}

		/* Test the end condition of the test. */
		if (*b == FINAL_CHAR) {
			TRACE(("ul_reader> received: [c:%d, b:\"%c\", s:%d]\n", i, *b, n));
			break;
		}

		/* Test the contents of UL buffer. */
		stat = os_memcmp(b, s->ul_ref_b, s->ul_buf_size);
		OS_TRAP_IF(stat != 0);
	}

	/* Test the read mode. */
	if (! s->copy_rd) {
		/* Release the pending UL buffer. */
		n = os_sync_zread(s->van_id, NULL, 0);
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
 * site_dl_reader_exec() - the py dl_reader thread analyzes the received DL test
 * data from van.
 *
 * @msg:  addess of the generic input message.
 *
 * Return:	None.
 **/
static void site_dl_reader_exec(os_queue_elem_t *msg)
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

	TRACE(("dl_reader> [s:ready,o:analyze]\n"));

	/* Fill the DL reference buffer. */

	/* The py dl_writer thread analyzes data from van with the sync zero
	 * copy read interface or with the copy read interface. */
	for(i = 0;; i++) {
		/* Test the read mode. */
		if (s->copy_rd) {
			/* Receive the DL paylaod with the copy read interface. */
			os_memset(buf, 0, OS_BUF_SIZE);
			n = os_sync_read(s->py_id, buf, OS_BUF_SIZE);
			OS_TRAP_IF(n != s->dl_buf_size);
			b = buf;
		}
		else {
			/* Receive the DL paylaod with the zero copy read interface. */
			zbuf = NULL;
			n = os_sync_zread(s->py_id, &zbuf, OS_BUF_SIZE);
			OS_TRAP_IF(zbuf == NULL || n != s->dl_buf_size);
			b = zbuf;
		}

		/* Test the end condition of the test. */
		if (*b == FINAL_CHAR) {
			TRACE(("dl_reader> received: [c:%d, b:\"%c\", s:%d]\n", i, *b, n));
			break;
		}
		
		TRACE(("dl_reader> received: [c:%d, b:\"%c...\", s:%d]\n", i, *b, n));

		/* Test the contents of DL buffer. */
		stat = os_memcmp(b, s->dl_ref_b, s->dl_buf_size);
		OS_TRAP_IF(stat != 0);
	}

	/* Test the read mode. */
	if (! s->copy_rd) {
		/* Release the pending DL buffer. */
		n = os_sync_zread(s->py_id, NULL, 0);
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
 * site_dl_writer_exec() - the van dl_writer thread generates DL test data for
 * py.
 *
 * @msg:  addess of the generic input message.
 *
 * Return:	None.
 **/
static void site_dl_writer_exec(os_queue_elem_t *msg)
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
	
	TRACE(("dl_writer> [s:ready,o:generate]\n"));

	/* Initialize the DL write buffer. */
	os_memset(buf, 0, OS_BUF_SIZE);
	os_memset(buf, s->dl_fill_char, s->dl_buf_size);

	/* The van dl_writer thread generates data for py with the sync write
	 * interface. */
	for (i = 0; i < s->dl_wr_cycles; i++) {
		os_sync_write(s->van_id, buf, s->dl_buf_size);
		TRACE(("dl_writer> sent: [c:%d, b:\"%c...\", s:%d]\n", i, *buf, s->dl_buf_size));
	}

	/* Send the final char to py. */
	*buf = FINAL_CHAR;
	os_sync_write(s->van_id, buf, s->dl_buf_size);
	TRACE(("dl_writer> sent: [c:%d, b:\"%c\", s:%d]\n", i, *buf, s->dl_buf_size));

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

	TRACE(("%s [p:main,s:boot,o:init]\n", P));

	/* Get the pointer to the site state. */
	s = &site_stat;
	
	/* Fill the DL and UL reference buffer. */
	os_memset(s->dl_ref_b, s->dl_fill_char, s->dl_buf_size);
	os_memset(s->ul_ref_b, s->ul_fill_char, s->ul_buf_size);
	
	/* Initialize the operating system. */
	os_init();

	/* Configure the OS trace. */
	os_trace_button(s->os_trace);

	/* Install the van and py shm device. */
	s->van_id = os_open("/van");
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
	
	msg.cb = site_dl_writer_exec;
	OS_SEND(s->dl_writer, &msg, sizeof(msg));
	
	msg.cb = site_dl_reader_exec;
	OS_SEND(s->dl_reader, &msg, sizeof(msg));
	
	msg.cb = site_ul_reader_exec;
	OS_SEND(s->ul_reader, &msg, sizeof(msg));
	
	msg.cb = site_ul_writer_exec;
	OS_SEND(s->ul_writer, &msg, sizeof(msg));

	/* Wait for the resume trigger. */
	site_wait();
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
	
	printf("  -i x  I/O configuration: substiute x with:\n");
	printf("          a: asynchronous read and write\n");
	printf("          c: sync. copy read and write\n");
	printf("          z: sync. zero copy read and sync. copy write\n");
	
	printf("  -z    perform the test with sync. write and sync. zero copy read\n");
	printf("  -c    perform the test with sync. write and sync. copy read\n");
	
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
	printf("  zero copy read\n");
	printf("  DL cycles: %d\n", s->dl_wr_cycles);
	printf("  UL cycles: %d\n", s->ul_wr_cycles);
	printf("  DL buf size: %d\n", s->dl_buf_size);
	printf("  UL buf size: %d\n", s->ul_buf_size);
	printf("  DL fill char: %c\n", s->dl_fill_char);	
	printf("  UL fill char: %c\n", s->ul_fill_char);	
	printf("  Final char: %c\n", FINAL_CHAR);	
	printf("  OS trace off\n");
	printf("  Site trace off\n");
}

/**
 * site_ul_fill_char() - analyze the requested UL fill character
 *
 * @arg:  pointer to the digit string.
 *
 * Return:	0 or force a software trap.
 **/
static void site_ul_fill_char(char *arg)
{
	struct site_stat_s *s;	
	int len;
	
	/* Get the pointer to the site state. */
	s = &site_stat;

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
	s->ul_fill_char = *arg;
	if (s->ul_fill_char == FINAL_CHAR) {
		site_usage();
		exit(1);
	}
}

/**
 * site_dl_fill_char() - analyze the requested DL fill character
 *
 * @arg:  pointer to the digit string.
 *
 * Return:	0 or force a software trap.
 **/
static void site_dl_fill_char(char *arg)
{
	struct site_stat_s *s;	
	int len;
	
	/* Get the pointer to the site state. */
	s = &site_stat;

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
	s->dl_fill_char = *arg;
	if (s->dl_fill_char == FINAL_CHAR) {
		site_usage();
		exit(1);
	}
}

/**
 * void site_ul_buf_size() - define the character number of a UL transfer buffer.
 *
 * @arg:  pointer to the digit string.
 *
 * Return:	0 or force a software trap.
 **/
static void site_ul_buf_size(char *arg)
{
	struct site_stat_s *s;
	
	/* Get the pointer to the site state. */
	s   = &site_stat;

	if (arg == NULL) {
		site_usage();
		exit(1);
	}

	/* Convert the digit string and test the number. */
	s->ul_buf_size = strtol(arg, NULL, 10);
	if (s->ul_buf_size < MIN_SIZE || s->ul_buf_size > MAX_SIZE) {
		site_usage();
		exit(1);
	}
}

/**
 * void site_dl_buf_size() - define the character number of a DL transfer buffer.
 *
 * @arg:  pointer to the digit string.
 *
 * Return:	0 or force a software trap.
 **/
static void site_dl_buf_size(char *arg)
{
	struct site_stat_s *s;
	
	/* Get the pointer to the site state. */
	s   = &site_stat;

	if (arg == NULL) {
		site_usage();
		exit(1);
	}

	/* Convert the digit string and test the number. */
	s->dl_buf_size = strtol(arg, NULL, 10);
	if (s->dl_buf_size < MIN_SIZE || s->dl_buf_size > MAX_SIZE) {
		site_usage();
		exit(1);
	}
}

/**
 * site_ul_wr_cycles() - define the number of the UL cycles
 *
 * @arg:  pointer to the digit string.
 *
 * Return:	0 or force a software trap.
 **/
static void site_ul_wr_cycles(char *arg)
{
	if (arg == NULL) {
		site_usage();
		exit(1);
	}

	site_stat.ul_wr_cycles = strtol(arg, NULL, 10);
}

/**
 * site_dl_wr_cycles() - define the number of the DL cycles
 *
 * @arg:  pointer to the digit string.
 *
 * Return:	0 or force a software trap.
 **/
static void site_dl_wr_cycles(char *arg)
{
	if (arg == NULL) {
		site_usage();
		exit(1);
	}
	
	site_stat.dl_wr_cycles = strtol(arg, NULL, 10);
}

/**
 * void site_default() - define the site default settings.
 *
 * Return:	0 or force a software trap.
 **/
static void site_default(void)
{
	struct site_stat_s *s;
	
	/* Get the pointer to the site state. */
	s = &site_stat;

	s->copy_rd      = 0;
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
	while ((opt = getopt(argc, argv, "cd:f:hl:or:s:tu:z")) != -1) {
		/* Analyze the current argument. */
		switch (opt) {
		case 'c':
			s->copy_rd = 1;
			break;
		case 'd':
			site_dl_wr_cycles(optarg);
			break;
		case 'f':
			site_dl_fill_char(optarg);
			break;
		case 'h':
			site_usage();
			exit(0);
			break;
		case 'l':
			site_dl_buf_size(optarg);
			break;
		case 'o':
			s->os_trace = 1;
			break;
		case 'r':
			site_ul_fill_char(optarg);
			break;
		case 's':
			site_ul_buf_size(optarg);
			break;
		case 't':
			s->my_trace = 1;
			break;
		case 'u':
			site_ul_wr_cycles(optarg);
			break;
		case 'z':
			s->copy_rd = 0;
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
	printf("  Copy read: %d\n", s->copy_rd);
	printf("  DL cycles: %d\n", s->dl_wr_cycles);
	printf("  UL cycles: %d\n", s->ul_wr_cycles);
	printf("  DL buf size: %d\n", s->dl_buf_size);
	printf("  UL buf size: %d\n", s->ul_buf_size);
	printf("  DL fill char: %c\n", s->dl_fill_char);	
	printf("  UL fill char: %c\n", s->ul_fill_char);
	printf("  Final char: %c\n", FINAL_CHAR);	
	printf("  OS trace: %d\n", s->os_trace);
	printf("  Site trace: %d\n", s->my_trace);
	
	return (0);
}
