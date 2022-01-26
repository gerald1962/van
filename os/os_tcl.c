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
#define CABLE_COUNT 2  /* Number of the system cables. */

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
 * cable - state of the cable I/O wires.
 *
 * @van_ep_name:  name of the van cable end point, see os_c_open() or os_bopen().
 * @tcl_ep_id:    tcl cable end point id, to invoke the generic tcl/tk commands
 *                like gets, puts and close.
 * @tcl_chn_id:   tcl channel id, to create a new channel with 
 *                Tcl_CreateChannel(): either cable wires with message queue
 *                - buffered - or triggered by I/O interrupts directly.
 * @open:         open the neighbour entry point of the van controller.
 * @close:        the neighbour entry point of the van controller.
 * @read:         from a cable end point.
 * @write:        to a cable end point.
 * @writable:     get the size of the free output buffer.
 * @sync:         get the number of the pending output bytes.
 * @van_ep_id:    van OS cable end point id.
 * @locked:       if 1, the end point is in use.
 * @inp_toggle:   if 1, read the current input message, otherwise ignore it. This
 *                toggle prevents Tcl, to buffer the input messages, until it has
 *                recognized end of file. 
 * @chn_type:     addresses of procedures that can be called to perform I/O and other
 *                functions on the channel.
 * @tcl_chn:      backlink from the instanceData to the channel. 
 **/
static struct cable_s {
	char  *van_ep_name;
	char  *tcl_ep_id;
	char  *tcl_chn_id;
	int   (*open)     (const char *ep_name, int mode);
	void  (*close)    (int ep_id);
	int   (*read)     (int ep_id, char *buf, int count);
	int   (*write)    (int ep_id, char *buf, int count);
	int   (*writable) (int ep_id);
	int   (*sync) (int ep_id);
	int   van_ep_id;
	int   locked;
	int   inp_toggle;
	Tcl_ChannelType  chn_type;
	Tcl_Channel      tcl_chn;

} cable[CABLE_COUNT] = {{
		"/van/display",
		"van_disp_ctrl",
		"van_buf_cable",
		os_bopen,
		os_bclose,
		os_bread,
		os_bwrite,
		os_bwritable,
		os_bsync,
		0
	}, {
		"/van//battery",
		"van_batt_ctrl",
		"van_irq_cable",
		os_c_open,
		os_c_close,
		os_c_read,
		os_c_write,
		os_c_writable,
		os_c_sync,
		0 }};

/**
 * cable_system - global state of all controller neighbours.
 *
 * @os_busy:  if 1, os_init() has already been executed.
 **/
static struct cable_system_s {
	int  os_busy;
} cable_system;

/*============================================================================
  LOCAL FUNCTION PROTOTYPES
  ============================================================================*/
/*============================================================================
  LOCAL FUNCTIONS
  ============================================================================*/
/**
 * cab_watchProc() - called by the generic layer to initialize the event
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
static void cab_watchProc(ClientData instanceData, int mask)
{
}

/**
 * cab_getOptionProc() - called by the generic layer to get the value of a
 * channel type specific option on a channel. 
 *
 * @instanceData:  is the same as the value provided to Tcl_CreateChannel() when
 *                 the channel was created. 
 * @interp:        If an error occurs and interp is not NULL, the procedure
 *                 should store an error message in the interpreter's result.
 * @optionName:    is the name of an option supported by this type of channel.
 * @optionValue:   If the option name is not NULL, the function stores its
 *                 current value, as a string, in the Tcl dynamic string
 *                 optionValue. If optionName is NULL, the function stores in
 *                 optionValue an alternating list of all supported options and
 *                 their current values.
 *
 * Return:	None.
 **/
static int cab_getOptionProc(ClientData instanceData, Tcl_Interp *interp,
			     CONST char *optionName, Tcl_DString *optionValue)
{
	struct cable_s *c;
	char  buf[8];
	int rv, len;
	
	/* Entry condition. */
	OS_TRAP_IF(instanceData == NULL || interp == NULL ||
		   optionValue == NULL);

	/* Test the option name. */
	if (optionName == NULL)
		return TCL_ERROR;

	/* Decode the instance data. */
	c = instanceData;

	/* Test the option name. */
	if (os_strcmp(optionName, "-writable") == 0) {
		/* Get the next free message buffer. */
		rv = c->writable(c->van_ep_id);

		/* Convert the buffer size. */
		len = snprintf(buf, 8, "%d", rv);
	
		/* Save the return value. */
		Tcl_DStringAppend(optionValue, buf, len);
		return  TCL_OK;
	}
	
	/* Test the option name. */
	if (os_strcmp(optionName, "-sync") == 0) {
		/* Get the number of the pending output bytes. */
		rv = c->sync(c->van_ep_id);

		/* Convert the buffer size. */
		len = snprintf(buf, 8, "%d", rv);
	
		/* Save the return value. */
		Tcl_DStringAppend(optionValue, buf, len);
		return  TCL_OK;
	}

	return TCL_ERROR;
}

/**
 * cab_setOptionProc() - called by the generic layer to set a channel type
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
static int cab_setOptionProc(ClientData instanceData, Tcl_Interp *interp,
			     CONST char *optionName, CONST char *newValue)
{
	return  TCL_ERROR;
}

/**
 * cab_outputProc() - called by the generic layer to transfer data from an
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
static int cab_outputProc(ClientData instanceData, CONST char *buf, int toWrite,
			   int *errorCodePtr)
{
	struct cable_s *c;
	int n;
	
	/* Entry condition. */
	OS_TRAP_IF(instanceData == NULL || buf == NULL || toWrite < 1 ||
		   toWrite > OS_BUF_SIZE || errorCodePtr == NULL);
	
	/* Decode the instance data. */
	c = instanceData;
	
	/* Send the message to the controller. */
	n = c->write(c->van_ep_id, (char *) buf, toWrite);
	OS_TRAP_IF(n < 0 && n != toWrite);

	return n;
}

/**
 * cab_inputProc() - called by the generic layer to read data from the cable and
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
static int cab_inputProc(ClientData instanceData, char *buf, int bufSize,
			  int *errorCodePtr)
{
	struct cable_s *c;
	int n;
	
	/* Entry condition. */
	OS_TRAP_IF(instanceData == NULL || buf == NULL || bufSize < 1 ||
		   bufSize < OS_BUF_SIZE || errorCodePtr == NULL);

	/* Decode the instance data. */
	c = instanceData;
	
	/* Change the toggle value. */
	c->inp_toggle = c->inp_toggle ? 0 : 1;
	if (c->inp_toggle)
		return 0;
	
	/* Read a controller message if present. */
	n = c->read(c->van_ep_id, buf, bufSize);
	
	return n;
}

/**
 * cab_closeProc() - called by the generic layer to clean up driver-related
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
static int cab_closeProc(ClientData instanceData, Tcl_Interp *interp)
{
	struct cable_s *c, *cv;
	int i;
	
	/* Entry condition. */
	OS_TRAP_IF(instanceData == NULL || interp == NULL);

	/* Decode the instance data. */
	c = instanceData;

	/* Release the entry point. */
	c->locked = 0;
	
	/* Delete the display endpoint device. */
	c->close(c->van_ep_id);

	/* Test the busy state of all entry points. */
	for (i = 0, cv = cable; i < CABLE_COUNT; i++, cv++) {
		/* Test the end point state. */
		if (cv->locked)
			break;
			
	}

	/* Test the OS termination condition. */
	if (i < CABLE_COUNT)
		return 0;
	
	/* Release the OS resources. */
	cable_system.os_busy = 0;
	os_exit();

	return 0;
}

/**
 * cab_plug_insert() - create the cable channel.
 *
 * @interp:  Tcl interpreter handle.
 * @c:       pointer to the cable state.
 *
 * Return:	None.
 **/
static void cab_plug_insert(Tcl_Interp *interp, struct cable_s *c)
{
	Tcl_ChannelType *t;
	int mode;

	/* Entry condition. */
	OS_TRAP_IF(interp == NULL);

	/* Get the pointer to the channel type. */
	t = &c->chn_type;
	
	/* Initialize the cable channel type. */
	t->typeName         = c->tcl_chn_id;
	t->version          = TCL_CHANNEL_VERSION_2;
	t->closeProc        = cab_closeProc;
	t->inputProc        = cab_inputProc;
	t->outputProc       = cab_outputProc;
	t->seekProc         = NULL;
	t->setOptionProc    = cab_setOptionProc;
	t->getOptionProc    = cab_getOptionProc;
	t->watchProc        = cab_watchProc;
	t->getHandleProc    = NULL;
	t->close2Proc       = NULL;
	t->blockModeProc    = NULL;
	t->flushProc        = NULL;
	t->handlerProc      = NULL;
	t->wideSeekProc     = NULL;
	t->threadActionProc = NULL;
	t->truncateProc     = NULL;

	/* The cable channel is readable and writable. */
	mode = TCL_READABLE | TCL_WRITABLE;

	/* Open a new channel and associates the supplied typePtr and instanceData with it. */
	c->tcl_chn = Tcl_CreateChannel(t, c->tcl_ep_id, c, mode);
	
	/* Add a channel to the set of channels accessible in interp. After this
	 * call, Tcl programs executing in that interpreter can refer to the
	 * channel in input or output operations using the name given in the call
	 * to Tcl_CreateChannel. */
	Tcl_RegisterChannel(interp, c->tcl_chn);

	/* Ensure the one time call and prepare IPC about shared memory.
	 * Make sure, that the cable controller is running. */
	if (! cable_system.os_busy) {
		cable_system.os_busy = 1;
		os_init(0);
	}
	
	/* Deactivate the OS trace. */
	os_trace_button(0);

	/* Insert the display plug. */
	c->van_ep_id = c->open(c->van_ep_name, 0);

	/* Initialize the input toggle. */
	c->inp_toggle = 1;
}
	
/**
 * cable_cmd() - invoked as a Tcl command, to establish a buffer link to
 * the cable controller.
 *
 * @cdata:   points to a data structure that describes what to do.
 * @interp:  Tcl interpreter handle.
 * @objc:    giving the number of argument values.
 * @objv:    array with pointers to the argument values.
 *
 * Return:	TCL_OK.
 **/
static int cable_cmd(ClientData cdata, Tcl_Interp *interp, int objc,
			Tcl_Obj *const objv[])
{
	struct cable_s *c;
	char *n;
	int i;

	if (objc != 2) {
		Tcl_WrongNumArgs(interp, 1, objv, "");
		return TCL_ERROR;
	}

	/* Entry condition. */
	OS_TRAP_IF(interp == NULL || objv == NULL);

	/* Get the pointer to the end point name. */
	n = objv[1]->bytes;

	/* Search for the end point name. */
	for (i = 0, c = cable; i < CABLE_COUNT; i++, c++) {
		/* Test the end point state and name. */
		if (! c->locked && os_strcmp(c->van_ep_name, n) == 0)
			break;
			
	}

	/* Test the search result. */
	if (i >= CABLE_COUNT) {
		Tcl_WrongNumArgs(interp, 1, objv, "");
		return TCL_ERROR;
	}

	/* The entry point is available. */
	c->locked = 1;
	
	/* Insert the neighbour plug of the controller. */
	cab_plug_insert(interp, c);
		
	/* Save the cable end point, to execute the generic tcl/tk I/O operations. */
	Tcl_AppendResult(interp, c->tcl_ep_id, (char *) NULL);

	return TCL_OK;
}

/*============================================================================
  EXPORTED FUNCTIONS
  ============================================================================*/

/**
 * Van_Init() - create a van OS cable with a display or battery end point. 
 * This function is called when Tcl loads the van OS extensions.
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

	/* bCable shall be invoked as a Tcl command, to establish a buffer or
	 * irq cable link to the cable controller. */
	Tcl_CreateObjCommand(interp, "cable", cable_cmd, NULL, NULL);

	return TCL_OK;
}

/**
 * os_tcl_exit() - test the state, what you have used in any tcl/tk scripts.
 *
 * Return:	None.
 **/
void os_tcl_exit(void)
{
	struct cable_s *c;
	int i;
	
	/* Test the state of all tcl/tk channels. */
	for (i = 0, c = cable; i < CABLE_COUNT; i++, c++)
		OS_TRAP_IF(c->locked);
}

/**
 * os_tcl_init() - protect the tcl entry points.
 *
 * Return:	None.
 **/
void os_tcl_init(void)
{
}
