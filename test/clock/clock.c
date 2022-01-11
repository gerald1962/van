// SPDX-License-Identifier: GPL-2.0

/*
 * tim - OS timer test.
 *
 * Copyright (C) 2022 Gerald Schueller <gerald.schueller@web.de>
 */

/*============================================================================
  IMPORTED INCLUDE REFERENCES
  ============================================================================*/
#include "os.h"    /* Operating system: os_sem_create(). */
#include <time.h>  /* Get time in seconds: time(). */

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
 * c - Clock configuration 
 *
 * @interval:  interval time in millisecons.
 * @cycles  :  number of test cycles.
**/
static struct {
	int interval;
	int cycles;
} clk;

/*============================================================================
  LOCAL FUNCTION PROTOTYPES
  ============================================================================*/
static void clock_usage(void);

/*============================================================================
  LOCAL FUNCTIONS
  ============================================================================*/
/**
 * clock_atoi() - convert a string to integer.
 *
 * @string:   pointer to the gotten string.
 *
 * Return:	integer.
 **/
static int clock_atoi(char *string)
{
	int i;
	
	/* Entry condition. */
	if (string == NULL) {
		clock_usage();
		exit(1);
	}

	/* Convert the string to integer in the decimal system. */
	i = strtol(string, NULL, 10);

	/* Test the integer. */
	if (i < 1) {
		clock_usage();
		exit(1);
	}

	return i;
}

/**
 * clock_usage() - provide information about the clock usage.
 *
 * Return:	None.
 **/
static void clock_usage(void)
{
	printf("clock - clock test\n");
	printf("  -h    show this usage\n");
	printf("  -i t  interval time in milliseconds\n");
	printf("  -c n  number of the clock test cycles\n");
	printf("\nDefault settings:\n");
	printf("  interval:  %d\n", clk.interval);
	printf("  cycles:    %d\n", clk.cycles);
}

/*============================================================================
  EXPORTED FUNCTIONS
  ============================================================================*/
/**
 * main() - start function of the timer test.
 *
 * @argc:  argument counter.
 * @argv:  list of the arguments.
 *
 * Return:	0 or force a software trap.
 **/
int main(int argc, char *argv[])
{
	long exec_time, limit;
	int opt, t_id, i;

	/* Initialize test test valuses. */
	clk.interval = 100;
	clk.cycles   = 7;

	/* Analyze the clock arguments. */
	while ((opt = getopt(argc, argv, "c:hi:")) != -1) {
		/* Analyze the current argument. */
		switch(opt) {
		case 'c':
			clk.cycles = clock_atoi(optarg);
			break;
		case 'h':
			clock_usage();
			exit(0);
			break;
		case 'i':
			clk.interval = clock_atoi(optarg);
			break;
		default:
			clock_usage();
			exit(1);
			break;
		}
	}
	
	/* Initialize the operating system. */
	os_init(1);

	/* Create the interval timer. */
	t_id = os_clock_init("clk", clk.interval);

	/* Start the periodic timer. */
	os_clock_start(t_id);
	
	/* Print the first timer trace. */
	os_clock_trace(t_id, OS_CT_FIRST);

	/* Take the second value of the current time as random start value for the
	 * random number generator. */
	srand(time(NULL));

	/* Calculate the max. execution time with the intended violation of the
	 * adjusted clock. */
	limit = clk.interval + (clk.interval / 3);
	
	/* Test the periodic timer. */
	for(i = 0; i < clk.cycles; i++) {
		/* Simulate the current execute time. */
		exec_time = random() % limit;
		
		/* Sleep n milliseconds. */
		os_clock_msleep(exec_time);
		
		/* Wait for the timer expiration. */
		os_clock_barrier(t_id);
		
		/* Print the test cycle information. */
		os_clock_trace(t_id, OS_CT_MIDDLE);	
	}

	/* Stop the periodic timer. */
	os_clock_stop(t_id);
	
	/* Print the test cycle information. */
	os_clock_trace(t_id, OS_CT_LAST);
		
	/* Destroy the  interval timer. */
	os_clock_delete(t_id);
	
	/* Release the OS resources. */
	os_exit();

	return (0);
}
