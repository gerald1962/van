// SPDX-License-Identifier: GPL-2.0

/*
 * van controller.
 *
 * Copyright (C) 2022 Gerald Schueller <gerald.schueller@web.de>
 */

/*============================================================================
  IMPORTED INCLUDE REFERENCES
  ============================================================================*/
#include "os.h"      /* Operating system: os_sem_create(). */

/*============================================================================
  EXPORTED INCLUDE REFERENCES
  ============================================================================*/
/*============================================================================
  LOCAL NAME CONSTANTS DEFINITIONS
  ============================================================================*/
#define P "C>"  /* Controller prompt. */

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
 * ctrl_disp_read() - analyze the display output.
 *
 * @id:  display controller cable id.
 *
 * Return:	None.
 **/
static void ctrl_disp_read(int id)
{
	char buf[OS_BUF_SIZE];
	int n;

	/* Wait for a display signal. */
	n = os_c_read(id, buf, OS_BUF_SIZE);

	/* Test the input state. */
	if (n < 1)
		return;

	/* Trace the display message. */
	printf("%s: n=%d, buf=%s\n", P, n, buf);		
}

/**
 * ctrl_batt_write() - send a signal to the battery.
 *
 * @id:      battery controller cable id.
 * @cycle:   time stamp.
 * @button:  switch on or off the battery.
 * Return:	None.
 **/
static void ctrl_batt_write(int id, int cycle, int button)
{
	char buf[OS_BUF_SIZE];
	int n;

	/* Define a battery signal. */
	n = snprintf(buf, OS_BUF_SIZE, "cycle=%d::button=%d", cycle, button);

	/* Include EOS. */
	n++;
	
	/* Send the signal to the battery. */
	os_c_write(id, buf, n);
}

/**
 * ctrl_batt_read() - analyze the battery output signals.
 *
 * @id:  battery controller cable id.
 *
 * Return:	None.
 **/
static void ctrl_batt_read(int id)
{
	char buf[OS_BUF_SIZE];
	int n;
	
	n = os_c_read(id, buf, OS_BUF_SIZE);

	/* Test the input state. */
	if (n < 1)
		return;

	/* Trace the battery message. */
	printf("%s %s\n", P, buf);		
}

/*============================================================================
  EXPORTED FUNCTIONS
  ============================================================================*/
/**
 * main() - start function of the control technology platform.
 *
 * Return:	0 or force a software trap.
 **/
int main(void)
{
	int b_id, d_id, t_id, cycle, button;
	
	printf("vcontroller\n");
	
	/* Install communicaton resources. */
	os_init(1);

	/* Enable or disable the OS trace. */
	os_trace_button(0);

	/* Create the the end point for the battery and display. */
	b_id = os_c_open("/van/ctrl_batt", O_NBLOCK);
	d_id = os_c_open("/van/ctrl_disp", O_NBLOCK);
	
	/* Create the interval timer with x millisecond period. */
	t_id = os_clock_init("ctrl", 250);
	
	/* Start the periodic timer. */
	os_clock_start(t_id);

	/* Initialize the battery button. */
	button = 0;
	
	/* Test loop. */
	for (cycle = 1;; cycle++) {
		/* Analyze the battery output signals. */
		ctrl_batt_read(b_id);

		/* Analyze the display output signals. */
		ctrl_disp_read(d_id);

		/* Switch on the battery. */
		if (cycle == 20) {
			/* Change the battery button value. */
			button = 1;
		}
		
		/* Send a signal to the battery. */
		ctrl_batt_write(b_id, cycle, button);

		/* Wait for the controller clock tick. */
		os_clock_barrier(t_id);
	}

	/* Release the end points. */
	os_c_close(b_id);
	os_c_close(d_id);
	
	/* Release the OS resources. */
	os_exit();
	
	return(0);
}
