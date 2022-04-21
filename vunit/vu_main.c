// SPDX-License-Identifier: GPL-2.0

/*
 * van unit test system.
 *
 * Copyright (C) 2022 Gerald Schueller <gerald.schueller@web.de>
 */

/*============================================================================
  IMPORTED INCLUDE REFERENCES
  ============================================================================*/
#include "os.h"      /* Van Operating system: printf(). */

/*============================================================================
  EXPORTED INCLUDE REFERENCES
  ============================================================================*/
/*============================================================================
  LOCAL NAME CONSTANTS DEFINITIONS
  ============================================================================*/
/* Controller prompt. */
#define P "U>"

/*============================================================================
  MACROS
  ============================================================================*/
/*============================================================================
  LOCAL TYPE DEFINITIONS
  ============================================================================*/
struct list_head {
	struct list_head *next, *prev;
};

struct swait_queue_head {
	// raw_spinlock_t		lock;
	struct list_head	task_list;
};

/*
 * struct completion - structure used to maintain state for a "completion"
 *
 * This is the opaque structure used to maintain the state for a "completion".
 * Completions currently use a FIFO to queue threads that have to wait for
 * the "completion" event.
 *
 * See also:  complete(), wait_for_completion() (and friends _timeout,
 * _interruptible, _interruptible_timeout, and _killable), init_completion(),
 * reinit_completion(), and macros DECLARE_COMPLETION(),
 * DECLARE_COMPLETION_ONSTACK().
 */
struct completion {
	unsigned int done;
	struct swait_queue_head wait;
};

struct kunit;

/**
 * struct kunit_try_catch - provides a generic way to run code which might fail.
 * @test: The test case that is currently being executed.
 * @try_completion: Completion that the control thread waits on while test runs.
 * @try_result: Contains any errno obtained while running test case.
 * @try: The function, the test case, to attempt to run.
 * @catch: The function called if @try bails out.
 * @context: used to pass user data to the try and catch functions.
 *
 * kunit_try_catch provides a generic, architecture independent way to execute
 * an arbitrary function of type kunit_try_catch_func_t which may bail out by
 * calling kunit_try_catch_throw(). If kunit_try_catch_throw() is called, @try
 * is stopped at the site of invocation and @catch is called.
 *
 * struct kunit_try_catch provides a generic interface for the functionality
 * needed to implement kunit->abort() which in turn is needed for implementing
 * assertions. Assertions allow stating a precondition for a test simplifying
 * how test cases are written and presented.
 *
 * Assertions are like expectations, except they abort (call
 * kunit_try_catch_throw()) when the specified condition is not met. This is
 * useful when you look at a test case as a logical statement about some piece
 * of code, where assertions are the premises for the test case, and the
 * conclusion is a set of predicates, rather expectations, that must all be
 * true. If your premises are violated, it does not makes sense to continue.
 */
struct kunit_try_catch {
	/* private: internal use only. */
	struct kunit *test;
	struct completion *try_completion;
	int try_result;
#if 0
	kunit_try_catch_func_t try;
	kunit_try_catch_func_t catch;
	void *context;
#endif
};

/**
 * struct kunit - represents a running instance of a test.
 *
 * @priv: for user to store arbitrary data. Commonly used to pass data
 *	  created in the init function (see &struct kunit_suite).
 *
 * Used to store information about the current context under which the test
 * is running. Most of this data is private and should only be accessed
 * indirectly via public functions; the one exception is @priv which can be
 * used by the test writer to store arbitrary data.
 */
struct kunit {
	void *priv;

	/* private: internal use only. */
	const char *name; /* Read only after initialization! */
	char *log; /* Points at case log after initialization */

	struct kunit_try_catch try_catch;
#if 0
	/* param_value is the current parameter value for a test case. */
	const void *param_value;
	/* param_index stores the index of the parameter in parameterized tests. */
	int param_index;
	/*
	 * success starts as true, and may only be set to false during a
	 * test case; thus, it is safe to update this across multiple
	 * threads using WRITE_ONCE; however, as a consequence, it may only
	 * be read after the test case finishes once all threads associated
	 * with the test case have terminated.
	 */
	spinlock_t lock; /* Guards all mutable test state. */
	enum kunit_status status; /* Read only after test_case finishes! */
	/*
	 * Because resources is a list that may be updated multiple times (with
	 * new resources) from any thread associated with a test case, we must
	 * protect it with some type of lock.
	 */
	struct list_head resources; /* Protected by lock. */

	char status_comment[KUNIT_STATUS_COMMENT_SIZE];
#endif
};

/*============================================================================
  LOCAL FUNCTION PROTOTYPES
  ============================================================================*/
/*============================================================================
  LOCAL DATA
  ============================================================================*/
/*============================================================================
  EXPORTED FUNCTIONS
  ============================================================================*/
/**
 * main() - start function of the van unit test system.
 *
 * @argc:  argument counter.
 * @argv:  list of the arguments.
 *
 * Return:	0 or force a software trap.
 **/
int main(int argc, char *argv[])
{
	printf("%s VUnit test system.\n", P);
	return (0);
}
