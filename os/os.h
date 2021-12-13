/* SPDX-License-Identifier: GPL-2.0 */
#ifndef __os_h__
#define __os_h__

/*============================================================================
  IMPORTED INCLUDE REFERENCES
  ============================================================================*/
#include <stdio.h>      /* Standard C library:   printf(). */
#include <semaphore.h>  /* Semaphore operations: sem_t. */

/*============================================================================
  NAME CONSTANTS DEFINITIONS
  ============================================================================*/
/* Name of the current funtion. */
#define F  __FUNCTION__

/* Support 1 MB for the standard string operations. */
#define OS_MAX_STRING_LEN  1048576

/* Maximum length of a name strings. */
#define OS_MAX_NAME_LEN  16

/* Number of the supported threads. */
#define OS_THREAD_LIMIT  5

/* Limit of the thread input queue. */
#define OS_QUEUE_LIMIT  1024

/* Limit of the os_malloc calls without correspondig ms_free calls. */
#define OS_MALLOC_LIMIT     512

/* Limit of the van files with os_malloc calls. */
#define OS_MALLOC_FILE_LIMIT  3

/*============================================================================
  MACROS
  ============================================================================*/

/* Trigger a core dump */
#define OS_TRAP() \
    os_trap(__FILE__, F, __LINE__)

#define OS_TRAP_IF(cond_) \
    do { \
        if (cond_) { \
            OS_TRAP(); \
        } \
    } while (0)

/* Wrapper for os_mallo() and os_free. */
#define OS_MALLOC(size_)  os_malloc((size_), __FILE__, __LINE__)
#define OS_FREE(ptr_)     do { os_free((void **) &(ptr_)); } while(0);

/* Wrapper to send a message to any thread. */
#define OS_SEND(thread_, msg_, size_) do { \
        os_queue_send((thread_), (os_queue_elem_t *) (msg_), (size_)); \
} while(0)

/*============================================================================
  TYPE DEFINITIONS
  ============================================================================*/

/* Map the spinlock to mutex. */
typedef pthread_mutex_t spinlock_t;

/**
 * os_thread_prio_t - supported thread priorities.
 *
 * @OS_THREAD_PRIO_HARDRT:   hard realtime priority.
 * @OS_THREAD_PRIO_SOFTRT:   soft realtime priority.
 * @OS_THREAD_PRIO_BACKG:    background priority for applications.
 * @OS_THREAD_PRIO_FOREG:    foreground priority for applications.
 **/
typedef enum {
	OS_THREAD_PRIO_HARDRT  = 99,
	OS_THREAD_PRIO_SOFTRT  = 50,
	OS_THREAD_PRIO_BACKG   = 40,
	OS_THREAD_PRIO_FOREG   = 35,
	OS_THREAD_PRIO_DEFAULT = 5
} os_thread_prio_t;

/* Forward declaration of the generic message. */
typedef struct os_queue_elem_s os_queue_elem_t;

/* Callback for the received message. */
typedef void os_queue_cb_t(os_queue_elem_t *msg);

/**
 * OS_QUEUE_MSG_HEAD - generic message header.
 *
 * @next:   pointer to the next queue element.
 * @param:  generic message parameter.
 * @cb:     callback for the message processing.
 **/
#define OS_QUEUE_MSG_HEAD \
    struct os_queue_elem_s  *next; \
    void           *param; \
    os_queue_cb_t  *cb;

struct os_queue_elem_s {
    OS_QUEUE_MSG_HEAD;
};

/**
 * os_statistics_t - overall state of the OS.
 *
 * @cs_count:   number of created mutexes.
 * @sem_count:  number of created semaphores.
 * @spin_count: number of created spin locks.
 * @malloc_c:   number of the os_malloc calls.
 * @free_c:     number of the os_free calls.
 * @thread_c:   number of the installed threads.
 **/
typedef struct {
	int cs_count;
	int sem_count;
	int spin_count;
        int malloc_c;
        int free_c;
	int thread_c;
} os_statistics_t;

/**
 * os_dev_type_t - ids of the shared memory devices.
 *
 * OS_DEV_VAN:    id of the van shared memory device.
 * OS_DEV_PY:     id of the py shared memory device.
 * OS_DEV_COUNT:  number of the shared memory devices.
 **/
typedef enum {
	OS_DEV_VAN,
	OS_DEV_PY,
	OS_DEV_COUNT
} os_dev_type_t;
	
/*============================================================================
  GLOBAL DATA
  ============================================================================*/
/*============================================================================
  EXPORTED FUNCTIONS
  ============================================================================*/

void os_statistics(os_statistics_t *stat);

/* Bootstrapping. */
void os_init(void);
void os_exit(void);

/* Trace handling. */
void os_trace_button(int n);

/* Trap handling. */
void os_trap(char *file, const char *function, unsigned long line);

/* Dynamic memory. */
void *os_malloc(size_t size, char *file, unsigned long line);
void os_free(void **ptr);

/* Memory and string. */
void *os_memset(void *s, int c, size_t n);
void *os_memcpy(void *dest, size_t dest_n, const void *src, size_t src_n);
int os_memcmp(const void *s1, const void *s2, size_t n);
size_t os_strnlen(const char *s, size_t maxlen);
size_t os_strlen(const char *s);
char *os_strcpy(char *dest, int dest_n, const char *src);
int os_strcmp(const char *s1, const char *s2);

/* Critical section. */
void os_cs_init(pthread_mutex_t *mutex);
void os_cs_enter(pthread_mutex_t *mutex);
void os_cs_leave(pthread_mutex_t *mutex);
void os_cs_destroy(pthread_mutex_t *mutex);

/* Semaphores. */
void os_sem_init(sem_t *sem, unsigned int init_value);
void os_sem_wait(sem_t *sem);
void os_sem_release(sem_t *sem);
void os_sem_delete(sem_t *sem);

/* Interrupts. */
void os_spin_init(spinlock_t *spinlock);
void os_spin_lock(spinlock_t *spinlock);
void os_spin_unlock(spinlock_t *spinlock);
void os_spin_destroy(spinlock_t *spinlock);

/* Threads. */
void *os_thread_create(const char *name, os_thread_prio_t prio, int queue_size);
char *os_thread_name(void *thread);
void os_thread_destroy(void *thread);

/* Message queue. */
void os_queue_send(void *g_thread, os_queue_elem_t *msg, int size);

/* Shared memory. */
int os_open(char *device_name);
void os_close(int dev_id);
void os_sync_write(int dev_id, char *buf, int count);
int os_sync_pread(int dev_id, char **buf, int count);
int os_sync_read(int dev_id, char *buf, int count);

#endif /* __os_h__ */
