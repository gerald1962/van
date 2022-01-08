// SPDX-License-Identifier: GPL-2.0

/*
 * Operating system interfaces.
 *
 * Copyright (C) 2022 Gerald Schueller <gerald.schueller@web.de>
 */

/*============================================================================
  IMPORTED INCLUDE REFERENCES
  ============================================================================*/
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
#if 0
#include <stdio.h>  /* for puts() */
#include <string.h> /* for memset() */
#include <unistd.h> /* for sleep() */
#include <stdlib.h> /* for EXIT_SUCCESS */

#include <signal.h> /* for `struct sigevent` and SIGEV_THREAD */
#include <time.h>   /* for timer_create(), `struct itimerspec`,
                     * timer_t and CLOCK_REALTIME 
                     */

void thread_handler(union sigval sv) {
        char *s = sv.sival_ptr;

        /* Will print "5 seconds elapsed." */
        puts(s);
}

int main(void) {
        char info[] = "5 seconds elapsed.";
        timer_t timerid;
        struct sigevent sev;
        struct itimerspec trigger;

        /* Set all `sev` and `trigger` memory to 0 */
        memset(&sev, 0, sizeof(struct sigevent));
        memset(&trigger, 0, sizeof(struct itimerspec));

        /* 
         * Set the notification method as SIGEV_THREAD:
         *
         * Upon timer expiration, `sigev_notify_function` (thread_handler()),
         * will be invoked as if it were the start function of a new thread.
         *
         */
        sev.sigev_notify = SIGEV_THREAD;
        sev.sigev_notify_function = &thread_handler;
        sev.sigev_value.sival_ptr = &info;

        /* Create the timer. In this example, CLOCK_REALTIME is used as the
         * clock, meaning that we're using a system-wide real-time clock for
         * this timer.
         */
        timer_create(CLOCK_REALTIME, &sev, &timerid);

        /* Timer expiration will occur withing 5 seconds after being armed
         * by timer_settime().
         */
        trigger.it_value.tv_sec = 5;

        /* Arm the timer. No flags are set and no old_value will be retrieved.
         */
        timer_settime(timerid, 0, &trigger, NULL);

        /* Wait 10 seconds under the main thread. In 5 seconds (when the
         * timer expires), a message will be printed to the standard output
         * by the newly created notification thread.
         */
        sleep(10);

        /* Delete (destroy) the timer */
        timer_delete(timerid);

        return EXIT_SUCCESS;
}
#endif

#if 0
Creating a Periodic Timer
You can use a periodic timer to provide repeated signals to your process.

The following example illustrates a periodic timer with a delay of a second and a repeating interval of ten milliseconds. It also configures a thread function as the timer expiry notification using SIGEV_THREAD.

The following example code performs the following tasks:

Creates a notification function (thread function) that must be invoked after timer expiry.

Sets the thread priority to 255 using the thread scheduling parameters (struct sched_param). This ensures that the thread function has the highest priority when it is invoked as a result of a timer expiry.

Creates a timer based on the current system time (CLOCK_REALTIME) and a notification function (struct sigevent sig) that must be invoked when the timer expires.

Defines the input values for timer_settime(). The key input values are the timer value (in.it_value.tv_sec = 1;) and the interval (in.it_interval.tv_nsec = 100000000;). The periodic timer will expire after a second and then invoke the notification function every one-tenth of a second until it is destroyed.

Starts the periodic timer using timer_settime().

Uses sleep(2) to pause the program execution for two seconds before destroying the timer.

#include <time.h>
#include <stdio.h>
#include <signal.h>
#include <pthread.h>
#include <unistd.h>
#include <errno.h>
static int i = 0;

//Thread function to be invoked when the periodic timer expires
void sighler (union sigval val)
    {
    printf("Handler entered with value :%d for %d times\n", val.sival_int, ++i);
    }
int main()
    {
    int Ret;

    pthread_attr_t attr;
    pthread_attr_init( &attr );

    struct sched_param parm;
    parm.sched_priority = 255;
    pthread_attr_setschedparam(&attr, &parm);

    struct sigevent sig;
    sig.sigev_notify = SIGEV_THREAD;
    sig.sigev_notify_function = sighler;
    sig.sigev_value.sival_int =20;
    sig.sigev_notify_attributes = &attr;

    //create a new timer.
    timer_t timerid;
    Ret = timer_create(CLOCK_REALTIME, &sig, &timerid);
    if (Ret == 0)
        {
        struct itimerspec in, out;
        in.it_value.tv_sec = 1;
        in.it_value.tv_nsec = 0;
        in.it_interval.tv_sec = 0;
        in.it_interval.tv_nsec = 100000000;
        //issue the periodic timer request here.
        Ret = timer_settime(timerid, 0, &in, &out);
        if(Ret == 0)
            sleep(2);
        else
            printf("timer_settime() failed with %d\n", errno);
        //delete the timer.
        timer_delete(timerid);
        }
    else
    printf("timer_create() failed with %d\n", errno);
    return Ret;
    }
#endif

/*============================================================================
  EXPORTED FUNCTIONS
  ============================================================================*/

#if 0
/**
 * os_timer_barrier() - ordering timer constraint on controller operations 
 * before and after the barrier instructions as agreed in os_timer_init().
 *
 * @id:       assigned timer id by os_timer_init().
 * @overrun:  overrun indication for the last timer expiration.
 *
 * Return:	the elapsed timer between subsequent invocations.
 **/
int os_timer_barrier(int id, int *overrun);

/**
 * os_timer_stop() - stop a periodic timer.
 *
 * @id:  assigned timer id by os_timer_init().
 *
 * Return:	None.
 **/
void os_timer_stop(int id);

/**
 * os_timer_start() - start a periodic timer.
 *
 * @id:  assigned timer id by os_timer_init().
 *
 * Return:	None.
 **/
void os_timer_start(int id);

/**
 * os_timer_delete() - remove a periodic timer.
 *
 * @id:  assigned timer id by os_timer_init().
 *
 * Return:	None.
 **/
void os_timer_delete(int id);

/**
 * os_timer_init() - create a periodic timer with a repeating interval of i
 * milliseconds.
 *
 * @name:      timer name.
 * @interval:  repeating interval in milliseconds.
 *
 * Return:	the timer id.
 **/
int os_timer_init(char *name, int interval);

#endif
