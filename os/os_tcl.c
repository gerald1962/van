// SPDX-License-Identifier: GPL-2.0

/*
 * Van Tcl C extension.
 *
 * Copyright (C) 2022 Gerald Schueller <gerald.schueller@web.de>
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
/** 
 * tcl_cable - state of the cable I/O wires.
 *
 * @chn_type:    addresses of procedures that can be called to perform I/O and other
 *               functions on the channel.
 * @tcl_chn:     backlink from the instanceData to the channel. 
 * @cid:         cable end point id.
 * @inp_toggle:  if 1, read the current input message, otherwise ignore it. This
 *               toggle prevents Tcl, to buffer the input messages, until it has
 *               recognized end of file. 
 **/
static struct tcl_cable_s {
	Tcl_ChannelType  chn_type;
	Tcl_Channel      tcl_chn;
	int  cid;
	int  inp_toggle;
} tcl_cable;

/*============================================================================
  LOCAL FUNCTION PROTOTYPES
  ============================================================================*/
/*============================================================================
  LOCAL FUNCTIONS
  ============================================================================*/
/**
 * tcl_watchProc() - called by the generic layer to initialize the event
 * notification mechanism to notice events of interest on this channel.
 *
 * @instanceData:  is the same as the value provided to Tcl_CreateChannel() when
 *                 the channel was created. 
 * @mask:          is an OR-ed combination of TCL_READABLE, TCL_WRITABLE and
 *                 TCL_EXCEPTION; it indicates events the caller is interested
 *                 in noticing on this channel.
 *
 * Return:	None.
 **/
static void tcl_watchProc(ClientData instanceData, int mask)
{
}

/**
 * tcl_getOptionProc() - called by the generic layer to get the value of a
 * channel type specific option on a channel. 
 *
 * @instanceData:  is the same as the value provided to Tcl_CreateChannel() when
 *                 the channel was created. 
 * @interp:        If an error occurs and interp is not NULL, the procedure
 *                 should store an error message in the interpreter's result.
 * @optionName:    is the name of an option supported by this type of channel.
 * @newValue:      If the option name is not NULL, the function stores its
 *                 current value, as a string, in the Tcl dynamic string
 *                 optionValue. If optionName is NULL, the function stores in
 *                 optionValue an alternating list of all supported options and
 *                 their current values.
 *
 * Return:	None.
 **/
static int tcl_getOptionProc(ClientData instanceData, Tcl_Interp *interp,
			     CONST char *optionName, Tcl_DString *optionValue)
{
	struct tcl_cable_s *t;
	char  buf[8];
	int rv, len;
	
	/* Entry condition. */
	OS_TRAP_IF(instanceData == NULL || interp == NULL ||
		   optionName == NULL || optionValue == NULL);
	
	/* Test the option name. */
	if (os_strcmp(optionName, "-writable") != 0)
		return TCL_ERROR;

	/* Decode the instance data. */
	t = instanceData;
	
	/* Get the next free message buffer. */
	rv = os_bwritable(t->cid);

	/* Convert the buffer size. */
	len = snprintf(buf, 8, "%d", rv);
	
	/* Save the return value. */
	Tcl_DStringAppend(optionValue, buf, len);
	
	return  TCL_OK;
}

/**
 * tcl_setOptionProc() - called by the generic layer to set a channel type
 * specific option on a channel.
 *
 * @instanceData:  is the same as the value provided to Tcl_CreateChannel() when
 *                 the channel was created. 
 * @interp:        If an error occurs and interp is not NULL, the procedure
 *                 should store an error message in the interpreter's result.
 * @optionName:    is the name of an option to set.
 * @newValue:      is the new value for that option, as a string.
 *
 * Return:	If the option value is successfully modified to the new value,
 * the function returns TCL_OK. It should call Tcl_BadChannelOption which itself
 * returns TCL_ERROR if the optionName is unrecognized. If newValue specifies a
 * value for the option that is not supported or if a system call error occurs,
 * the function should leave an error message in the result field of interp if
 * interp is not NULL. The function should also call Tcl_SetErrno() to store an
 * appropriate POSIX error code.
 **/
static int tcl_setOptionProc(ClientData instanceData, Tcl_Interp *interp,
			     CONST char *optionName, CONST char *newValue)
{
	printf("%s: ...\n", F);
	return  TCL_OK;
}

/**
 * tcl_outputProc() - called by the generic layer to transfer data from an
 * internal buffer to any cable end point.
 *
 * @instanceData:  is the same as the value provided to Tcl_CreateChannel() when
 *                 the channel was created. 
 * @buf:           contains an array of bytes to be written to the cable end
 *                 point.
 * @toWrite:       indicates how many bytes are to be written from the buf
 *                 argument.
 * @errorCodePtr:  points to an integer variable provided by the generic layer.
 *                 If an error occurs, the function should set this variable to
 *                 a POSIX error code that identifies the error.
 *
 * Return:	On success, the function should return a nonnegative integer
 * indicating how many bytes were written to the output wire of the end point.
 * The return value is normally the same as toWrite. If an error
 * occurs the function should return -1. In case of error, no data have
 * been written to the device.
 **/
static int tcl_outputProc(ClientData instanceData, CONST char *buf, int toWrite,
			  int *errorCodePtr)
{
	struct tcl_cable_s *t;
	int n;
	
	/* Entry condition. */
	OS_TRAP_IF(instanceData == NULL || buf == NULL || toWrite < 1 ||
		   toWrite > OS_BUF_SIZE || errorCodePtr == NULL);
	
	/* Decode the instance data. */
	t = instanceData;
	
	/* Send the message to the controller. */
	n = os_bwrite(t->cid, (char *) buf, toWrite);
	OS_TRAP_IF(n < 0 && n != toWrite);

	return n;
}

/**
 * tcl_inputProc() - called by the generic layer to read data from the cable and
 * store it in an internal buffer.
 *
 * @instanceData:  is the same as the value provided to Tcl_CreateChannel() when
 *                 the channel was created. 
 * @buf:           points to an array of bytes in which to store input from the
 *                 cable end point.
 * @bufSize:       indicates how many bytes are available at buf.
 * @errorCodePtr:  points to an integer variable provided by the generic layer.
 *                 If an error occurs, the function should set the variable to a
 *                 POSIX error code that identifies the error that occurred.
 *
 * Return:	On success, the function should return a nonnegative integer
 * indicating how many bytes were read from the input wire of the cable and
 * stored at buf. On error, the function should return -1. If an error occurs
 * after some data has been read from the device, that data is lost.
 **/
static int tcl_inputProc(ClientData instanceData, char *buf, int bufSize,
			 int *errorCodePtr)
{
	struct tcl_cable_s *t;
	int n;
	
	/* Entry condition. */
	OS_TRAP_IF(instanceData == NULL || buf == NULL || bufSize < 1 ||
		   bufSize < OS_BUF_SIZE || errorCodePtr == NULL);

	/* Decode the instance data. */
	t = instanceData;
	
	/* Change the toggle value. */
	t->inp_toggle = t->inp_toggle ? 0 : 1;
	if (t->inp_toggle)
		return 0;
	
	/* Read a controller message if present. */
	n = os_bread(t->cid, buf, bufSize);
	
	return n;
}

/**
 * tcl_closeProc() - called by the generic layer to clean up driver-related
 * information when the channel is closed.
 *
 * @instanceData:  is the same as the value provided to Tcl_CreateChannel() when
 *                 the channel was created. 
 * @interp:        If an error occurs and interp is not NULL, the procedure
 *                 should store an error message in the interpreter's result.
 *
 * Return:	If the close operation is successful, the procedure should
 * return zero; otherwise it should return a nonzero POSIX error code.
 **/
static int tcl_closeProc(ClientData instanceData, Tcl_Interp *interp)
{
	/* Entry condition. */
	OS_TRAP_IF(instanceData == NULL || interp == NULL);

	printf("%s: ...\n", F);
	return 0;
}

/**
 * tcl_plug_insert() - create the cable channel.
 *
 * @interp:  Tcl interpreter handle.

 * Return:	None.
 **/
static void tcl_plug_insert(Tcl_Interp *interp)
{
	struct tcl_cable_s *t;
	Tcl_ChannelType *c;
	int mode;

	/* Entry condition. */
	OS_TRAP_IF(interp == NULL);

	/* Get the pointer to the cable channel state. */
	t = &tcl_cable;

	/* Get the pointer to the channel type. */
	c = &t->chn_type;
	
	/* Initialize the cable channel type. */
	c->typeName         = "van_cable";
	c->version          = TCL_CHANNEL_VERSION_2;
	c->closeProc        = tcl_closeProc;
	c->inputProc        = tcl_inputProc;
	c->outputProc       = tcl_outputProc;
	c->seekProc         = NULL;
	c->setOptionProc    = tcl_setOptionProc;
	c->getOptionProc    = tcl_getOptionProc;
	c->watchProc        = tcl_watchProc;
	c->getHandleProc    = NULL;
	c->close2Proc       = NULL;
	c->blockModeProc    = NULL;
	c->flushProc        = NULL;
	c->handlerProc      = NULL;
	c->wideSeekProc     = NULL;
	c->threadActionProc = NULL;
	c->truncateProc     = NULL;

	/* The cable channel is readable and writable. */
	mode = TCL_READABLE | TCL_WRITABLE;

	/* Open a new channel and associates the supplied typePtr and instanceData with it. */
	t->tcl_chn = Tcl_CreateChannel(c, "c_disp_ctrl", t, mode);
	
	/* Add a channel to the set of channels accessible in interp. After this
	 * call, Tcl programs executing in that interpreter can refer to the
	 * channel in input or output operations using the name given in the call
	 * to Tcl_CreateChannel. */
	Tcl_RegisterChannel(interp, t->tcl_chn);

	/* XXX Ensure the one time call and prepare IPC about shared memory.
	 * Make sure, that the cable controller is running. */
	os_init(0);

	/* Deactivate the OS trace. */
	os_trace_button(0);

	/* Insert the display plug. */
	t->cid = os_bopen("/display");

	/* Initialize the input toggle. */
	t->inp_toggle = 1;
}
	
/**
 * tcl_cable_cmd() - invoked as a Tcl command, to establish a cable link to
 * the cable controller.
 *
 * @cdata:   points to a data structure that describes what to do.
 * @interp:  Tcl interpreter handle.
 * @objc:    giving the number of argument values.
 * @objv:    array with pointers to the argument values.
 *
 * Return:	TCL_OK.
 **/
static int tcl_cable_cmd(ClientData cdata, Tcl_Interp *interp, int objc,
			Tcl_Obj *const objv[])
{
	if (objc != 2) {
		Tcl_WrongNumArgs(interp, 1, objv, "");
		return TCL_ERROR;
	}

	/* Insert the display or battery plug. */
	tcl_plug_insert(interp);
	
	/* Save the cable end point to execute the generic I/O operations. */
	Tcl_AppendResult(interp, "c_disp_ctrl", (char *) NULL);
	
	return TCL_OK;
}

/*============================================================================
  EXPORTED FUNCTIONS
  ============================================================================*/

/**
 * Van_Init() - create a van OS cable with a display end point. This function is
 * called when Tcl loads the van OS extensions.
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

	/* cable shall be invoked as a Tcl command, to establish a cable link to
	 * the cable controller. */
	Tcl_CreateObjCommand(interp, "cable", tcl_cable_cmd, NULL, NULL);

	return TCL_OK;
}
