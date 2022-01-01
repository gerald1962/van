// SPDX-License-Identifier: GPL-2.0

/*
 * Test the van device interworking with the python device.
 *
 * Copyright (C) 2021 Gerald Schueller <gerald.schueller@web.de>
 */
/*============================================================================
  IMPORTED INCLUDE REFERENCES
  ============================================================================*/
#include "os.h"    /* Operating system: os_sem_create(). */
#include "vote.h"  /* Van OS test environment. */

/*============================================================================
  EXPORTED INCLUDE REFERENCES
  ============================================================================*/
/*============================================================================
  LOCAL NAME CONSTANTS DEFINITIONS
  ============================================================================*/
/* Priority of the test threads. */
#define VIP_PRIO   OS_THREAD_PRIO_FOREG /* Priority of the test threads. */
#define VIP_QSIZE  4                    /* Input queue size. */

/*============================================================================
  MACROS
  ============================================================================*/
/*============================================================================
  LOCAL TYPE DEFINITIONS
  ============================================================================*/
/**
 * vip_stat_t - state of vip system.
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
} vip_stat_t;

/*============================================================================
  LOCAL FUNCTION PROTOTYPES
  ============================================================================*/
static int vip_start(void);
static int vip_sync_1b(void);
static int vip_zsync_2048b(void);
static int vip_aio(void);
static int vip_stop(void);

/*============================================================================
  LOCAL DATA
  ============================================================================*/
/* Define vot_p for TEST_ASSERT_EQ. */
static test_stat_t *vote_p;

/* List of the of the van py interworking test cases. */
static test_elem_t vip_system[] = {
	{ TEST_ADD(vip_start), 0 },
	{ TEST_ADD(vip_sync_1b), 0 },
	{ TEST_ADD(vip_zsync_2048b), 0 },
	{ TEST_ADD(vip_aio), 0 },
	{ TEST_ADD(vip_stop), 0 },
	{ NULL, NULL, 0 }
};

/* State of the vip system. */
static vip_stat_t vip;

/*============================================================================
  LOCAL FUNCTIONS
  ============================================================================*/
/**
 * vip_py_signal() - the py test thread resumes the main process.
 *
 * Return:	None.
 **/
static void vip_py_signal(void)
{
	/* Resume the main process. */
	vip.py_sig = 1;
	os_sem_release(&vip.suspend);
}

/**
 * vip_van_signal() - the van test thread resumes the main process.
 *
 * Return:	None.
 **/
static void vip_van_signal(void)
{
	/* Resume the main process. */
	vip.van_sig = 1;
	os_sem_release(&vip.suspend);
}

/**
 * vip_wait() - the main process waits for the resume signals
 *
 * Return:	None.
 **/
static void vip_wait(void)
{
	/* Wait for the resume trigger. */
	while(! vip.van_sig || ! vip.py_sig)
		os_sem_wait(&vip.suspend);

	/* Reset the signals. */
	vip.van_sig = 0;
	vip.py_sig  = 0;
}

/**
 * vip_stop() - release the vip resources.
 *
 * Return:	the execution state.
 **/
static int vip_stop(void)
{
	os_statistics_t expected = { 2, 2, 0, 2330, 2330, 0 };
	int stat;

	/* Remove the py device. */
	os_close(vip.py_id);
	
	/* Remove the van device. */
	os_close(vip.van_id);
		
	/* Remove the test threads. */
	os_thread_destroy(vip.py);
	os_thread_destroy(vip.van);

	/* Remove the control semaphore. */
	os_sem_delete(&vip.suspend);
	
	/* Verify the OS state. */
	stat = test_os_stat(&expected);

	return stat;
}

/**
 * vip_aio() - test the asynchronous operations.
 *
 * Return:	the execution state.
 **/
static int vip_py_write_cb(int dev_id, char *buf, int count)
{
	/* Entry condition. */
	OS_TRAP_IF(buf == NULL || count != OS_BUF_SIZE);
	TEST_ASSERT_EQ(vip.py_id, dev_id);

	/* Test the release condition. */
	if (vip.py_aio_c >= 2) {
		/* Resume the main process. */
		vip_py_signal();
		return 0;
	}
	
	/* Generate UL data. */
	*buf = 'x';

	/* Update the test cycles. */
	vip.py_aio_c++;
	
	return 1;
}

static int vip_py_read_cb(int dev_id, char *buf, int count)
{
	/* Entry condition. */
	OS_TRAP_IF(buf == NULL || count < 1);
	TEST_ASSERT_EQ(vip.py_id, dev_id);
	
	/* Resume the main process. */
	vip_py_signal();

	return count;
}

static int vip_van_write_cb(int dev_id, char *buf, int count)
{
	/* Entry condition. */
	OS_TRAP_IF(buf == NULL || count != OS_BUF_SIZE);
	TEST_ASSERT_EQ(vip.van_id, dev_id);
	
	/* Test the release condition. */
	if (vip.van_aio_c >= 2) {
		/* Resume the main process. */
		vip_van_signal();
		return 0;
	}

	/* Generate DL data. */
	*buf = 'y';
	
	/* Update the test cycles. */
	vip.van_aio_c++;
	
	return 1;
}

static int vip_van_read_cb(int dev_id, char *buf, int count)
{
	/* Entry condition. */
	OS_TRAP_IF(buf == NULL || count < 1);
	TEST_ASSERT_EQ(vip.van_id, dev_id);
	
	return count;
}

static int vip_aio(void)
{
	os_statistics_t expected = { 10, 10, 4, 2330, 2326, 4 };
	os_aio_cb_t cb;
	int stat;

	/* Install the van async. operation. */
	cb.read_cb  = vip_van_read_cb;
	cb.write_cb = vip_van_write_cb;
	os_aio_action(vip.van_id, &cb);
	
	/* Install the py async. operation. */
	cb.read_cb  = vip_py_read_cb;
	cb.write_cb = vip_py_write_cb;
	os_aio_action(vip.py_id, &cb);

	/* Trigger the van aio callback. */
	os_aio_write(vip.van_id);
	os_aio_read(vip.van_id);

	/* Trigger the py aio callback. */
	os_aio_write(vip.py_id);
	os_aio_read(vip.py_id);
	
	/* Wait for the transfer operations. */
	vip_wait();

	/* Verify the OS state. */
	stat = test_os_stat(&expected);

	return stat;
}

/**
 * vip_zsync_2048b() - test the synchronous zero copy read operation with
 * OS_BUF_SIZE byte.
 *
 * Return:	the execution state.
 **/
/* py thread context */
static void vip_py_zsync_2048b(os_queue_elem_t *m)
{
	char buf[OS_BUF_SIZE], *zbuf;
	int n;
	
	/* py expects OS_BUF_SIZE bytes from van. */
	zbuf = NULL;
	n = os_zread(vip.py_id, &zbuf, OS_BUF_SIZE);
	TEST_ASSERT_EQ(n, OS_BUF_SIZE);

	/* Release the pending DL buffer. */
	n = os_zread(vip.py_id, NULL, 0);
	TEST_ASSERT_EQ(n, 0);

	/* py shall send OS_BUF_SIZE byte. */
	os_write(vip.py_id, buf, OS_BUF_SIZE);

	/* Resume the main process. */
	vip_py_signal();
}

/* van thread context */
static void vip_van_zsync_2048b(os_queue_elem_t *m)
{
	char buf[OS_BUF_SIZE], *zbuf;
	int n;
	
	/* van shall send OS_BUF_SIZE byte. */
	os_write(vip.van_id, buf, OS_BUF_SIZE);

	/* van expects OS_BUF_SIZE byte from py. */
	zbuf = NULL;
	n = os_zread(vip.van_id, &zbuf, OS_BUF_SIZE);
	TEST_ASSERT_EQ(n, OS_BUF_SIZE);

	/* Release the pending UL buffer. */
	n = os_zread(vip.van_id, NULL, 0);
	TEST_ASSERT_EQ(n, 0);
	
	/* Resume the main process. */
	vip_van_signal();
}

static int vip_zsync_2048b(void)
{
	os_statistics_t expected = { 10, 10, 4, 2330, 2326, 4 };
	os_queue_elem_t msg;
	int stat;

	/* Generate DL und consume UL data. */
	msg.cb = vip_van_zsync_2048b;
	OS_SEND(vip.van, &msg, sizeof(msg));

	msg.cb = vip_py_zsync_2048b;
	OS_SEND(vip.py, &msg, sizeof(msg));
	
	/* Wait for the transfer operations. */
	vip_wait();
	
	/* Verify the OS state. */
	stat = test_os_stat(&expected);

	return stat;
}

/**
 * vip_sync_1b() - test the synchronous operation with 1 byte.
 *
 * Return:	the execution state.
 **/
/* py thread context */
static void vip_py_sync_1b(os_queue_elem_t *m)
{
	char buf[1];
	int n;
	
	/* py expects 1 byte from van. */
	n = os_read(vip.py_id, buf, 1);
	TEST_ASSERT_EQ(n, 1);
	
	/* py shall send 1 byte. */
	os_write(vip.py_id, buf, 1);

	/* Resume the main process. */
	vip_py_signal();
}

/* van thread context */
static void vip_van_sync_1b(os_queue_elem_t *m)
{
	char buf[1];
	int n;
	
	/* van shall send 1 byte. */
	os_write(vip.van_id, buf, 1);

	/* van expects 1 byte from py. */
	n = os_read(vip.van_id, buf, 1);
	TEST_ASSERT_EQ(n, 1);
	
	/* Resume the main process. */
	vip_van_signal();
}

static int vip_sync_1b(void)
{
	os_statistics_t expected = { 10, 10, 4, 2328, 2324, 4 };
	os_queue_elem_t msg;
	int stat;

	/* Generate DL und consume UL data. */
	msg.cb = vip_van_sync_1b;
	OS_SEND(vip.van, &msg, sizeof(msg));

	msg.cb = vip_py_sync_1b;
	OS_SEND(vip.py, &msg, sizeof(msg));
	
	/* Wait for the transfer operations. */
	vip_wait();
	
	/* Verify the OS state. */
	stat = test_os_stat(&expected);

	return stat;
}

/**
 * vip_start() - create the van and py test thread.
 *
 * Return:	the execution state.
 **/
static int vip_start(void)
{
	os_statistics_t expected = { 10, 10, 4, 2326, 2322, 4 };
	int stat;

	/* Create the control semaphore for the main process. */
	os_sem_init(&vip.suspend, 0);
	
	/* Create the test threads. */
	vip.van = os_thread_create("van", VIP_PRIO, VIP_QSIZE);
	vip.py  = os_thread_create("py", VIP_PRIO, VIP_QSIZE);

	/* Install the van device. */
	vip.van_id = os_open("/van_py", 0);
	
	/* Install the python device. */
	vip.py_id = os_open("/python", 0);
	
	/* Verify the OS state. */
	stat = test_os_stat(&expected);

	return stat;
}

/*============================================================================
  EXPORTED FUNCTIONS
  ============================================================================*/
/**
 * vip_init() - define vot_p for TEST_ASSERT_EQ.
 *
 * Return:	None.
 **/
void vip_init(test_stat_t *stat)
{
	vote_p = stat;
}

/**
 * vip_run() - test the van device interworking with the python device.
 *
 * Return:	None.
 **/
void vip_run(void)
{
	/* Execute the list of the test cases. */
	test_set_process(TEST_ADD(vip_system));
}
