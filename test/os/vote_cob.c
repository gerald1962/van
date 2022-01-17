// SPDX-License-Identifier: GPL-2.0

/*
 * Test the van controller interworking with the battery device.
 *
 * Cobyright (C) 2022 Gerald Schueller <gerald.schueller@web.de>
 */
/*============================================================================
  IMPORTED INCLUDE REFERENCES
  ============================================================================*/
#include <unistd.h>  /* Standard Unix lib: usleep(). */
#include "os.h"      /* Operating system: os_sem_create(). */
#include "vote.h"    /* Van OS test environment. */

/*============================================================================
  EXPORTED INCLUDE REFERENCES
  ============================================================================*/
/*============================================================================
  LOCAL NAME CONSTANTS DEFINITIONS
  ============================================================================*/
/* Priority of the test threads. */
#define COB_PRIO   OS_THREAD_PRIO_FOREG /* Priority of the test threads. */
#define COB_QSIZE  4                    /* Input queue size. */

/*============================================================================
  MACROS
  ============================================================================*/
/*============================================================================
  LOCAL TYPE DEFINITIONS
  ============================================================================*/
/**
 * cob_stat_t - state of cob system.
 *
 * @van:        address of the van test thread.
 * @van_id:     id of the van device.
 * @van_sig:    van resume trigger.
 * @van_aio_c:  aio test counter.
 *
 * @py:         addess of the py test thread.
 * @py_id:      id of the python device.
 * @py_sig:     van resume trigger.
 * @py_aio_c:   py aio test counter.
 * @suspend:    control semaphore for the main process.
 **/
typedef struct {
	void   *van;
	int     van_id;
	int     van_sig;
	int     van_aio_c;
	
	void   *py;
	int     py_id;
	int     py_sig;
	int     py_aio_c;
	
	sem_t   suspend;
} cob_stat_t;

/*============================================================================
  LOCAL FUNCTION PROTOTYPES
  ============================================================================*/
static int cob_start(void);
static int cob_sync_1b(void);
static int cob_zsync_2048b(void);
static int cob_aio(void);
static int cob_stop(void);

/*============================================================================
  LOCAL DATA
  ============================================================================*/
/* Define vot_p for TEST_ASSERT_EQ. */
static test_stat_t *vote_p;

/* List of the of the van py interworking test cases. */
static test_elem_t cob_system[] = {
	{ TEST_ADD(cob_start), 0 },
	{ TEST_ADD(cob_sync_1b), 0 },
	{ TEST_ADD(cob_zsync_2048b), 0 },
	{ TEST_ADD(cob_aio), 0 },
	{ TEST_ADD(cob_stop), 0 },
	{ NULL, NULL, 0 }
};

/* State of the cob system. */
static cob_stat_t cob;

/*============================================================================
  LOCAL FUNCTIONS
  ============================================================================*/
/**
 * cob_py_signal() - the py test thread resumes the main process.
 *
 * Return:	None.
 **/
static void cob_py_signal(void)
{
	/* Resume the main process. */
	cob.py_sig = 1;
	os_sem_release(&cob.suspend);
}

/**
 * cob_van_signal() - the van test thread resumes the main process.
 *
 * Return:	None.
 **/
static void cob_van_signal(void)
{
	/* Resume the main process. */
	cob.van_sig = 1;
	os_sem_release(&cob.suspend);
}

/**
 * cob_wait() - the main process waits for the resume signals
 *
 * Return:	None.
 **/
static void cob_wait(void)
{
	/* Wait for the resume trigger. */
	while(! cob.van_sig || ! cob.py_sig)
		os_sem_wait(&cob.suspend);

	/* Reset the signals. */
	cob.van_sig = 0;
	cob.py_sig  = 0;
}

/**
 * cob_stop() - release the cob resources.
 *
 * Return:	the execution state.
 **/
static int cob_stop(void)
{
	os_statistics_t expected = { 5, 4, 0, 2330, 2330, 0 };
	int stat;

	/* Remove the py device. */
	os_c_close(cob.py_id);
	
	/* Remove the van device. */
	os_c_close(cob.van_id);
		
	/* Remove the test threads. */
	os_thread_destroy(cob.py);
	os_thread_destroy(cob.van);

	/* Remove the control semaphore. */
	os_sem_delete(&cob.suspend);
	
	/* Verify the OS state. */
	stat = test_os_stat(&expected);

	return stat;
}

/**
 * cob_aio() - test the asynchronous operations.
 *
 * Return:	the execution state.
 **/
static int cob_py_write_cb(int dev_id, char *buf, int count)
{
	/* Entry condition. */
	OS_TRAP_IF(buf == NULL || count != OS_BUF_SIZE);
	TEST_ASSERT_EQ(cob.py_id, dev_id);

	/* Test the release condition. */
	if (cob.py_aio_c >= 2) {
		/* Resume the main process. */
		cob_py_signal();
		return 0;
	}
	
	/* Generate UL data. */
	*buf = 'x';

	/* Update the test cycles. */
	cob.py_aio_c++;
	
	return 1;
}

static int cob_py_read_cb(int dev_id, char *buf, int count)
{
	/* Entry condition. */
	OS_TRAP_IF(buf == NULL || count < 1);
	TEST_ASSERT_EQ(cob.py_id, dev_id);
	
	/* Resume the main process. */
	cob_py_signal();

	return count;
}

static int cob_van_write_cb(int dev_id, char *buf, int count)
{
	/* Entry condition. */
	OS_TRAP_IF(buf == NULL || count != OS_BUF_SIZE);
	TEST_ASSERT_EQ(cob.van_id, dev_id);
	
	/* Test the release condition. */
	if (cob.van_aio_c >= 2) {
		/* Resume the main process. */
		cob_van_signal();
		return 0;
	}

	/* Generate DL data. */
	*buf = 'y';
	
	/* Update the test cycles. */
	cob.van_aio_c++;
	
	return 1;
}

static int cob_van_read_cb(int dev_id, char *buf, int count)
{
	/* Entry condition. */
	OS_TRAP_IF(buf == NULL || count < 1);
	TEST_ASSERT_EQ(cob.van_id, dev_id);
	
	return count;
}

static int cob_aio(void)
{
	os_statistics_t expected = { 13, 17, 4, 2330, 2326, 4 };
	os_aio_cb_t cb;
	int stat;

	/* Install the van async. operation. */
	cb.read_cb  = cob_van_read_cb;
	cb.write_cb = cob_van_write_cb;
	os_c_action(cob.van_id, &cb);
	
	/* Install the py async. operation. */
	cb.read_cb  = cob_py_read_cb;
	cb.write_cb = cob_py_write_cb;
	os_c_action(cob.py_id, &cb);

	/* Trigger the van aio callback. */
	os_c_awrite(cob.van_id);
	os_c_aread(cob.van_id);

	/* Trigger the py aio callback. */
	os_c_awrite(cob.py_id);
	os_c_aread(cob.py_id);
	
	/* Wait for the transfer operations. */
	cob_wait();

	/* Verify the OS state. */
	stat = test_os_stat(&expected);

	return stat;
}

/**
 * cob_zsync_2048b() - test the synchronous zero coby read operation with
 * OS_BUF_SIZE byte.
 *
 * Return:	the execution state.
 **/
/* py thread context */
static void cob_py_zsync_2048b(os_queue_elem_t *m)
{
	char buf[OS_BUF_SIZE], *zbuf;
	int n;
	
	/* py expects OS_BUF_SIZE bytes from van. */
	zbuf = NULL;
	n = os_c_zread(cob.py_id, &zbuf, OS_BUF_SIZE);
	TEST_ASSERT_EQ(n, OS_BUF_SIZE);

	/* Release the pending DL buffer. */
	n = os_c_zread(cob.py_id, NULL, 0);
	TEST_ASSERT_EQ(n, 0);

	/* py shall send OS_BUF_SIZE byte. */
	os_c_write(cob.py_id, buf, OS_BUF_SIZE);

	/* Resume the main process. */
	cob_py_signal();
}

/* van thread context */
static void cob_van_zsync_2048b(os_queue_elem_t *m)
{
	char buf[OS_BUF_SIZE], *zbuf;
	int n;
	
	/* van shall send OS_BUF_SIZE byte. */
	os_c_write(cob.van_id, buf, OS_BUF_SIZE);

	/* van expects OS_BUF_SIZE byte from py. */
	zbuf = NULL;
	n = os_c_zread(cob.van_id, &zbuf, OS_BUF_SIZE);
	TEST_ASSERT_EQ(n, OS_BUF_SIZE);

	/* Release the pending UL buffer. */
	n = os_c_zread(cob.van_id, NULL, 0);
	TEST_ASSERT_EQ(n, 0);
	
	/* Resume the main process. */
	cob_van_signal();
}

static int cob_zsync_2048b(void)
{
	os_statistics_t expected = { 13, 17, 4, 2330, 2326, 4 };
	os_queue_elem_t msg;
	int stat;

	/* Generate DL und consume UL data. */
	msg.cb = cob_van_zsync_2048b;
	OS_SEND(cob.van, &msg, sizeof(msg));

	msg.cb = cob_py_zsync_2048b;
	OS_SEND(cob.py, &msg, sizeof(msg));
	
	/* Wait for the transfer operations. */
	cob_wait();
	
	/* XXX Delay for os_free(). */
	usleep(10);

	/* Verify the OS state. */
	stat = test_os_stat(&expected);

	return stat;
}

/**
 * cob_sync_1b() - test the synchronous operation with 1 byte.
 *
 * Return:	the execution state.
 **/
/* py thread context */
static void cob_py_sync_1b(os_queue_elem_t *m)
{
	char buf[1];
	int n;
	
	/* py expects 1 byte from van. */
	n = os_c_read(cob.py_id, buf, 1);
	TEST_ASSERT_EQ(n, 1);
	
	/* py shall send 1 byte. */
	os_c_write(cob.py_id, buf, 1);

	/* Resume the main process. */
	cob_py_signal();
}

/* van thread context */
static void cob_van_sync_1b(os_queue_elem_t *m)
{
	char buf[1];
	int n;
	
	/* van shall send 1 byte. */
	os_c_write(cob.van_id, buf, 1);

	/* van expects 1 byte from py. */
	n = os_c_read(cob.van_id, buf, 1);
	TEST_ASSERT_EQ(n, 1);
	
	/* Resume the main process. */
	cob_van_signal();
}

static int cob_sync_1b(void)
{
	os_statistics_t expected = { 13, 17, 4, 2328, 2324, 4 };
	os_queue_elem_t msg;
	int stat;

	/* Generate DL und consume UL data. */
	msg.cb = cob_van_sync_1b;
	OS_SEND(cob.van, &msg, sizeof(msg));

	msg.cb = cob_py_sync_1b;
	OS_SEND(cob.py, &msg, sizeof(msg));
	
	/* Wait for the transfer operations. */
	cob_wait();
	
	/* XXX Delay for os_free(). */
	usleep(10);

	/* Verify the OS state. */
	stat = test_os_stat(&expected);

	return stat;
}

/**
 * cob_start() - create the van and py test thread.
 *
 * Return:	the execution state.
 **/
static int cob_start(void)
{
	os_statistics_t expected = { 13, 17, 4, 2326, 2322, 4 };
	int stat;

	/* Create the control semaphore for the main process. */
	os_sem_init(&cob.suspend, 0);
	
	/* Create the test threads. */
	cob.van = os_thread_create("controller", COB_PRIO, COB_QSIZE);
	cob.py  = os_thread_create("battery",    COB_PRIO, COB_QSIZE);

	/* Install the van device. */
	cob.van_id = os_c_open("/ctrl_batt", 0);
	
	/* Install the python device. */
	cob.py_id = os_c_open("/battery", 0);
	
	/* Verify the OS state. */
	stat = test_os_stat(&expected);

	return stat;
}

/*============================================================================
  EXPORTED FUNCTIONS
  ============================================================================*/
/**
 * cob_init() - define vot_p for TEST_ASSERT_EQ.
 *
 * @stat:  pointer to the test configuration.
 *
 * Return:	None.
 **/
void cob_init(test_stat_t *stat)
{
	vote_p = stat;
}

/**
 * cob_run() - test the van device interworking with the python device.
 *
 * Return:	None.
 **/
void cob_run(void)
{
	/* Execute the list of the test cases. */
	test_set_process(TEST_ADD(cob_system));
}
