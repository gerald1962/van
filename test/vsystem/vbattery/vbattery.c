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
 * bu_t - battery usage: list of the battery control button actions requested
 *        from system controller.
 *
 * @B_OFF:   disconnect the battery.
 * @B_ON:    activate the battery.
 * @B_STOP:  shutdown the battery system.
 **/
typedef enum {
	B_OFF = 0,
	B_ON,
	B_STOP
} bu_t;

/**
 * batt_ci_s - input from the controller.
 *
 * @cycle:   time stamp of the controller.
 * @ctrl_b:  battery use action by the controller.  
 * @rech_b:  current recharge button setting.
 **/
static struct batt_ci_s {
	int   cycle;
	bu_t  ctrl_b;
	int   rech_b;
} ci = { 0, B_OFF, 0 };

/**
 * batt_co_s - output to the controller.
 *
 * @cycle:  time stamp of the battery.
 * @vlt:    actual voltage of the battery.
 * @cur:    actual current of the battery.
 * @cha:    actual charge quantity.
 **/
static struct batt_co_s {
	int  cycle;
	int  vlt;
	int  crt;
	int  cha;
} co;

/**
 * batt_s_s - description of the battery state.
 *
 * @b_id:   end point of the battery cable.
 * @t_id:   id of the periodic control timer.
 * @clock:  interval of the periodic battery timer.
 * @vlt:    actual voltage of the battery.
 * @cur:    actual current of the battery.
 * @cc:     available charging current.
 * @cap:    maximum charge quantity of the battery.
 * @con:    actual current consumption or extracted charge from the battery.
 **/
static struct batt_s_s {
	int  b_id;
	int  t_id;
	int  clock;
	int  vlt;
	int  crt;
	int  cc;
	int  cap;
	int  con;
} bs = { -1, -1, 100, 1, 0, 2, 10000, 0 };
		

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
static void batt_write(int id, struct batt_co_s *co)
{
	char buf[OS_BUF_SIZE];
	int n, rv;

	/* Send voltage and current to the controller. */
	n = snprintf(buf, OS_BUF_SIZE, "cycle=%d::voltage=%d::current=%d::charge=%d:",
		     co->cycle, co->vlt, co->crt, co->cha);

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
static void batt_read(int id, struct batt_ci_s *ci)
{
	char buf[OS_BUF_SIZE], *s, *sep;
	int n;

	n = os_c_read(id, buf, OS_BUF_SIZE);

	/* Test the input state. */
	if (n < 1)
		return;

	/* Trace the message from controller. */
	printf("%s INPUT %s", P, buf);
	printf("\n");

	/* Locate the cycle string. */
	s = strstr(buf, "cycle=");
	OS_TRAP_IF(s == NULL);
	
	/* Extract the cycle value. */
	s += os_strlen("cycle=");

	/* Locate the separator. */
	sep = strchr(s, ':');
	OS_TRAP_IF(sep == NULL);

	/* Replace the separator with EOS. */
	*sep = '\0';

	/* Convert clock counter. */
	n = strtol(s, NULL, 10);

	/* Save the cycle counter. */
	ci->cycle = n;
	
	/* Restore the separator. */
	*sep = ':';
	
	/* Locate the battery control button string. */
	s = strstr(buf, "control_b=");
	OS_TRAP_IF(s == NULL);
	
	/* Jump over the battery control button string. */
	s += os_strlen("control_b=");

	/* Locate the separator. */
	sep = strchr(s, ':');
	OS_TRAP_IF(sep == NULL);

	/* Replace the separator with EOS. */
	*sep = '\0';

	/* Convert and test the received control button action. */
	if (os_strcmp(s, "B_ON") == 0) {
		/* Power on the battery. */
		ci->ctrl_b = B_ON;
	}
	else if (os_strcmp(s, "B_OFF") == 0) {
		/* Disconnect the battery. */
		ci->ctrl_b = B_OFF;

	}
	else if (os_strcmp(s, "B_STOP") == 0) {
		/* Shutdown the battery system. */
		ci->ctrl_b = B_STOP;
	}
	else {
		/* Invalid control battery action. */
		OS_TRAP();
	}
	
	/* Restore the separator. */
	*sep = ':';	
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
	int cycle;

	printf("vbattery\n");
	
	/* Prepare the battery end point. */
	os_init(0);

	/* Enable or disable the OS trace. */
	os_trace_button(0);

	/* Create the the end point for the controller. */
	bs.b_id = os_c_open("/van/battery", O_NBLOCK);

	/* Create the interval timer with a 100 millisecon period. */
	bs.t_id = os_clock_init("batt", bs.clock);

	/* Start the periodic timer. */
	os_clock_start(bs.t_id);

	/* Test loop. */
	for (cycle = 0; ci.ctrl_b != B_STOP; cycle += bs.clock) {
		/* Analyze the controller output signals. */
		batt_read(bs.b_id, &ci);

		/* Test the battery use requested by the controller. */
		switch (ci.ctrl_b) {
		case B_OFF:
			/* Disconnect the battery. */
			
			/* Change the current value. */
			bs.crt = 0;
			break;
			
		case B_ON:
			/* Power on the battery. */
			
			/* Change the current value. */
			bs.crt = bs.vlt;
			
			/* Simulate the power consumption. */
			bs.con += bs.vlt * bs.crt * bs.clock;

			/* Simulate the voltage collapse. */
			if (bs.con >= bs.cap) {

				printf("%s voltage collaps\n", P);
				
				/* Correct the power consumption. */
				bs.con = bs.cap;
				
				/* Voltage collaps. */
				bs.vlt = bs.crt = 0;
			}
#if 0
			/* Test the recharge condition. */
			if (bs.rech && bs.con > 0) {
				/* Recharge the battery. */
				bs.vlt = 1;
				bs.con -= bs.vlt * bs.cc * bs.clock;

				/* Correct the consumption value. */
				if (bs.con < 0)
					bs.con = 0;
			}
#endif
			break;
			
		case B_STOP:
			/* Shut down the van system. */
			continue;
			
		default:
			OS_TRAP();
			break;
		}
		
		if (bs.vlt == 0)
			printf("%s ERROR cycle=%d::battery collapsed\n", P, cycle);
		
		/* Send the signal to the controller. */
		co.cycle = cycle;
		co.vlt   = bs.vlt;
		co.crt   = bs.crt;
		co.cha   = bs.cap - bs.con;
		batt_write(bs.b_id, &co);
		
		/* Wait for the battery clock tick. */
		os_clock_barrier(bs.t_id);
	}
	
	/* Stop the clock. */
	os_clock_delete(bs.t_id);
	
	/* Release the battery end point. */
	os_c_close(bs.b_id);
	
	/* Release the OS resources. */
	os_exit();
	
	/* Print the goodby notificaton. */
	printf("vbattery done\n");

	return(0);
}
