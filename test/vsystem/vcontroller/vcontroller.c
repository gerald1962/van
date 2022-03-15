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
/**
 * cb_t - list of the battery control button actions requested from the display
 *        actuators.
 *
 * @B_OFF:   disconnect the battery.
 * @B_ON:    activate the battery.
 * @B_STOP:  shutown the battery system.
 **/
typedef enum {
	B_OFF = 0,
	B_ON,
	B_STOP
} cb_t;

/**
 * ctrl - controller status.
 *
 * @bi:  input from the battery.
 * @bo:  output to the battery
 * @pi:  input from the panel or display.
 * @po:  output to the panel or display.
 * @s:   current controller state.
 *
 * @b_id:    end point of the battery cable.
 * @d_id:    end point of the display cable.
 * @t_id:    id of the periodic control timer.
 * @clock:   time intervall of the battery controller.
 *
 * @cycle:   time stamp - battery.
 * @ctrl_b:  current contol button setting.
 * @rech_b:  current recharge button setting.
 * @vlt:     voltage - battery.
 * @crt:     current - battery.
 * @cha:     charge quantity - battery.
 **@lev:     fill leve of the battery.
 **/
static struct ctrl_s {
	struct ctrl_bi_s {
		int  cycle;
		int  vlt;
		int  crt;
		int  cha;
	} bi;
	
	struct ctrl_bo_s {
		int   cycle;
		cb_t  ctrl_b;
		int   rech_b;
	} bo;
	
	struct ctrl_pi_s {
		int   cycle;
		cb_t  ctrl_b;
		int   rech_b;
	} pi;
	
	struct ctrl_po_s {
		int  cycle;
		int  vlt;
		int  crt;
		int  cha;
		int  lev;
	} po;
	
	struct ctrl_s_s {
		int   b_id;
		int   d_id;
		int   t_id;
		int   clock;
		
		cb_t  ctrl_b;
		int   rech_b;
		int   vlt;
		int   crt;
		int   cap;
		int   cha;
		int   lev;
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
	n = snprintf(buf, OS_BUF_SIZE, "cycle=%d::voltage=%d::current=%d::charge=%d::level=%d:",
		     po->cycle, po->vlt, po->crt, po->cha, po->lev);

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
	char buf[OS_BUF_SIZE], *s, *sep;
	int b_len, m_len, e_len, n;

	/* Wait for a display signal. */
	b_len = os_c_read(id, buf, OS_BUF_SIZE);

	/* Test the input state. */
	if (b_len < 2)
		return;

	/* Save the message len. */
	m_len = b_len - 1;
	
	/* Add EOS. */
	buf[m_len] = '\0';

	/* Trace the input signal from the display. */
	printf("%s INPUT-D %s", P, buf);		
	printf("\n");
	
	/* Locate the control button string. */
	s = os_strstr(buf, m_len, "control_b=");
	OS_TRAP_IF(s == NULL);
	
	/* Jump over the control button string. */
	s += os_strlen("control_b=");

	/* Locate the separator of the message element. */
	e_len = os_strlen(s);
	sep = os_strchr(s, e_len, ':');
	OS_TRAP_IF(sep == NULL);
	
	/* Replace the message element separator with EOS. */
	*sep = '\0';

	/* Convert and test the received control button action. */
	if (os_strcmp(s, "B_ON") == 0) {
		/* Power on the battery. */
		pi->ctrl_b = B_ON;
	}
	else if (os_strcmp(s, "B_OFF") == 0) {
		/* Disconnect the battery. */
		pi->ctrl_b = B_OFF;

	}
	else if (os_strcmp(s, "B_EMPTY") == 0) {
		/* The controller has already deactivated the battery
		 * automatically. */
	}
	else if (os_strcmp(s, "B_STOP") == 0) {
		/* Shutdown the battery system. */
		pi->ctrl_b = B_STOP;
	}
	else {
		/* Invalid control battery action. */
		OS_TRAP();
	}

	/* Insert the message element separator again. */
	*sep = ':';
	
	/* Locate the recharge button string. */
	s = os_strstr(buf, m_len, "recharge_b="); 
	OS_TRAP_IF(s == NULL);
	
	/* Jump over the recharge button setting. */
	s += os_strlen("recharge_b=");

	/* Locate the separator of the message element. */
	e_len = os_strlen(s);
	sep = os_strchr(s, e_len, ':');
	OS_TRAP_IF(sep == NULL);
	
	/* Replace the separator with EOS. */
	*sep = '\0';

	/* Convert and test the received recharge button setting. */	
	e_len = os_strlen(s);
	n = os_strtol_b10(s, e_len);

	/* Update the recharge operation. */
	pi->rech_b = n;
	
	/* Insert the message element separator again. */
	*sep = ':';
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
	char *s, buf[OS_BUF_SIZE];
	int n;

	/* Define the battery signal. */
	switch (bo->ctrl_b) {
	case B_OFF:
		s = "B_OFF";
		break;
	case B_ON:
		s = "B_ON";
		break;
	case B_STOP:
		s = "B_STOP";
		break;
	default:
		s = "";
		break;
	}
	
	n = snprintf(buf, OS_BUF_SIZE, "cycle=%d::control_b=%s::recharge_b=%d:",
		     bo->cycle, s, bo->rech_b);

	/* Include EOS. */
	n++;

	/* Trace the message to battery. */
	printf("%s OUTPUT-B %s", P, buf);
	printf("\n");

	/* Send the signal to the battery. */
	os_c_write(id, buf, n);

	/* Test the button state. */
	if (bo->ctrl_b == B_STOP) {
		/* Wait for the processing of the stop signal. */
		do {
			os_clock_msleep(1);
			
		} while (os_c_sync(id));
	}
}

/**
 * ctrl_read_int() - search for the pattern combined with the digit string and
 * convert the digit string to int.
 *
 * @buf:  source buffer.
 * @len:  length of the source buffer.
 * @pat:  search pattern.
 *
 * Return:	the digit string as integer.
 **/
static int ctrl_read_int(char *buf, int len, char *pat)
{
	char *s, *sep;
	int n, i;
	
	/* Locate the pattern string. */
	s = os_strstr(buf, len, pat);
	OS_TRAP_IF(s == NULL);
	
	/* Jump over the pattern string. */
	s += os_strlen(pat);;

	/* Locate the separator of the message element. */
	n = os_strlen(s);
	sep = os_strchr(s, n, ':');
	OS_TRAP_IF(sep == NULL);

	/* Replace the separator with EOS. */
	*sep = '\0';
	
	/* Convert the received digits. */
	n = os_strlen(s);
	i = os_strtol_b10(s, n);

	/* Replace EOS with the separator. */
	*sep = ':';

	return i;
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
	char buf[OS_BUF_SIZE];
	int n;
	
	n = os_c_read(id, buf, OS_BUF_SIZE);

	/* Test the input state. */
	if (n < 2)
		return;

	/* Trace the battery message. */
	printf("%s INPUT-B %s", P, buf);		
	printf("\t");

	/* Substract EOS. */
	n--;
	
	/* Get the battery time stamp. */
	bi->cycle = ctrl_read_int(buf, n, "cycle=");
	
	/* Get the voltage value. */
	bi->vlt = ctrl_read_int(buf, n, "voltage=");

	/* Get the current value. */
	bi->crt = ctrl_read_int(buf, n, "current=");

	/* Get the charge value. */
	bi->cha = ctrl_read_int(buf, n, "charge=");
}

/**
 * ctrl_write() - send the output signals to the battery and display.
 *
 * @cycle:  current timestamp of the battery controller.
 *
 * Return:	None.
 **/
static void ctrl_write(int cycle)
{
	struct ctrl_s_s *s = &ctrl.s;
	struct ctrl_bi_s *bi = &ctrl.bi;
	struct ctrl_bo_s *bo = &ctrl.bo;
	struct ctrl_po_s *po = &ctrl.po;
	
	/* Set the output to the battery. */
	bo->cycle  = cycle;
	bo->ctrl_b = s->ctrl_b;
	bo->rech_b = s->rech_b;
	ctrl_batt_write(s->b_id, bo);

	/* Test the battery capacitiy and the activity. */
	if (s->cha <= 0) {
		/* Update the battery control operation. */
		bo->ctrl_b = B_OFF;
	}

	/* Test the stop button. */
	if (s->ctrl_b == B_STOP)
		return;
	
	/* Set the output to the display. */
	po->cycle = cycle;
	po->vlt   = s->vlt;
	po->crt   = s->crt;
	po->cha   = bi->cha;  /* XXX */
	po->lev   = s->lev;
	ctrl_disp_write(s->d_id, po);
}

/**
 * ctrl_read() - get the input from the battery and display.
 *
 * Return:	None.
 **/
static void ctrl_read(void)
{
	struct ctrl_s_s *s = &ctrl.s;
	struct ctrl_bi_s *bi = &ctrl.bi;
	struct ctrl_pi_s *pi = &ctrl.pi;

	/* Get the input from the battery. */
	ctrl_batt_read(s->b_id, bi);
	s->vlt = bi->vlt;
	s->crt = bi->crt;

	/* Get the input from the display. */
	ctrl_disp_read(s->d_id, pi);
	s->ctrl_b = pi->ctrl_b;
	s->rech_b = pi->rech_b;		
}

/**
 * ctrl_cleanup() - release the battery controller resources.
 *
 * Return:	None.
 **/
static void ctrl_cleanup(void)
{
	struct ctrl_s_s *s = &ctrl.s;	
	
	/* Stop the clock. */
	os_clock_delete(s->t_id);
	
	/* Release the end points. */
	os_c_close(s->b_id);
	os_c_close(s->d_id);
	
	/* Release the OS resources. */
	os_exit();
	
	/* Print the goodby notificaton. */
	printf("vcontroller done \n");	
}
		
/**
 * ctrl_init() - initialize the battery controller state.
 *
 * Return:	None.
 **/
static void ctrl_init(void)
{
	struct ctrl_s_s *s = &ctrl.s;
	
	printf("vcontroller\n");
	
	/* Install communicaton resources. */
	os_init(1);

	/* Enable or disable the OS trace. */
	os_trace_button(0);

	/* Create the end points for the battery and display. */
	s->b_id = os_c_open("/van/ctrl_batt", O_NBLOCK);
	s->d_id = os_c_open("/van/ctrl_disp", O_NBLOCK);

	/* Initialize the controller. */
	s->cap   = 10000;
	s->cha   = s->cap;
	s->clock = 250;
	
	/* Create the interval timer with x millisecond period. */
	s->t_id = os_clock_init("ctrl", s->clock);
	
	/* Start the periodic timer. */
	os_clock_start(s->t_id);
}

/**
 * ctrl_usage() - provide information aboute the vcontroller configuration.
 *
 * Return:	None.
 **/
static void ctrl_usage(void)
{
	printf("vcontrol - van battery controller\n");
	printf("  -h          show this usage\n");
	printf("  -c ip-addr  ip address of the vcontroller\n");
	printf("  -d ip-addr  ip address of the vdisplay\n");
}

/**
 * ctrl_argv() - analyze the controller arguments.
 *
 * @argc:  argument counter.
 * @argv:  list of the arguments.
 *
 * Return:	None.
 **/
static void ctrl_argv(int argc, char *argv[])
{
	/* Test the number of the controller arguments. */
	if (argc != 1 && argc != 2 && argc != 5) {
		/* Support the user. */
		ctrl_usage();
		exit(1);
	}
}

/*============================================================================
  EXPORTED FUNCTIONS
  ============================================================================*/
/**
 * main() - start function of the van battery controller.
 *
 * @argc:  argument counter.
 * @argv:  list of the arguments.
 *
 * Return:	0 or force a software trap.
 **/
int main(int argc, char *argv[])
{
	int cycle;
	struct ctrl_s_s *s = &ctrl.s;
	struct ctrl_bi_s *bi = &ctrl.bi;

	/* Analyze the controller arguments. */
	ctrl_argv(argc, argv);
	
	/* Initialize the battery controller state. */
	ctrl_init();

	/* Test loop. */
	for (cycle = 0; s->ctrl_b != B_STOP; cycle += s->clock) {
		/* Get the input from the battery and display. */
		ctrl_read();
		
		/* Calculate the charging value: C = I * t. */
		s->cha -= s->crt * s->clock;

		/* XXX Calculate the the fill level of the battery in percent. */
#if 0
		s->lev = (s->cha * 100) / s->cap;
#else
		s->lev = (bi->cha * 100) / s->cap;
#endif

		/* Send the output signals to the battery and display. */
		ctrl_write(cycle);
				
		/* Wait for the controller clock tick. */
		os_clock_barrier(s->t_id);
	}

	/* Release the battery controller resources. */
	ctrl_cleanup();
	
	return(0);
}
