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
#include "os_private.h"  /* Local interfaces of the OS: os_trap_init() */

/*============================================================================
  NAME CONSTANTS DEFINITIONS
  ============================================================================*/
#define OS_SHM_FILE  "/tmp/shm.txt"  /* Name of the shared memory file. */
#define OS_VAN_INT   "/van_int"      /* Van interrupt simulation. */
#define OS_PY_INT    "/py_int"       /* Python interrupt simulation. */
#define OS_SHM_SIZE  8192            /* Size of the shm file. */


#define OS_SHM_Q_SIZE     4  /* Data transfer queue size about shm. */
#define OS_THREAD_Q_SIZE  8  /* Input queue size of the van/py thread. */

/*============================================================================
  MACROS
  ============================================================================*/
/*============================================================================
  TYPE DEFINITIONS
  ============================================================================*/
/**
 * os_dev_ops_t - shared memory access functions or operations.
 *
 * @open:        create the shared memory device.
 * @close:       destroy the shared memory device.
 * @write:  send data above the shm channel.
 * @zread:  zero-copy request for data access to the receive channel.
 * @read:   copy data from the receive channel.
 **/
typedef struct os_dev_ops_s {
	char *device_name;
	int device_id;
	int (*open) (char *device_name);
	void (*close) (int dev_id);
	void (*write) (int dev_id, char *buf, int count);
	int (*zread) (int dev_id, char **buf, int count);
	int (*read) (int dev_id, char *buf, int count);
	void (*aio_action)(int dev_id, os_aio_cb_t *cb);
	void (*aio_write)(int dev_id);
	void (*aio_read)(int dev_id);
} os_dev_ops_t;
	
/**
 * os_shm_msg_t - shm input message with payload status information.
 *
 * @size:      size of the payload.
 * @consumed:  if 1, the payload has been processed.
 **/
typedef struct {
	int  size;
	int  consumed;
} os_shm_msg_t;

/**
 * os_shm_queue_t - shm input queue of van or py.
 *
 * @ring:  list of the input messages.
 * @tail:  start of the message list.
 * @head:  end of the message list.
 **/
typedef struct {
	os_shm_msg_t  ring[OS_SHM_Q_SIZE];
	int  tail;
	int  head;
} os_shm_queue_t;

/** 
 * os_shm_top_t - shm topology.
 *
 * @mutex:     critical section in os_queue_add.
 * @dl_queue:  python shm queue: van -> py.
 * @dl_count:  size of the DL payload: van->py.
 * @dl_size:   size of the DL buffer: van->py.
 * @dl_start:  start address of the DL buffer.
 * @dl_end:    end address of the DL buffer.
 * @ul_queue:  van shm queue: py -> van.
 * @ul_count   size of the UL payload: py->van.
 * @ul_size:   size of the UL buffer: py->van.
 * @ul_start:  start address of the UL buffer.
 * @ul_end:    end address of the UL buffer.
 **/
typedef struct {
	pthread_mutex_t  q_mutex;
	os_shm_queue_t  *dl_queue;
	atomic_int       dl_count;
	int    dl_size;
	char  *dl_start;
	char  *dl_end;
	os_shm_queue_t  *ul_queue;
	atomic_int       ul_count;
	int    ul_size;
	char  *ul_start;
	char  *ul_end;
} os_shm_top_t;

/**
 * os_dev_t - shared memory device state.
 *
 * @id:                van or py device.
 * @name:              name of the shared memory device.
 * @init:              1, if the shm area is available.
 * @file_name:         name of the shared memory file.
 * @fd:                file descriptor of the shm file.
 * @size:              size of the shm file.
 * @start:             start address of the mapped shm area.
 * @my_int_name:       name of the van/py interrupt.
 * @other_int_name:    name of the py/van interrupt.
 * @my_int:            points to the named van/py semaphore.
 * @other_int:         points to the named py/van semaphore.
 * @thread:            address of the van/py int handler/thread.
 * @use_aio:           if 1, the aio actions shall be executed.
 * @suspend_writer:    suspend the write caller in write.
 * @suspend_reader:    suspend the read caller in read.
 * @aio_cb:            aio read and write callbacks.
 * @aio_wr_trigger:    if 1, the van/py handler shall invoke the aio write_cb.
 * @aio_rd_trigger:    if 1, the van/py handler shall invoke the aio read_cb.
 * @pending_ul:        if 1, the UL transfer buffer is pending.
 * @pending_dl:        if 1, the DL transfer buffer is pending.
 * @down:              if 1, ignore the py/van interrupt.
 * @sync_write:        if 1, the user has invoked write.
 * @sync_read:         if 1, the user has invoked read.
 * @write_mutex:       protect the critical sections in write.
 * @read_mutex:        protect the critical sections in read.
 * @top:               topology of the shm area.
 **/
typedef struct {
	os_dev_type_t  id;
	char   *name;
	int     init;
	char   *file_name;
	int     fd;
	off_t   size;
	void   *start;
	char   *my_int_name;
	char   *other_int_name;
	sem_t  *my_int;
	sem_t  *other_int;
	void   *thread;
	sem_t   suspend_writer;
	sem_t   suspend_reader;

	os_aio_cb_t      aio_cb;
	atomic_int       aio_use;
	atomic_int       aio_wr_trigger;
	atomic_int       aio_rd_trigger;

	atomic_int       pending_ul;
	atomic_int       pending_dl;
	atomic_int       down;
	atomic_int       sync_read;
	atomic_int       sync_write;
	pthread_mutex_t  write_mutex;
	pthread_mutex_t  read_mutex;
	pthread_mutex_t  aio_mutex;
	os_shm_top_t     top;
} os_dev_t;

/*============================================================================
  GLOBAL DATA
  ============================================================================*/
/*============================================================================
  EXPORTED FUNCTIONS
  ============================================================================*/
void os_shm_open(os_dev_t *s);
void os_shm_close(os_dev_t *s);

void os_van_init(os_conf_t *conf, os_dev_ops_t *op);
void os_van_exit(void);
void os_van_ripcord(int coverage);

void os_py_init(os_conf_t *conf, os_dev_ops_t *op);
void os_py_exit(void);
void os_py_ripcord(int coverage);

#endif /* __os_shm_h__ */
