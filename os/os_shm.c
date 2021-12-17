// SPDX-License-Identifier: GPL-2.0

/*
 * Shared memory interfaces.
 *
 * Copyright (C) 2021 Gerald Schueller <gerald.schueller@web.de>
 */

/*============================================================================
  IMPORTED INCLUDE REFERENCES
  ============================================================================*/
#include "os_shm.h"      /* Shared memory entry points. */

/*============================================================================
  EXPORTED INCLUDE REFERENCES
  ============================================================================*/
/*============================================================================
  LOCAL NAME CONSTANTS DEFINITIONS
  ============================================================================*/
#define OS_SHM_SIZE  8192        /* Expected size of the shm file. */

/*============================================================================
  MACROS
  ============================================================================*/
/*============================================================================
  LOCAL TYPE DEFINITIONS
  ============================================================================*/
/*============================================================================
  LOCAL DATA
  ============================================================================*/
/* Pointer to the OS configuration */
static os_conf_t *os_conf_p;

/* List of the shared memory devices. */
static os_dev_ops_t *os_dev_ops[OS_DEV_COUNT];

/*============================================================================
  LOCAL FUNCTION PROTOTYPES
  ============================================================================*/
/*============================================================================
  LOCAL FUNCTIONS
  ============================================================================*/
/**
 * os_dev_ops_get() - search for the entry points of the shm device.
 *
 * @dev_id:  id of the shared memory device.
 *
 * Return:	the list of the shm device operations.
 **/
static os_dev_ops_t *os_dev_ops_get(int dev_id)
{
	os_dev_ops_t **elem;
	int i;
	
	/* Search for the shared memory devices. */
	for (elem = os_dev_ops, i = OS_DEV_VAN; i < OS_DEV_COUNT;i++, *elem++) {
		/* Test the device id. */
		if (*elem != NULL && (*elem)->device_id == dev_id)
			break;
	}
	
	/* Test the device state. */
	OS_TRAP_IF(i >= OS_DEV_COUNT);

	return *elem;
}

/**
 * os_dev_ops_add() - extend the list of the shared memory devices.
 *
 * @op:  list of the shm device entry points.
 *
 * Return:	None.
 **/
static void os_dev_ops_add(os_dev_ops_t *op)
{
	os_dev_ops_t **elem;
	int i;

	/* Search for a free device element. */
	for (elem = os_dev_ops, i = OS_DEV_VAN;
	     i < OS_DEV_COUNT && *elem != NULL; i++, *elem++)
		;

	/* End condition. */
	OS_TRAP_IF(i >= OS_DEV_COUNT);

	/* Add the new shm element. */
	os_dev_ops[i] = OS_MALLOC(sizeof(os_dev_ops_t));
	os_memcpy(os_dev_ops[i], sizeof(os_dev_ops_t), op,
		  sizeof(os_dev_ops_t));
}

/*============================================================================
  EXPORTED FUNCTIONS
  ============================================================================*/
/**
 * os_open() - the van or py process shall call this function to request the
 * resources for the shared memory transer.
 *
 * @device_name:  name of the shared memory deivce: "/van" or "/python".
 *
 * Return:	the device id.
 **/
int os_open(char *device_name)
{
	os_dev_ops_t **elem;
	int i;
	
	/* Entry condition. */
	OS_TRAP_IF(device_name == NULL);
	
	/* Search for the shared memory devices. */
	for (elem = os_dev_ops, i = OS_DEV_VAN; i < OS_DEV_COUNT;i++, *elem++) {
		/* Test the device name. */
		if (*elem != NULL &&
		    os_strcmp((*elem)->device_name, device_name) == 0)
			break;
	}

	/* Test the device state. */
	OS_TRAP_IF(i >= OS_DEV_COUNT);

	return (*elem)->open(device_name);
}

/**
 * os_close() - the van or py process shall call this function to remove the
 * shared memory ressources.
 *
 * @dev_id:  van or py device id.
 *
 * Return:	None.
 **/
void os_close(int dev_id)
{
	os_dev_ops_t *elem;

	/* Search for the list of the shm device enty points. */
	elem = os_dev_ops_get(dev_id);
	elem->close(dev_id);
}

/**
 * os_sync_write() - send the DL payload to py or the UL payload to van and
 * suspend the caller until the payload has been processed.
 *
 * @dev_id:  id of the shared memory device.
 * @buf:     pointer to the payload.
 * @count:   size of the payload.
 *
 * Return:	None.
 **/
void os_sync_write(int dev_id, char *buf, int count)
{
	os_dev_ops_t *elem;

	/* Search for the list of the shm device enty points. */
	elem = os_dev_ops_get(dev_id);
	elem->sync_write(dev_id, buf, count);
}

/**
 * os_sync_zread() - py or van waits for incoming payload. With each successiv call, the reference to the
 * previous call is released automatically.
 *
 * @dev_id:  id of the shared memory device.
 * @buf:     pointer to the received payload.
 * @count:   size of the destination buffer.
 *
 * Return:	number of the received bytes.
 **/
int os_sync_zread(int dev_id, char **buf, int count)
{
	os_dev_ops_t *elem;

	/* Search for the list of the shm device enty points. */
	elem = os_dev_ops_get(dev_id);
	return elem->sync_zread(dev_id, buf, count);
}

/**
 * os_sync_read() - py or van waits for incoming payload.
 *
 * @dev_id:  id of the shared memory device.
 * @buf:     pointer to the received payload.
 * @count:   size of the destination buffer.
 *
 * Return:	number of the received bytes.
 **/
int os_sync_read(int dev_id, char *buf, int count)
{
	os_dev_ops_t *elem;

	/* Search for the list of the shm device enty points. */
	elem = os_dev_ops_get(dev_id);
	return elem->sync_read(dev_id, buf, count);
}

/**
 * os_shm_close() - delete the access and the mapping to the shared memory.
 *
 * @s:  pointer to the shared memory state.
 *
 * Return:	None.
 **/
void os_shm_close(os_dev_t *s)
{
	int rv;
	
	/* Remove the reference to the van semaphore. */
	rv = sem_close(s->my_int);
	OS_TRAP_IF(rv != 0);
	
	/* Remove the reference to the python semaphore. */
	rv = sem_close(s->other_int);
	OS_TRAP_IF(rv != 0);

	/* Delete the mapping of the shared memory area. */
	rv = munmap(s->start, s->size);
	
	/* Close the shared memory file. */
	rv = close(s->fd);
	OS_TRAP_IF(rv < 0);

	/* Destroy the mutex for the critical sections in os_queue_add. */
	os_cs_destroy(&s->top.q_mutex);
	
	/* Delete the mutex for the critical sections in os_sync_write. */
	os_cs_destroy(&s->write_mutex);

	/* Delete the mutex for the critical sections in os_sync_zread. */
	os_cs_destroy(&s->read_mutex);

	/* Destroy the semaphore for os_sync_write(). */
	os_sem_delete(&s->suspend_writer);
	
	/* Destroy the semaphore for os_sync_zread(). */
	os_sem_delete(&s->suspend_reader);
}

/**
 * os_shm_open() - map the file into shared memory.
 *
 * @s:  pointer to the shared memory state.
 *
 * Return:	None.
 **/
void os_shm_open(os_dev_t *s)
{
	os_shm_top_t *t;
	struct stat statbuf;
	int    rv, prot;
	void  *p;

	/* Open the shared memory file. */
	s->file_name = OS_SHM_FILE;
	s->fd = open(OS_SHM_FILE, O_RDWR);
	OS_TRAP_IF(s->fd < 0);

	/* Get information about the shared memory file. */
	rv = fstat(s->fd, &statbuf);
	s->size = statbuf.st_size;
	OS_TRAP_IF(rv != 0 || statbuf.st_size != OS_SHM_SIZE);

	/* Define the access to the shared memory. */
	prot = PROT_READ | PROT_WRITE;
	
	/* Map the file into shared memory. */
	s->start = mmap(NULL, s->size, prot, MAP_SHARED, s->fd, 0);
	OS_TRAP_IF(s->start == MAP_FAILED);

	/* Create the mutex for the critical sections in os_sync_write. */
	os_cs_init(&s->write_mutex);
	
	/* Create the mutex for the critical sections in os_sync_zread. */
	os_cs_init(&s->read_mutex);
	
	/* Get the reference to the shm topology. */
	t = &s->top;

	/* Copy the pointer to the start of the shm. */
	p = s->start;

	/* Map the data transfer queues to the shm. */
	t->dl_queue = (os_shm_queue_t *) p;
	p = p + sizeof(os_shm_queue_t);
	
	t->ul_queue = (os_shm_queue_t *) p;
	p = p + sizeof(os_shm_queue_t);
	
	/* Map the UL buffer to the shm. */
	t->ul_size  = OS_BUF_SIZE;
	t->ul_start = (char *) p;
	t->ul_end   = (char *) p + OS_BUF_SIZE - 1;
	p = (char *) p + OS_BUF_SIZE;
	
	/* Map the DL buffer to the shm. */
	t->dl_size  = OS_BUF_SIZE;
	t->dl_start = (char *) p;
	t->dl_end   = (char *) p + OS_BUF_SIZE - 1;

	/* Create the mutex for the critical sections in os_queue_add. */
	os_cs_init(&t->q_mutex);
	
	/* Create the semaphore for os_sync_write(). */
	os_sem_init(&s->suspend_writer, 0);
	
	/* Create the semaphore for os_sync_zread(). */
	os_sem_init(&s->suspend_reader, 0);
}

/**
 * os_shm_init() - trigger the installation of the shared memory devices.
 *
 * @conf:  pointer to the trace configuration.
 *
 * Return:	None.
 **/
void os_shm_init(os_conf_t *conf)
{
	os_dev_ops_t op;
	
	/* Save the reference to the OS configuration. */
	os_conf_p = conf;

	/* Trigger the installation of the shared memory devices. */
	os_van_init(conf, &op);
	os_dev_ops_add(&op);
	
	os_py_init(conf, &op);
	os_dev_ops_add(&op);
}

/**
 * os_shm_exit() - test the state of the shared memory devices.
 *
 * Return:	None.
 **/
void os_shm_exit(void)
{
	os_dev_ops_t **elem;
	int i;

	/* Test the state of the shared memory devices. */
	os_van_exit();
	os_py_exit();

	/* Free the shared memory devices. */
	for (elem = os_dev_ops, i = OS_DEV_VAN; i < OS_DEV_COUNT; i++, *elem++) {
		if (*elem != NULL)
			OS_FREE(*elem);
	}
}
