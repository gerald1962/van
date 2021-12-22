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
/*============================================================================
  MACROS
  ============================================================================*/
/*============================================================================
  LOCAL TYPE DEFINITIONS
  ============================================================================*/
/*============================================================================
  LOCAL FUNCTION PROTOTYPES
  ============================================================================*/
static int vip_open_van(void);
static int vip_open_py(void);
static int vip_sync_write(void);

/*============================================================================
  LOCAL DATA
  ============================================================================*/
/* Define vot_p for TEST_ASSERT_EQ. */
static test_stat_t *vote_p;

/* List of the of the van py interworking test cases. */
static test_elem_t vip_system[] = {
	{ TEST_ADD(vip_open_van), 0 },
	{ TEST_ADD(vip_open_py), 0 },
//	{ TEST_ADD(vip_sync_write), 0 },
	{ NULL, NULL, 0 }
};

/*============================================================================
  LOCAL FUNCTIONS
  ============================================================================*/
/**
 * vip_sync_write() - test the synchronous write operation.
 *
 * Return:	the execution state.
 **/
static int vip_sync_write(void)
{
	os_statistics_t expected = { 2, 2, 0, 2318, 2316, 0 };
	int dev_id, stat;
	char buf[1];

	/* Install the van shared memory device. */
	dev_id = os_open("/van");

	/* Send a DL buffer to py. */
	os_write(dev_id, buf, 1);

	/* Remove the van shared memory device. */
	os_close(dev_id);
	
	/* Verify the OS state. */
	stat = test_os_stat(&expected);

	return stat;
}

/**
 * vip_open_py() - open and close the python device.
 *
 * Return:	the execution state.
 **/
static int vip_open_py(void)
{
	/* XXX */
#if 0
	os_statistics_t expected = { 2, 2, 0, 2324, 2322, 0 };
	int van_id, py_id, stat;

	/* Install the van and python device. */
	van_id = os_open("/van");
	py_id = os_open("/python");
	
	/* Remove the py and van device. */
	os_close(py_id);
	os_close(van_id);
	
	/* Verify the OS state. */
	stat = test_os_stat(&expected);

	return stat;
#else
	os_statistics_t expected = { 2, 2, 0, 2324, 2322, 0 };

	int dev_id, stat;

	/* Install the van shared memory device. */
	dev_id = os_open("/python");
	
	/* Verify the OS state. */
	stat = test_os_stat(&expected);

	return stat;
#endif
}

/**
 * vip_open_van() - open and close the van device.
 *
 * Return:	the execution state.
 **/
static int vip_open_van(void)
{
	os_statistics_t expected = { 2, 2, 0, 2318, 2316, 0 };
	int dev_id, stat;

	/* Install the van shared memory device. */
	dev_id = os_open("/van");
	
	/* Remove the van shared memory device. */
	os_close(dev_id);
	
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
