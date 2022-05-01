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
 * man memset
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
 * man memcpy
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
 * man memccmp
 **/
int os_memcmp(const void *s1, const void *s2, size_t n)
{
	/* Entry condition. */
	OS_TRAP_IF(s1 == NULL || s2 == NULL || n < 1);
	return memcmp(s1, s2, n);
}

/**
 * os_memchr() - memchr with additional assertions.
 *
 * man memchr
 **/
void *os_memchr(const void *s, const void *end, int c, size_t n)
{
	/* Entry condition. */
	OS_TRAP_IF(s == NULL || end == NULL || end < s || n < 1 ||
		   ((char *) end - (char *) s) != n);
	return memchr(s, c, n);
}

/**
 * os_strnlen() - strnlen with additional assertions.
 *
 * man strnlen
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
 * man strlen
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
 * man strcpy
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
 * os_strncmp() - strncmp with additional assertions.
 *
 * man strncmp
 **/
int os_strncmp(const char *s1, const char *s2, int n)
{
	/* Entry condition. */
	OS_TRAP_IF(s1 == NULL || s2 == NULL || n > OS_MAX_STRING_LEN);
	return strncmp(s1, s2, n);
}

/**
 * os_strcmp() - strcmp with additional assertions.
 *
 * man strcmp
 **/
int os_strcmp(const char *s1, const char *s2)
{
	/* Entry condition. */
	OS_TRAP_IF(s1 == NULL || s2 == NULL);
	return strcmp(s1, s2);
}

/**
 * os_strncat() - strncat with additional assertions.
 *
 * man strncat
 **/
char *os_strncat(char *dest, const char *src, size_t n)
{
	/* Entry condition. */
	OS_TRAP_IF(dest == NULL || src == NULL || n > OS_MAX_STRING_LEN);

	return strncat(dest, src, n);
}

/**
 * os_strstr() - strstr with additional assertions.
 *
 * man strstr
 **/
char *os_strstr(const char *haystack, int h_len, const char *needle)
{
	/* Entry condition. */
	OS_TRAP_IF(haystack == NULL || h_len < 1 || h_len > OS_MAX_STRING_LEN ||
		   needle == NULL || haystack[h_len] != '\0');

	return strstr(haystack, needle);
}

/**
 * os_strchr() - strchr with additional assertions.
 *
 * man strchr
 **/
char *os_strchr(const char *s, int s_len, int c)
{
	/* Entry condition. */
	OS_TRAP_IF(s == NULL || s_len < 1 || s_len > OS_MAX_STRING_LEN ||
		   s[s_len] != '\0');

	return strchr(s, c);
}

/**
 * os_strtol_b10() - strtol with base 10 and additional assertions.
 *
 * man strtol
 **/
long int os_strtol_b10(const char *nptr, int n_len)
{
	char *endptr;
	int n;
	
	/* Entry condition. */
	OS_TRAP_IF(nptr == NULL || n_len < 1 || n_len > OS_MAX_STRING_LEN ||
		   nptr[n_len] != '\0');

	/* Convert the digit string to a long integer. */
	endptr = NULL;
	n = strtol(nptr, &endptr, 10);

	/* Test the ramaining garbage characters. */
	OS_TRAP_IF(*endptr != '\0');

	return n;
}
