// SPDX-License-Identifier: GPL-2.0

/*
 * Van Tcl C extension.
 *
 * Copyright (C) 2021 Gerald Schueller <gerald.schueller@web.de>
 */

/*============================================================================
  IMPORTED INCLUDE REFERENCES
  ============================================================================*/
#include <tcl/tcl.h>  /* Facilities of the Tcl interpreter. */
#include "os.h"       /* Van OS: os_init(). */

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
/**
 * van_exit_cmd() - shall be invoked as a Tcl command, to ensure the one time
 * call and test pending resources.
 *
 * @cdata:   points to a data structure that describes what to do.
 * @interp:  Tcl interpreter handle.
 * @objc:    giving the number of argument values.
 * @objv:    array with pointers to the argument values.
 *
 * Return:	TCL_OK.
 **/
static int van_exit_cmd(ClientData cdata, Tcl_Interp *interp, int objc,
			Tcl_Obj *const objv[])
{
	if (objc != 1) {
		Tcl_WrongNumArgs(interp, 1, objv, "");
		return TCL_ERROR;
	}
	
	/* Ensure the one time call and and shutdown the van OS. */
	os_exit();
	
	return TCL_OK;
}

/**
 * van_init_cmd() - invoked as a Tcl command, to initialize the van OS.
 *
 * @cdata:   points to a data structure that describes what to do.
 * @interp:  Tcl interpreter handle.
 * @objc:    giving the number of argument values.
 * @objv:    array with pointers to the argument values.
 *
 * Return:	TCL_OK.
 **/
static int van_init_cmd(ClientData cdata, Tcl_Interp *interp, int objc,
			Tcl_Obj *const objv[])
{
	if (objc != 1) {
		Tcl_WrongNumArgs(interp, 1, objv, "");
		return TCL_ERROR;
	}
	
	/* Ensure the one time call and prepare IPC about shared memory. */
	os_init(0);
	
	return TCL_OK;
}

/*============================================================================
  EXPORTED FUNCTIONS
  ============================================================================*/

/**
 * Van_Init() - create the Tcl/Tk van OS interfaces. This function is called
 * when Tcl loads the van OS extensions.
 *
 * @interp:  Tcl interpreter handle.
 *
 * Return:	TCL_OK or TCL_ERROR.
 **/
int DLLEXPORT Van_Init(Tcl_Interp *interp)
{
	/* Bind the van OS extensions to the interpreter at run time. 0 means
	 * that versions newer than TCL_VERSION are also acceptable as long as
	 * they have the same major version number as version. */
	if (Tcl_InitStubs(interp, TCL_VERSION, 0) == NULL) {
		return TCL_ERROR;
	}
	
	/* Version management for the van OS about package name and version
	 * string. */
	if (Tcl_PkgProvide(interp, "Van", "1.0") == TCL_ERROR) {
		return TCL_ERROR;
	}

	/* van_init shall be invoked as a Tcl command, to initialize the van
	 * OS. */
	Tcl_CreateObjCommand(interp, "van_init", van_init_cmd, NULL, NULL);

	/* van_exit shall be invoked as a Tcl command, to free the allocated
	 * resources of the van OS. */
	Tcl_CreateObjCommand(interp, "van_exit", van_exit_cmd, NULL, NULL);

	return TCL_OK;
}
