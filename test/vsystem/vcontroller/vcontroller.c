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
 * main() - start function of the control technology platform.
 *
 * Return:	0 or force a software trap.
 **/
int main(void)
{
	char buf[64];
	int b_id, d_id, i, size;
	
	printf("vcontroller\n");
	
	/* Install communicaton resources. */
	os_init(1);

	/* Enable OS trace. */
	os_trace_button(0);

	/* Create the the end point for the battery and display. */
	b_id = os_c_open("/ctrl_batt", O_NBLOCK);
	d_id = os_c_open("/ctrl_disp", O_NBLOCK);

	/* Test loop. */
	for(i = 1;; ) {
		/* Wait for the display test message. */
		size = os_c_read(d_id, buf, 64);

		/* Test the input state. */
		if (size > 0) {
			buf[size] = '\0';
			printf("Display: %d: m=\"%s\", s=%d\n", i, buf, size);
			i++;
		}
		
		/* Sleep some milliseconds. */
		os_clock_msleep(100);
	}

	/* Release the end points. */
	os_c_close(b_id);
	os_c_close(d_id);
	
	/* Release the OS resources. */
	os_exit();
	
	return(0);
}
