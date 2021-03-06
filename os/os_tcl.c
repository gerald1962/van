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
#define CABLE_COUNT 3  /* Number of the cable types. */

/*============================================================================
  MACROS
  ============================================================================*/
/*============================================================================
  LOCAL TYPE DEFINITIONS
  ============================================================================*/
/*============================================================================
  LOCAL FUNCTION PROTOTYPES
  ============================================================================*/
static int cab_inet_open(const char *name, int mode);

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
 * @connect:      get the connection status of the peers.
 * @van_ep_id:    van OS cable end point id.
 * @locked:       if 1, the end point is in use.
 * @os_conf:      if 1, vdisplay shall be started on a remote network peer.
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
	int   (*open)      (const char *ep_name, int mode);
	void  (*close)     (int ep_id);
	int   (*read)      (int ep_id, char *buf, int count);
	int   (*write)     (int ep_id, char *buf, int count);
	int   (*writable)  (int ep_id);
	int   (*sync)      (int ep_id);
	int   (*connect)   (int ep_id);
	int   van_ep_id;
	int   locked;
	int   os_conf;
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
		os_bconnect,
		0
	}, {
		"/van/battery",
		"van_batt_ctrl",
		"van_irq_cable",
		os_c_open,
		os_c_close,
		os_c_read,
		os_c_write,
		os_c_writable,
		os_c_sync,
		os_c_connect,
		0
	}, {
		"/van/inet",
		"",
		"",
		cab_inet_open,
		os_inet_close,
		os_inet_read,
		os_inet_write,
		os_inet_writable,
		os_inet_sync,
		os_inet_connect,
		0
	}
};

/**
 * cable_system - global state of all controller neighbours.
 *
 * @os_busy:  if 1, os_init() has already been executed.
 * @test:     if 1, we want to determine the code coverage.
 * @objc:     giving the number of vcable_cmd() argument values.
 * @objv:     array with pointers to the vcable_cmd() argument values.
 **/
static struct cable_system_s {
	int        os_busy;
	int        test;
	int        objc;
	Tcl_Obj  **objv;
} cable_system;

/*============================================================================
  LOCAL FUNCTIONS
  ============================================================================*/
/**
 * cab_inet_open() - establish the inet connection between the vdisplay and the
 * vcontroller.
 *
 * @name:  no use.
 * @mode:  no use.
 *
 * Return:	return the connection id.
 **/
static int cab_inet_open(const char *name, int mode)
{
	Tcl_Obj  **objv;
	char *my_a_str, *my_p_str, *his_a_str, *his_p_str;
	int objc, i, my_p_int, my_p_len, his_p_int, his_p_len, cid;

	/* Get the pointer to the vcable_cmd() arguments. */
	objc = cable_system.objc;
	objv = cable_system.objv;

	/* Entry condition. */
	OS_TRAP_IF(objc != 7 || objv == NULL);
	
	/* 7: the inet cable shall be used.
	 * objv[1] describes the cable type: /van/display or /van/inet.
	 * objv[2] describes the network type: lo or lan.
	 * objv[3] describes the IPv4 address of vdisplay.
	 * objv[4] describes the port of the vdisplay.
	 * objv[5] describes the IPv4 address of the vcontroller.
	 * objv[6] describes the port of the vcontroller. */

	/* Initialize the argument index. */
	i = 2;

	/* Get the pointer to the inet addresses and ports. */

	/* vdisplay inet configuration. */
	i++;
	my_a_str  = objv[i]->bytes;
	
	i++;
	my_p_str  = objv[i]->bytes;
	my_p_len  = objv[i]->length;
	
	/* vcontroller inet configuration. */
	i++;
	his_a_str = objv[i]->bytes;
	
	i++;
	his_p_str = objv[i]->bytes;
	his_p_len = objv[i]->length;

	/* Convert the port strings to int. */
	my_p_int  = os_strtol_b10(my_p_str, my_p_len);
	his_p_int = os_strtol_b10(his_p_str, his_p_len);
	
	/* Allocate the socket resources. */
	cid = os_inet_open(my_a_str, my_p_int, his_a_str, his_p_int);

	/* Reset the arguments. */
	cable_system.objc = 0;
	cable_system.objv = NULL;	
	
	return cid;
}

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

	/* Test the option name. */
	if (os_strcmp(optionName, "-connect") == 0) {
		/* Get the connection status from vdisplay to vcontroller. */
		rv = c->connect(c->van_ep_id);

		/* Convert the connection status. */
		len = snprintf(buf, 8, "%d", rv);
	
		/* Save the return value. */
		Tcl_DStringAppend(optionValue, buf, len);
		return  TCL_OK;
	}

	return TCL_ERROR;
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
	OS_TRAP_IF(instanceData == NULL);

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
	if (i < CABLE_COUNT || cable_system.test)
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
	t->setOptionProc    = NULL;
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

	/* Ensure the one time call and prepare IPC about shared memory or inet.
	 * Make sure, that the cable controller is running. */
	if (! cable_system.os_busy && ! cable_system.test) {
		cable_system.os_busy = 1;
		os_init(c->os_conf);
		
		/* Deactivate the OS trace. */
		os_trace_button(0);
	}
	
	/* Insert the display plug. */
	c->van_ep_id = c->open(c->van_ep_name, 0);

	/* Initialize the input toggle. */
	c->inp_toggle = 1;
}
	
/**
 * vcable_cmd() - invoked as a Tcl command, to establish a connection to
 * the van cable controller.
 *
 * @cdata:   points to a data structure that describes what to do.
 * @interp:  Tcl interpreter handle.
 * @objc:    giving the number of argument values.
 * @objv:    array with pointers to the argument values.
 *
 * Return:	TCL_OK.
 **/
static int vcable_cmd(ClientData cdata, Tcl_Interp *interp, int objc,
		      Tcl_Obj *const objv[])
{
	struct cable_s *c;
	char *n;
	int i;

	/* Test the number of the arguments:
	 * 2: the shared memory cable shall be used:
	 * objv[1] describes the name of the vdisplay entry point.
	 *
	 * 7: the inet cable shall be used.
	 * objv[1] describes the cable type: /van/display or /van/inet.
	 * objv[2] describes the network type: lo or lan.
	 * objv[3] describes the IPv4 address of vdisplay.
	 * objv[4] describes the port of the vdisplay.
	 * objv[5] describes the IPv4 address of the vcontroller.
	 * objv[6] describes the port of the vcontroller. */
	if (objc != 2 && objc != 7) {
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

	/* Initialize the van OS configuration. */
	c->os_conf = 0;
	
	/* Test the interface type. */
	if (objc == 7) {
		/* Get the pointer to the network type. */
		n = objv[2]->bytes;
		
		/* Test the network type. */
		if (os_strcmp(n, "lan") == 0)
			c->os_conf = 1;
	}

	/* The entry point is available. */
	c->locked = 1;

	/* Save the pointer to the command arguments. */
	cable_system.objc = objc;
	cable_system.objv = (Tcl_Obj **) objv;
	
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

	/* vcable shall be invoked as a Tcl command, to establish a buffer or
	 * irq cable link to the van cable controller. */
	Tcl_CreateObjCommand(interp, "vcable", vcable_cmd, NULL, NULL);

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
 * @test_mode:  if 1, do not invoke os_init() and os_exit().
 *
 * Return:	None.
 **/
void os_tcl_init(int test_mode)
{
	/* Save the test mode. */
	cable_system.test = test_mode;
}
