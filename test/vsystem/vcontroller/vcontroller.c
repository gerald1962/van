// SPDX-License-Identifier: GPL-2.0

/*
 * van controller.
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
/**
 * bs_t - list of the battery states.
 *
 * @B_OFF:   disconnected battery.
 * @B_ON:    the battery has been activated.
 * @B_STOP:  shutown the battery system.
 **/
typedef enum {
	B_OFF = 0,
	B_ON,
	B_STOP
} bs_t;

/**
 * bs - controller status.
 *
 * @cycle:  time stamp - battery.
 * @vlt:    voltage - battery.
 * @crt:    current - battery.
 * @cha:    charching - battery.
 **/

static struct ctrl_s {
	struct ctrl_bi_s {
		int  cycle;
		int  vlt;
		int  crt;
	} bi;
	
	struct ctrl_bo_s {
		int   cycle;
		bs_t  button;
	} bo;
	
	struct ctrl_pi_s {
		int   cycle;
		bs_t  button;
	} pi;
	
	struct ctrl_po_s {
		int  cycle;
		int  vlt;
		int  crt;
		int  cha;
	} po;
	
	struct ctrl_s_s {
		bs_t  button;
		int   vlt;
		int   crt;
		int   cap;
		int   cha;
	} s;
} ctrl;

/*============================================================================
  LOCAL FUNCTION PROTOTYPES
  ============================================================================*/
/*============================================================================
  LOCAL FUNCTIONS
  ============================================================================*/
/**
 * ctrl_disp_write() - set the output to the display.
 *
 * @id:  battery controller cable id.
 * bo:   pointer to the ouput to the battery.
 *
 * Return:	None.
 **/
static void ctrl_disp_write(int id, struct ctrl_po_s* po)
{
	char buf[OS_BUF_SIZE];
	int n;

	/* Generate the output to the display. */
	n = snprintf(buf, OS_BUF_SIZE, "cycle=%d::voltage=%d::current=%d::charging=%d:",
		     po->cycle, po->vlt, po->crt, po->cha);

	/* Include EOS. */
	n++;

	/* Trace the message to battery. */
	printf("%s OUTPUT-D %s", P, buf);
	printf("\n");

	/* Send the signal to the display. */
	os_c_write(id, buf, n);
}

/**
 * ctrl_disp_read() - get the input from display.
 *
 * @id:  display controller cable id.
 * @pi:  pointer to the input from the display.
 *
 * Return:	None.
 **/
static void ctrl_disp_read(int id, struct ctrl_pi_s *pi)
{
	char buf[OS_BUF_SIZE], *s;
	int n;

	/* Wait for a display signal. */
	n = os_c_read(id, buf, OS_BUF_SIZE);

	/* Test the input state. */
	if (n < 2)
		return;

	/* Add EOS. */
	buf[n - 1] = '\0';

	/* Locate the button string. */
	s = strstr(buf, "button=");
	OS_TRAP_IF(s == NULL);
	
	/* Extract the button state. */
	s += os_strlen("button=");

	/* Convert and test the received button state. */
	if (os_strcmp(s, "B_ON") == 0) {
		/* Power on the battery. */
		pi->button = B_ON;
	}
	else if (os_strcmp(s, "B_OFF") == 0) {
		/* Disconnect the battery. */
		pi->button = B_OFF;

	}
	else if (os_strcmp(s, "B_STOP") == 0) {
		/* Shutdown the battery system. */
		pi->button = B_STOP;
	}
	else {
		/* Invalid battery state. */
		OS_TRAP();
	}

	printf("%s INPUT-P %s", P, buf);		
	printf("\n");
}

/**
 * ctrl_batt_write() - set the output to the battery.
 *
 * @id:  battery controller cable id.
 * bo:   pointer to the ouput to the battery.
 *
 * Return:	None.
 **/
static void ctrl_batt_write(int id, struct ctrl_bo_s* bo)
{
	char buf[OS_BUF_SIZE];
	int n;

	/* Define a battery signal. */
	n = snprintf(buf, OS_BUF_SIZE, "cycle=%d::button=%d", bo->cycle, bo->button);

	/* Include EOS. */
	n++;

	/* Trace the message to battery. */
	printf("%s OUTPUT-B %s", P, buf);
	printf("\n");

	/* Send the signal to the battery. */
	os_c_write(id, buf, n);

	/* Test the button state. */
	if (bo->button == B_STOP) {
		/* Wait for the processing of the stop signal. */
		do {
			os_clock_msleep(1);
			
		} while (os_c_sync(id));
	}
}

/**
 * ctrl_batt_read() - get the input from the battery.
 *
 * @id:  battery controller cable id.
 * @bi:  pointer to the input from the battery
 *
 * Return:	None.
 **/
static void ctrl_batt_read(int id, struct ctrl_bi_s *bi)
{
	char buf[OS_BUF_SIZE], *s, *l;
	int n;
	
	n = os_c_read(id, buf, OS_BUF_SIZE);

	/* Test the input state. */
	if (n < 1)
		return;

	/* Trace the battery message. */
	printf("%s INPUT-B %s", P, buf);		
	printf("\t");

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
	
	/* Convert and test the received cycle. */
	n = strtol(s, NULL, 10);

	/* Save the cycle counter. */
	bi->cycle = n;
	
	/* Replace EOS with the separator. */
	*l = ':';

	/* Locate the voltage string. */
	s = strstr(buf, "voltage=");
	OS_TRAP_IF(s == NULL);
	
	/* Extract the voltage value. */
	s += os_strlen("voltage=");

	/* Locate the separator. */
	l = strchr(s, ':');
	OS_TRAP_IF(l == NULL);

	/* Replace the separator with EOS. */
	*l = '\0';
	
	/* Convert and test the received voltage. */
	n = strtol(s, NULL, 10);

	/* Save the voltage. */
	bi->vlt = n;
	
	/* Replace EOS with the separator. */
	*l = ':';

	/* Locate the current string. */
	s = strstr(buf, "current=");
	OS_TRAP_IF(s == NULL);
	
	/* Extract the current value. */
	s += os_strlen("current=");

	/* Convert and test the received current. */
	n = strtol(s, NULL, 10);

	/* Save the current. */
	bi->crt = n;
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
	int b_id, d_id, t_id, clock, cycle;
	struct ctrl_s_s *s = &ctrl.s;
	struct ctrl_bo_s *bo = &ctrl.bo;
	struct ctrl_bi_s *bi = &ctrl.bi;
	struct ctrl_po_s *po = &ctrl.po;
	struct ctrl_pi_s *pi = &ctrl.pi;
	
	printf("vcontroller\n");
	
	/* Install communicaton resources. */
	os_init(1);

	/* Enable or disable the OS trace. */
	os_trace_button(0);

	/* Create the the end point for the battery and display. */
	b_id = os_c_open("/van/ctrl_batt", O_NBLOCK);
	d_id = os_c_open("/van/ctrl_disp", O_NBLOCK);

	/* Initialize the controller. */
	s->cap = 10000;
	s->cha = s->cap;
	clock  = 250;
	
	/* Create the interval timer with x millisecond period. */
	t_id = os_clock_init("ctrl", clock);
	
	/* Start the periodic timer. */
	os_clock_start(t_id);

	/* Test loop. */
	for (cycle = 0; s->button != B_STOP; cycle += clock) {

		/* Get the input from the battery. */
		ctrl_batt_read(b_id, bi);
		s->vlt = bi->vlt;
		s->crt = bi->crt;

		/* Get the input from the display. */
		ctrl_disp_read(d_id, pi);
		s->button = pi->button;
		
		/* Calculate the charging value: C = I * t. */
		s->cha -= s->crt * clock;

		/* Test the battery capacitiy and the activity. */
                if (s->cha <= 0 && s->button == B_ON)
			s->button = B_OFF; /* Switch off the battery. */
		
		/* Set the output to the battery. */
		bo->cycle  = cycle;
		bo->button = s->button;
	 	ctrl_batt_write(b_id, bo);

		/* Test the stop button. */
		if (s->button != B_STOP) {
			/* Set the output to the display. */
			po->cycle = cycle;
			po->vlt   = s->vlt;
			po->crt   = s->crt;
			po->cha   = s->cha;
			ctrl_disp_write(d_id, po);
		}
		
		/* Wait for the controller clock tick. */
		os_clock_barrier(t_id);
	}

	/* Print the goodby notificaton. */
	printf("vcontroller done \n");
	
	/* Stop the clock. */
	os_clock_delete(t_id);
	
	/* Release the end points. */
	os_c_close(b_id);
	os_c_close(d_id);
	
	/* Release the OS resources. */
	os_exit();
	
	return(0);
}
