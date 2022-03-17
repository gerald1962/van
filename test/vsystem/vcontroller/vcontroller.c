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

/* Max. length of an IPv4 string buffer. */
#define CTRL_IPv4_ADDR_LEN  32

/*============================================================================
  MACROS
  ============================================================================*/
/**
 * CTRL_IPV4_ERROR() - parse error of an IPv4 address string.
 *
 * Return:	None.
 **/
#define CTRL_IPV4_ERROR() do { \
	printf("%s error: IPv4 address format error\n", P); \
	ctrl_usage(); \
	exit(1); \
} while (0)

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
 * @rx_cnt:  number of the receved controller messages from the display.
 * @tx_cnt:  number of the sent messages to the controller.
 * @cycle:   time stamp - battery.
 * @ctrl_b:  current contol button setting.
 * @rech_b:  current recharge button setting.
 * @vlt:     voltage - battery.
 * @crt:     current - battery.
 * @cha:     charge quantity - battery.
 * @lev:     fill leve of the battery.
 *
 * @is_inet:   if 1, vcontroller and vdisplay are connected over inet.
 * @my_addr:   IPv4 address of the vcontroller.
 * @his_addr:  IPv4 address of the vdisplay.
 * @rx_cnt:    number of the received message from the display.
 * @tx_cnt:    number of the sent messages to the display.
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
		int   rx_cnt;
		int   tx_cnt;
		int   cycle;
		cb_t  ctrl_b;
		int   rech_b;
	} pi;
	
	struct ctrl_po_s {
		int  rx_cnt;
		int  tx_cnt;
		int  cycle;
		int  vlt;
		int  crt;
		int  cha;
		int  lev;
	} po;
	
	struct ctrl_s_s {
		int   is_inet;
		char  my_addr[CTRL_IPv4_ADDR_LEN];
		char  his_addr[CTRL_IPv4_ADDR_LEN];
		int   rx_cnt;
		int   tx_cnt;
	
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
	n = snprintf(buf, OS_BUF_SIZE,
		     "rxno=%d::txno=%d::cycle=%d::voltage=%d::current=%d::charge=%d::level=%d:",
		     po->rx_cnt, po->tx_cnt, po->cycle, po->vlt, po->crt, po->cha, po->lev);

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
	int b_len, m_len, e_len;

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
	
	/* Get the rx counter from the display. */
	pi->rx_cnt = ctrl_read_int(buf, m_len, "rxno=");
	
	/* Get the tx counter from the display. */
	pi->tx_cnt = ctrl_read_int(buf, m_len, "txno=");
	
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
	
	/* Get the battery recharge setting. */
	pi->rech_b = ctrl_read_int(buf, m_len, "recharge_b=");
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

	/* Update the send counter. */
	s->tx_cnt++;
	
	/* Set the output to the display. */
	po->rx_cnt = s->rx_cnt;
	po->tx_cnt = s->tx_cnt;
	po->cycle  = cycle;
	po->vlt    = s->vlt;
	po->crt    = s->crt;
	po->cha    = bi->cha;  /* XXX */
	po->lev    = s->lev;
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
	s->rx_cnt = pi->tx_cnt;
	
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
 * ctrl_usage() - provide information about the vcontroller configuration.
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
 * ctrl_ipv4_parse() - parse an IPv4 address string.
 *
 * @src:   input of the IPv4 address string parser.
 * @dest:  output of the IPv4 address string parser.
 * @done:  if 1, the IPv4 address is already preseent.
 *
 * Return:	None.
 **/
static void ctrl_ipv4_parse(char *src, char *dest, int done)
{
	/* One of the four integers of an IPv4 address and segment counter. */
	int i, c, is_empty;
	char *p;

	/* Entry condition. */
	if (done)
		CTRL_IPV4_ERROR();
	
	/* Initialze the segment integer and counter of an IPv4 address. */
	is_empty = 1;
	i = 0;
	c = 0;
	
	/* Loop thru the IP string. */
	for (p = src; *p != '\0'; p++) {
		/* Analyze the current character. */
		if (isdigit(*p)) {
			/* Update the digit test. */
			is_empty = 0;
			
			/* Update the segment integer. */
			i = (i * 10) + (*p - '0');
			
			/* Test the integer value of an IPv4 segement digit
			 * sequence: 127.0.0.256 */
			if (i > 255)
				CTRL_IPV4_ERROR();
		}
		else if (*p == '.') {
			/* Jump to the next segment of an IPv4 adderss: test the
			 * segment counter: 127.0..1  */
			if (c > 3 || is_empty)
				CTRL_IPV4_ERROR();

			/* Reset the digit test. */
			is_empty = 1;
			
			/* Reset the segment integer of an IPv4 address. */
			i = 0;

			/* Update the segment counter. */
			c++;
		}
		else {
			/* The vcontroller has been invoked with a strange IPv4 address. */
			CTRL_IPV4_ERROR();
		}
	}

	/* Test the segment counter. */
	if (! is_empty && c == 3) {
		/* Save the IPv4 address. */
		os_strcpy(dest, CTRL_IPv4_ADDR_LEN, src);
		return;
	}
	
	/* Invalid IPv4 format: 127.0.0 or 127.0 ... */
	CTRL_IPV4_ERROR();
}

/**
 * ctrl_ip_addr() - get the IP addresses.
 *
 * @argc:  argument counter.
 * @argv:  list of the arguments.
 *
 * Return:	None.
 **/
static void ctrl_ip_addr(int argc, char *argv[])
{
	struct ctrl_s_s *s;
	int my_addr, his_addr, opt;

	/* Get the reference to the controller state. */
	s = &ctrl.s;
	
	/* Initialize the IPv4 address test. */
	my_addr  = 0;
	his_addr = 0;
	
	/* Analyze the vcontroller arguments. */
	while ((opt = getopt(argc, argv, "c:d:")) != -1) {
		/* Analyze the current argument. */
		switch (opt) {
		case 'c':
			/* Parse the IPv4 address of the vcontroller. */
			ctrl_ipv4_parse(optarg, s->my_addr, my_addr);
			my_addr = 1;
			break;
		case 'd':
			/* Parse the IPv4 address of the vdisplay. */
			ctrl_ipv4_parse(optarg, s->his_addr, his_addr);
			his_addr = 1;
			break;
		default:
			ctrl_usage();
			exit(1);
			break;

		}
	}

	/* Exit condition. */
	if (! my_addr || ! his_addr) {
		ctrl_usage();
		exit(1);
	}

	/* Connect the vcontroller and the vdisplay over inet. */
	s->is_inet = 1;
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
	/* verify the arguments. */
	switch(argc) {
	case 1:
		/* The vcontroller shall be called as follows: vcontroller. 
		 * In this case the vcontroller and the vdisplay are connected
		 * over shared memory. */
		break;
	case 2:
		/* The vcontroller shall be called as follows: 
		 * vcontroller -h or x, to get usage information. */
		
		/* Support the user. */
		ctrl_usage();
		exit(0);
		break;
	case 5:
		/* The vcontroller shall be called as follows: 
		 * vcontroller -c 127.0.0.1 -d 127.0.0.1 
		 * In this case the vcontroller and vdisplay are connected over
		 * the internet. */
		
		/* Get the IP addresses. */
		ctrl_ip_addr(argc, argv);
		break;
	default:
		/* Support the user. */
		ctrl_usage();
		exit(1);
		break;
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

	/* Analyze the vcontroller arguments. */
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
	
	return (0);
}
