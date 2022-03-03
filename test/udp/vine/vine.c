// SPDX-License-Identifier: GPL-2.0

/*
 * Test of the internet interfaces for the van system.
 *
 * Copyright (C) 2022 Gerald Schueller <gerald.schueller@web.de>
 */

/*============================================================================
  IMPORTED INCLUDE REFERENCES
  ============================================================================*/
#include "os.h"      /* Operating system: os_inet_sopen(). */

/*============================================================================
  EXPORTED INCLUDE REFERENCES
  ============================================================================*/
/*============================================================================
  LOCAL NAME CONSTANTS DEFINITIONS
  ============================================================================*/
#define P            "I>"         /* Internet test prompt. */
#define CLIENT_ADDR  "127.0.0.1"  /* IP address of the van display. */
#define CLIENT_PORT  58062        /* Port number of the van display. */
#define SERV_ADDR    "127.0.0.1"  /* IP address of the van controller. */
#define SERV_PORT    62058        /* Port number of the van controller. */

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
 * main() - start function of the van internet test.
 *
 * Return:	0 or force a software trap.
 **/
int main(void)
{
	int serv_id;
	
	printf("%s van internet test\n", P);

	/* XXX Start the van controller peer. */
	serv_id = os_inet_open(SERV_ADDR, SERV_PORT, CLIENT_ADDR, CLIENT_PORT);

	/* Shutdown the van controller server. */
	os_inet_close(serv_id);
	
	return(0);
}
