// SPDX-License-Identifier: GPL-2.0

/*
 * Allocate and free dynamic memory.
 *
 * Copyright (C) 2021 Gerald Schueller <gerald.schueller@web.de>
 */

/*============================================================================
  IMPORTED INCLUDE REFERENCES
  ============================================================================*/
#include <stdlib.h>      /* Memory operations: malloc(). */
#include "os.h"          /* Operating system: os_sem_create() */

/*============================================================================
  EXPORTED INCLUDE REFERENCES
  ============================================================================*/
#include "os_private.h"  /* Local interfaces of the OS: os_trap_init() */

/*============================================================================
  LOCAL NAME CONSTANTS DEFINITIONS
  ============================================================================*/
/*============================================================================
  MACROS
  ============================================================================*/
/*============================================================================
  LOCAL TYPE DEFINITIONS
  ============================================================================*/

/* State of a single malloc element. */

/**
 * os_mem_t - start addess of the memory allocated with malloc().
 *
 * @elem_idx:   position in the memory list.
 * @file_idx:   position in the file list.
 * @line:       line number of the os_malloc() call.
 * @allocated:  1, if the element is in use.
 * @start:      start address of the allocated memoy for the client.
 **/
typedef struct {
        int    elem_idx;
        int    file_idx;
        unsigned long line;
        int    allocated;
        void  *start;
} os_mem_elem_t;

/* State of the malloc table. */
/**
 * os_mem_stat_t - list for the os_malloc calls.
 *
 * @protect:   mutex for the critical section.
 * @file:      file list.
 * @next:      current start index of the element list.
 * @malloc_c:  number of the os_malloc calls.
 * @free_c:    number of the os_free calls.
 * @free_c:    number of the os_free calls.
 * @elem:      os_malloc list.
 **/
typedef struct {
        pthread_mutex_t   protect;
        char             *file[OS_MALLOC_FILE_LIMIT];
        int               next;
        unsigned long     malloc_c;
        unsigned long     free_c;
        os_mem_elem_t     elem[OS_MALLOC_LIMIT];
} os_mem_stat_t;

/**
 * os_mem_t - start addess of the memory allocated with malloc().
 *
 * @idx:    position in the memory list.
 * @align:  64 bit alignmen.
 * @start:  start address of the allocated memoy for the client.
 **/
typedef struct {
        int list_idx;
        union {
                unsigned long long align;
                void *start;
        } ptr;
} os_mem_t;

/*============================================================================
  LOCAL DATA
  ============================================================================*/

/* State of the os_malloc list. */
static os_mem_stat_t os_mem_stat;

/*============================================================================
  LOCAL FUNCTION PROTOTYPES
  ============================================================================*/
/*============================================================================
  LOCAL FUNCTIONS
  ============================================================================*/

/**
 * os_mem_elem_put() - free an os_malloc element.
 *
 * @idx:  index of the reserved list element.
 * @ptr:  address of the external os_malloc buffer.
 *
 * Return:     None.
 **/
static void os_mem_elem_put(int idx, void *ptr)
{
	os_mem_stat_t *p;
	os_mem_elem_t *elem;
	
	/* Get the reference of the os_malloc list. */
        p = &os_mem_stat;

        /* Enter the critical section. */
        os_cs_enter(&p->protect);

        /* Entry condition. */
        OS_TRAP_IF(idx < 0 || idx >= OS_MALLOC_LIMIT);


        /* Get the reference of the os_malloc element. */
        elem = &p->elem[idx];

        /* Test the element state. */
        OS_TRAP_IF((! elem->allocated) || elem->start != ptr);

        /* Reset the os_malloc element. */
        elem->allocated = 0;

        /* Increment the free counter. */
        p->free_c++;
	
	/* Leave the critical section. */
        os_cs_leave(&p->protect);

}

/**
 * os_mem_elem_alloc() - get a new os_malloc element.
 *
 * @p:     address of the os_malloc state.
 *
 * Return:     the os_malloc element.
 **/
static os_mem_elem_t *os_mem_elem_alloc(os_mem_stat_t *p)
{
        os_mem_elem_t *elem, *list;
        int  next, last;

        /* Initialize the local state. */
        list = p->elem;
        next = p->next;
        elem = &list[next];

        elem->allocated = 1;
        elem->start = NULL;

        last  = next;

        /* Search for the next free element. */
        for (;;) {

                /* Increment the search index. */
                next++;
                if (next >= OS_MALLOC_LIMIT)
                        next = 0;

                /* Test the list state. */
                OS_TRAP_IF(next == last);

                /* Test the state of the list element. */
                if (! list[next].allocated) {
                        /* Save the index of the next free element. */
                        p->next = next;
                        break;
                }
        }

        return elem;

}

/**
 * os_mem_file_idx() - map the file name to index.
 *
 * @p:     address of the os_malloc state.
 * @file:  matches __FILE__.
 *
 * Return:	the file index.
 **/
static int os_mem_file_idx(os_mem_stat_t *p, char *file)
{
        int      i, free;
        char   **list;
	size_t   len;

        /* Initialize the file name index. */
        free = -1;

        /* Get the reference to the file name list. */
        list = p->file;

        /* Loop thru the file name list. */
        for (i = 0; i < OS_MALLOC_FILE_LIMIT; i++) {
                /* Test the file name element. */
                if (list[i] != NULL) {
                        /* Test the file name. */
                        if (os_strcmp(list[i], file) == 0)
                                return i; /* Found. */
			
                        /* Continue with the search. */
                        continue;
                }

                /* Test the free index. */
                if (free < 0)
                        free = i;
        }

        /* Test the free index. */
        OS_TRAP_IF(free < 0);

        /* Create a new file name element. */
        len = os_strlen(file) + 1;
        list[free] = malloc (len);
        OS_TRAP_IF (list[free] == NULL);

        /* Save the file name. */
        os_strcpy(list[free], len, file);

        return free;
}

/**
 * os_mem_elem_get() - get a os_malloc element.
 *
 * @file:  matches __FILE__.
 * @line:  matches __LINE__.
 *
 * Return:	the pointer to the os_malloc element.
 **/
static os_mem_elem_t *os_mem_elem_get(char *file, unsigned long line)
{
        os_mem_stat_t *p;
        os_mem_elem_t *elem;
        int idx;

        /* Get the reference of the os_malloc list. */
        p = &os_mem_stat;

        /* Enter the critical section. */
        os_cs_enter(&p->protect);

        /* Map the file name to index. */
        idx = os_mem_file_idx(p, file);

        /* Get os_malloc element. */
        elem = os_mem_elem_alloc (p);

        /* Increment the os_malloc counter. */
        p->malloc_c++;

        /* Save the file index and the line number. */
        elem->file_idx = idx;
        elem->line     = line;

        /* Leave the critical section. */
        os_cs_leave(&p->protect);

        return elem;
}

/*============================================================================
  EXPORTED FUNCTIONS
  ============================================================================*/

/**
 * os_malloc() - allocates size bytes and returns a pointer to the allocated
 * memory, see malloc().
 *
 * @size:  size of the requested memory.
 * @file:  matches __FILE__.
 * @line:  matches __LINE__.
 *
 * Return:	the pointer to the allocated memory.
 **/
void *os_malloc(size_t size, char *file, unsigned long line)
{
        os_mem_elem_t  *elem;
        os_mem_t  *mem;
	
	/* Entry condition. */
	OS_TRAP_IF(size < 1);

        /* Request memory from the OS. */
        mem = malloc(size + sizeof(os_mem_t));
        OS_TRAP_IF(mem == NULL);

        /* Save the start address. */
        mem->ptr.start = (char *) mem + sizeof(os_mem_t);

        /* Create a os_malloc table element. */
        elem = os_mem_elem_get(file, line);

        /* Save the start start address of the allocated memory. */
        elem->start = mem->ptr.start;

        /* Save the index of the malloc element. */
        mem->list_idx = elem->elem_idx;

        return mem->ptr.start;
}

/**
 * os_free() - frees the memory space pointed to by ptr, which must have been
 * returned by a previous call to os_malloc().
 *
 * @ptr:  start address of the allocated memory.
 *
 * Return:	None.
 **/
void os_free(void **ptr)
{
	os_mem_t  *mem;
	
	/* Entry condition. */
	OS_TRAP_IF (ptr == NULL || *ptr == NULL);

	/* Calculate the start of the allocated memory. */
        mem = (os_mem_t *) ((char *) *ptr - sizeof (os_mem_t));

        /* Free the list element. */
        os_mem_elem_put(mem->list_idx, mem->ptr.start);

        /* Free the allocated buffer. */
        free(mem);

	/* Reset the pointer. */
	*ptr = NULL;
}

/**
 * os_mem_init() - initialize the os_malloc list.
 *
 * Return:	None.
 **/
void os_mem_init(void)
{
        os_mem_elem_t *elem;
        int i;

        /* Initialize the mutex for critical section. */
        os_cs_init(&os_mem_stat.protect);

        /* Get the reference to the first element. */
        elem = os_mem_stat.elem;

        /* Run thru the os_malloc list. */
        for (i = 0; i < OS_MALLOC_LIMIT; i++, elem++) {
                /* Save the element index. */
                elem->elem_idx = i;
        }
}

/**
 * os_mem_exit() - release the malloc list resources.
 *
 * Return:	None.
 **/
void os_mem_exit(void)
{
	os_mem_stat_t *p;
	char **list;
	int i;

	p = &os_mem_stat;

	/* Enter the critical section. */
        os_cs_enter(&p->protect);

	/* Test the state of the malloc list. */
	OS_TRAP_IF(p->malloc_c != p->free_c);

	/* Get the reference to the file name list. */
        list = p->file;

	/* Loop thru the file name list. */
        for (i = 0; i < OS_MALLOC_FILE_LIMIT; i++) {
		/* Release the file names. */
		if (list[i] != NULL)
			free(list[i]);
	}
	
        /* Leave the critical section. */
        os_cs_leave(&p->protect);

	os_cs_destroy(&p->protect);
}
