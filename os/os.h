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
#define OS_MAX_NAME_LEN  8

/* Number of the supported threads. */
#define OS_THREAD_LIMIT  2

/* Limit of the thread input queue. */
#define OS_QUEUE_LIMIT  1024

/* Limit of the os_malloc calls without correspondig ms_free calls. */
#define OS_MALLOC_LIMIT     256

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

/* Wrapper for os_mallo(). */
#define OS_MALLOC(size_)  os_malloc(size_, __FILE__, __LINE__)

/* XXX FEATURE */
#define OS_DEBUG
#if defined (OS_DEBUG)
#define OS_TRACE(info_)  do { printf info_ ; } while (0)
#else
#define OS_TRACE(info_)
#endif

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

/*============================================================================
  GLOBAL DATA
  ============================================================================*/
/*============================================================================
  EXPORTED FUNCTIONS
  ============================================================================*/
/* Bootstrapping. */
void os_init(void);

/* Traps handling. */
void os_trap(char *file, const char *function, unsigned long line);

/* Dynamic memory. */
void os_free(void *ptr);
void *os_malloc(size_t size, char *file, unsigned long line);

/* Memory and string. */
void *os_memset(void *s, int c, size_t n);
void *os_memcpy(void *dest, size_t dest_n, const void *src, size_t src_n);
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
void os_thread_start(void *thread);

/* Message queue. */
void os_queue_send(void *g_thread, os_queue_elem_t *msg, int size);

#endif /* __os_h__ */
