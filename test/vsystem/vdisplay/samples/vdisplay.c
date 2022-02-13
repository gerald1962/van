// SPDX-License-Identifier: GPL-2.0

/*
 * tide - Tcl Demonstrator.
 *
 * Copyright (C) 2022 Gerald Schueller <gerald.schueller@web.de>
 */

/*============================================================================
  IMPORTED INCLUDE REFERENCES
  ============================================================================*/
#include "os.h"       /* Operating system: os_sem_create(). */

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
/**
 * tide_data - state of the Tcl demonstrator. 
 *
 * @my_trace:  if 1, activate the tide trace.
 **/
static struct tide_data_s {
	int    my_trace;
	Tcl_Interp  *interp;
	
} tide_data;

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
 * main() - start function of the Tcl demonstrator.
 *
 * Return:	0 or force a software trap.
 **/
int main(void)
{
	struct tide_data_s *t;
	const char *result;
	int rv, code;
		
	printf("tide - Tcl Demonstrator\n");
	
	/* Get the reference to the tide data. */
	t = &tide_data;

	/* Create a Tcl command interpreter. */
	t->interp = Tcl_CreateInterp();

	/* Create a van OS cabel with a display and any neighbouring lines. */
	rv = Van_Init(t->interp);
	OS_TRAP_IF(rv != TCL_OK);

	/* Read and evaluate the Tcl script. */
	code = Tcl_EvalFile(t->interp, "c.tcl");
	result = Tcl_GetStringResult(t->interp);

	/* Aanalyze the result of the scipt execution. */
	if (code != TCL_OK) {
		printf("Error was: %s\n", result);
		exit(1);
	}
	printf("Result was: %s\n", result);
	
	/* Delete a Tcl command interpreter. */
	Tcl_DeleteInterp(t->interp);
	
	return (0);
}
