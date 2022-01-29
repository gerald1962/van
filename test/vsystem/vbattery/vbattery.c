// SPDX-License-Identifier: GPL-2.0

/*
 * van battery.
 *
 * Copyright (C) 2022 Gerald Schueller <gerald.schueller@web.de>
 */

/*============================================================================
  IMPORTED INCLUDE REFERENCES
  ============================================================================*/
#include <string.h>  /* String operations. */
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
#define P "B>"  /* Battery prompt. */

/*============================================================================
  LOCAL TYPE DEFINITIONS
  ============================================================================*/
/*============================================================================
  LOCAL DATA
  ============================================================================*/
/**
 * bs - battery status.
 *
 * @cycle:  time stamp from controller.
 * @but:    battery activity.
 * @vlt:    voltage.
 * @crt:    current.
 * @cap:    capacity.
 * @con:    consumption.   
 **/
static struct batt_s {
	int  cycle;
	int  but;
	int  vlt;
	int  crt;
	int  cap;
	int  con;
} bs = { 0, 0, 1, 0, 10, 0 };

/*============================================================================
  LOCAL FUNCTION PROTOTYPES
  ============================================================================*/
/*============================================================================
  LOCAL FUNCTIONS
  ============================================================================*/
/**
 * batt_write() - send signals to the controller.
 *
 * @id:  controller battery cable id.
 *
 * Return:	None.
 **/
static void batt_write(int id)
{
	char buf[OS_BUF_SIZE];
	int n, rv;

	/* Send voltage and current to the controller. */
	n = snprintf(buf, OS_BUF_SIZE, "cycle=%d::voltage=%d::current=%d",
		     bs.cycle, bs.vlt, bs.crt);

	/* Include EOS. */
	n++;
	
	/* Send voltage and current to the controller. */
	rv = os_c_write(id, buf, n);

	/* Test the input state. */
	if (rv < 1)
		return;

	OS_TRAP_IF(rv != n);	
}

/**
 * batt_read() - analyze the controller output.
 *
 * @id:  controller battery cable id.
 *
 * Return:	None.
 **/
static void batt_read(int id)
{
	char buf[OS_BUF_SIZE], *s, *l;
	int n;
	
	n = os_c_read(id, buf, OS_BUF_SIZE);

	/* Test the input state. */
	if (n < 1)
		return;

	/* Trace the controller message. */
	printf("%s %s\n", P, buf);

	/* Locate the cycle string. */
	s = strstr(buf, "cycle=");
	OS_TRAP_IF(s == NULL);
	
	/* Extract the cycle value. */
	s += os_strlen("cycle=");

	/* Locate the separator. */
	l = strchr(s, ':');
	OS_TRAP_IF(l == NULL);

	/* Replace the separator with EOS. */
	*l = '\0';
	
	/* Convert and test the received button state. */
	n = strtol(s, NULL, 10);

	/* Save the cycle counter. */
	bs.cycle = n;
	
	/* Replace EOS with the separator. */
	*l = ':';
	
	/* Locate the button string. */
	s = strstr(buf, "button=");
	OS_TRAP_IF(s == NULL);
	
	/* Extract the button state. */
	s += os_strlen("button=");

	/* Convert and test the received button state. */
	n = strtol(s, NULL, 10);
	OS_TRAP_IF(n != 0 && n != 1);

	/* Save the button state. */
	bs.but = n;
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
	int b_id, t_id;
	
	printf("vbattery\n");
	
	/* Prepare the battery end point. */
	os_init(0);

	/* Enable or disable the OS trace. */
	os_trace_button(0);

	/* Create the the end point for the controller. */
	b_id = os_c_open("/van/battery", O_NBLOCK);

	/* Create the interval timer with a 100 millisecon period. */
	t_id = os_clock_init("batt", 100);

	/* Start the periodic timer. */
	os_clock_start(t_id);

	/* Test loop. */
	for (;;) {
		/* Analyze the controller output signals. */
		batt_read(b_id);

		/* Test the button state. */
		if (bs.but == 1) {
			/* Change the current value. */
			bs.crt = 1;
			
			/* Simulate the power consumption. */
			bs.con += bs.vlt * bs.crt;
			
			/* Simulate the voltage. */
			if (bs.con >= bs.cap) {
				/* Correct power consumption. */
				bs.con = bs.cap;
				
				/* Voltage collaps. */
				bs.vlt = 0;
				bs.crt = 0;
			}
		}

		/* Send signals to the controller. */
		batt_write(b_id);
		
		/* Wait for the battery clock tick. */
		os_clock_barrier(t_id);
	}
	
	/* Release the battery end point. */
	os_c_close(b_id);
	
	/* Release the OS resources. */
	os_exit();
	
	return(0);
}
