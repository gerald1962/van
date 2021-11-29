// SPDX-License-Identifier: GPL-2.0

/*
 * Save memory and string functions.
 *
 * Copyright (C) 2021 Gerald Schueller <gerald.schueller@web.de>
 */

/*============================================================================
  IMPORTED INCLUDE REFERENCES
  ============================================================================*/
#include <string.h>  /* String operations. */
#include "os.h"      /* Operating system: os_sem_create() */

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
  LOCAL DATA
  ============================================================================*/
/*============================================================================
  LOCAL FUNCTION PROTOTYPES
  ============================================================================*/
/*============================================================================
  LOCAL FUNCTIONS
  ============================================================================*/
/*============================================================================
  EXPORTED FUNCTIONS
  ============================================================================*/

/**
 * os_memset() - memset with additional assertions.
 *
 * see memset
 **/
void *os_memset(void *s, int c, size_t n)
{
	/* Entry condition. */
	OS_TRAP_IF(s == NULL);
	return memset(s, c, n);
}

/**
 * os_memcpy() - memset with additional assertions.
 *
 * see memcpy
 **/
void *os_memcpy(void *dest, size_t dest_n, const void *src, size_t src_n)
{
	/* Entry condition. */
	OS_TRAP_IF(dest == NULL || src == NULL || src_n < 1 || src_n > dest_n);
	return memcpy(dest, src, src_n);
}

/**
 * os_memcmp() - memcmp with additional assertions.
 *
 * see memccmp
 **/
int os_memcmp(const void *s1, const void *s2, size_t n)
{
	/* Entry condition. */
	OS_TRAP_IF(s1 == NULL || s2 == NULL || n < 1);
	return memcmp(s1, s2, n);
}

/**
 * os_strnlen() - strnlen with additional assertions.
 *
 * see strnlen
 **/
size_t os_strnlen(const char *s, size_t maxlen)
{
	/* Entry condition. */
	OS_TRAP_IF(s == NULL || maxlen > OS_MAX_STRING_LEN);
	return strnlen(s, maxlen);
}

/**
 * os_strlen() - strlen with additional assertions.
 *
 * see strlen
 **/
size_t os_strlen(const char *s)
{
	size_t len;
	
	/* Entry condition. */
	OS_TRAP_IF(s == NULL);

	len = strlen(s);

	/* Final condition. */
	OS_TRAP_IF(len > OS_MAX_STRING_LEN);

	return len;
}

/**
 * os_strcpy() - strcpy with additional assertions.
 *
 * see strcpy
 **/
char *os_strcpy(char *dest, int dest_n, const char *src)
{
	size_t src_n;

	/* Entry condition. */
	OS_TRAP_IF(dest == NULL || dest_n < 1);

	/* Initialize the destination string. */
	*dest = '\0';

	/* Calculate the length of the source string. */
	src_n = os_strnlen(src, dest_n);
	OS_TRAP_IF(src_n >= dest_n);

	return strcpy(dest, src);
}

/**
 * os_strcmp() - strcmp with additional assertions.
 *
 * see strcmp
 **/

int os_strcmp(const char *s1, const char *s2)
{
	/* Entry condition. */
	OS_TRAP_IF(s1 == NULL || s2 == NULL);
	return strcmp(s1, s2);
}
