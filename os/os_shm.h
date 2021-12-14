/* SPDX-License-Identifier: GPL-2.0 */
#ifndef __os_shm_h__
#define __os_shm_h__

/*============================================================================
  IMPORTED INCLUDE REFERENCES
  ============================================================================*/
#include <fcntl.h>       /* For O_* constants. */
#include <sys/stat.h>    /* For mode constants. */
#include <unistd.h>      /* File operationens: close(). */
#include <sys/mman.h>    /* Map file into memory. */
#include <errno.h>       /* Number of the last error: errno. */
#include "os.h"          /* Operating system: os_sem_create() */

/*============================================================================
  NAME CONSTANTS DEFINITIONS
  ============================================================================*/
/*============================================================================
  MACROS
  ============================================================================*/
/*============================================================================
  TYPE DEFINITIONS
  ============================================================================*/
/* Shared memory access functions or operations. */
/**
 * os_shm_ops_t - shared memory access functions or operations.
 *
 * @open:        create the shared memory device.
 * @close:       destroy the shared memory device.
 * @sync_write:  send data above the shm channel.
 * @sync_pread:  zero-copy request for data access to the receive channel.
 * @sync_read:   copy data from the receive channel.
 **/
typedef struct {
	int (*open) (char *device_name);
	void (*close) (int dev_id);
	void (*sync_write) (int dev_id, char *buf, int count);
	int (*sync_pread) (int dev_id, char **buf, int count);
	int (*sync_read) (int dev_id, char *buf, int count);
} os_shm_ops_t;
	
/*============================================================================
  GLOBAL DATA
  ============================================================================*/
/*============================================================================
  EXPORTED FUNCTIONS
  ============================================================================*/
#endif /* __os_shm_h__ */
