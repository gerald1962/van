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
#include <stddef.h>  /* Standard type definitions: offsetof(). */

/*============================================================================
  EXPORTED INCLUDE REFERENCES
  ============================================================================*/
/*============================================================================
  LOCAL NAME CONSTANTS DEFINITIONS
  ============================================================================*/
/* Controller prompt. */
#define P "U>"

/* Maximum size of a status comment. */
#define KUNIT_STATUS_COMMENT_SIZE  256

/* Maximum size of parameter description string. */
#define KUNIT_PARAM_DESC_SIZE  128

/* TAP specifies subtest stream indentation of 4 spaces, 8 spaces for a
 * sub-subtest.  See the "Subtests" section in
 * https://node-tap.org/tap-protocol/ */
#define KUNIT_SUBTEST_INDENT  "    "

/*============================================================================
  MACROS
  ============================================================================*/
/* Are two types/vars the same type (ignoring qualifiers)? */
#define __same_type(a, b)  __builtin_types_compatible_p(typeof(a), typeof(b))

/*
  gerald@gerald:~/uml/linux-5.18-rc3$ find . -name '*.*'|xargs grep _Static_assert
  /uml/linux-5.18-rc3$ less ./scripts/genksyms/keywords.c
  
  // SPDX-License-Identifier: GPL-2.0-only
static struct resword {
        const char *name;
        int token;
} keywords[] = {
...
        // c11 keywords that can be used at module scope
        { "_Static_assert", STATIC_ASSERT_KEYW },

 */

/**
 * static_assert - check integer constant expression at build time
 *
 * static_assert() is a wrapper for the C11 _Static_assert, with a
 * little macro magic to make the message optional (defaulting to the
 * stringification of the tested expression).
 *
 * Contrary to BUILD_BUG_ON(), static_assert() can be used at global
 * scope, but requires the expression to be an integer constant
 * expression (i.e., it is not enough that __builtin_constant_p() is
 * true for expr).
 *
 * Also note that BUILD_BUG_ON() fails the build if the condition is
 * true, while static_assert() fails the build if the expression is
 * false.
 */
#define static_assert(expr, ...) __static_assert(expr, ##__VA_ARGS__, #expr)
#define __static_assert(expr, msg, ...) _Static_assert(expr, msg)

/**
 * container_of - cast a member of a structure out to the containing structure
 * @ptr:	the pointer to the member.
 * @type:	the type of the container struct this is embedded in.
 * @member:	the name of the member within the struct.
 *
 */
#define container_of(ptr, type, member) ({				\
	void *__mptr = (void *)(ptr);					\
	static_assert(__same_type(*(ptr), ((type *)0)->member) ||	\
		      __same_type(*(ptr), void),			\
		      "pointer type mismatch in container_of()");	\
	((type *)(__mptr - offsetof(type, member))); })

#define KUNIT_ASSERTION(test, assert_type, pass, assert_class, INITIALIZER, fmt, ...) do { \
       if (!(pass)) {	  					               \
		static const struct kunit_loc __loc = KUNIT_CURRENT_LOC;       \
		struct assert_class __assertion = INITIALIZER;		       \
		kunit_do_failed_assertion(test,				       \
					  &__loc,			       \
					  assert_type,			       \
					  &__assertion.assert,		       \
					  fmt,				       \
					  ##__VA_ARGS__);		       \
	}								       \
} while (0)

/*
 * A factory macro for defining the assertions and expectations for the basic
 * comparisons defined for the built in types.
 *
 * Unfortunately, there is no common type that all types can be promoted to for
 * which all the binary operators behave the same way as for the actual types
 * (for example, there is no type that long long and unsigned long long can
 * both be cast to where the comparison result is preserved for all values). So
 * the best we can do is do the comparison in the original types and then coerce
 * everything to long long for printing; this way, the comparison behaves
 * correctly and the printed out value usually makes sense without
 * interpretation, but can always be interpreted to figure out the actual
 * value.
 */
#define KUNIT_BASE_BINARY_ASSERTION(test,				       \
				    assert_class,			       \
				    format_func,			       \
				    assert_type,			       \
				    left,				       \
				    op,					       \
				    right,				       \
				    fmt,				       \
				    ...)				       \
do {									       \
	const typeof(left) __left = (left);				       \
	const typeof(right) __right = (right);				       \
	static const struct kunit_binary_assert_text __text = {		       \
		.operation = #op,					       \
		.left_text = #left,					       \
		.right_text = #right,					       \
	};								       \
									       \
	KUNIT_ASSERTION(test,						       \
			assert_type,					       \
			__left op __right,				       \
			assert_class,					       \
			KUNIT_INIT_BINARY_ASSERT_STRUCT(format_func,	       \
							&__text,	       \
							__left,		       \
							__right),	       \
			fmt,						       \
			##__VA_ARGS__);					       \
} while (0)

#define KUNIT_BINARY_INT_ASSERTION(test,				       \
				   assert_type,				       \
				   left,				       \
				   op,					       \
				   right,				       \
				   fmt,					       \
				    ...)				       \
	KUNIT_BASE_BINARY_ASSERTION(test,				       \
				    kunit_binary_assert,		       \
				    kunit_binary_assert_format,		       \
				    assert_type,			       \
				    left, op, right,			       \
				    fmt,				       \
				    ##__VA_ARGS__)

/**
 * KUNIT_EXPECT_EQ() - Sets an expectation that @left and @right are equal.
 * @test: The test context object.
 * @left: an arbitrary expression that evaluates to a primitive C type.
 * @right: an arbitrary expression that evaluates to a primitive C type.
 *
 * Sets an expectation that the values that @left and @right evaluate to are
 * equal. This is semantically equivalent to
 * KUNIT_EXPECT_TRUE(@test, (@left) == (@right)). See KUNIT_EXPECT_TRUE() for
 * more information.
 */
#define KUNIT_EXPECT_EQ(test, left, right) \
	KUNIT_EXPECT_EQ_MSG(test, left, right, NULL)

#define KUNIT_EXPECT_EQ_MSG(test, left, right, fmt, ...)		       \
	KUNIT_BINARY_INT_ASSERTION(test,				       \
				   KUNIT_EXPECTATION,			       \
				   left, ==, right,			       \
				   fmt,					       \
				    ##__VA_ARGS__)

/* printk and log to per-test or per-suite log buffer.  Logging only done
 * if CONFIG_KUNIT_DEBUGFS is 'y'; if it is 'n', no log is allocated/used.
 */
#if 0
#define kunit_log(test_or_suite, fmt, ...)				\
	do {								\
		printk(fmt, ##__VA_ARGS__);				\
		kunit_log_append((test_or_suite)->log,	fmt "\n",	\
				 ##__VA_ARGS__);			\
	} while (0)
#else
#define kunit_log(test_or_suite, fmt, ...)				\
	do {								\
		printf(fmt, ##__VA_ARGS__);				\
	} while (0)
#endif

/**
 * KUNIT_CASE - A helper for creating a &struct kunit_case
 *
 * @test_name: a reference to a test case function.
 *
 * Takes a symbol for a function representing a test case and creates a
 * &struct kunit_case object from it. See the documentation for
 * &struct kunit_case for an example on how to use it.
 */
#define KUNIT_CASE(test_name) { .run_case = test_name, .name = #test_name }

/*============================================================================
  LOCAL TYPE DEFINITIONS
  ============================================================================*/
struct va_format {
        const char *fmt;
        va_list *va;
};

struct list_head {
	struct list_head *next, *prev;
};

typedef spinlock_t  raw_spinlock_t;

struct swait_queue_head {
	raw_spinlock_t	  lock;
	struct list_head  task_list;
};

struct kunit;

struct string_stream {
	size_t length;
	struct list_head fragments;
	/* length and fragments are protected by this lock */
	spinlock_t lock;
	struct kunit *test;
};

/**
 * struct kunit_assert - Data for printing a failed assertion or expectation.
 * @format: a function which formats the data in this kunit_assert to a string.
 *
 * Represents a failed expectation/assertion. Contains all the data necessary to
 * format a string to a user reporting the failure.
 */
struct kunit_assert {
	void (*format)(const struct kunit_assert *assert,
		       const struct va_format *message,
		       struct string_stream *stream);
};

/**
 * struct kunit_binary_assert - An expectation/assertion that compares two
 *	non-pointer values (for example, KUNIT_EXPECT_EQ(test, 1 + 1, 2)).
 * @assert: The parent of this type.
 * @text: Holds the textual representations of the operands and op (e.g.  "==").
 * @left_value: The actual evaluated value of the expression in the left slot.
 * @right_value: The actual evaluated value of the expression in the right slot.
 *
 * Represents an expectation/assertion that compares two non-pointer values. For
 * example, to expect that 1 + 1 == 2, you can use the expectation
 * KUNIT_EXPECT_EQ(test, 1 + 1, 2);
 */
struct kunit_binary_assert {
	struct kunit_assert assert;
#if 0
	const struct kunit_binary_assert_text *text;
#endif
	long long left_value;
	long long right_value;
};

/**
 * enum kunit_assert_type - Type of expectation/assertion.
 * @KUNIT_ASSERTION: Used to denote that a kunit_assert represents an assertion.
 * @KUNIT_EXPECTATION: Denotes that a kunit_assert represents an expectation.
 *
 * Used in conjunction with a &struct kunit_assert to denote whether it
 * represents an expectation or an assertion.
 */
enum kunit_assert_type {
	KUNIT_ASSERTION,
	KUNIT_EXPECTATION,
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

/**
 * enum kunit_status - Type of result for a test or test suite
 * @KUNIT_SUCCESS: Denotes the test suite has not failed nor been skipped
 * @KUNIT_FAILURE: Denotes the test has failed.
 * @KUNIT_SKIPPED: Denotes the test has been skipped.
 */
enum kunit_status {
	KUNIT_SUCCESS,
	KUNIT_FAILURE,
	KUNIT_SKIPPED,
};

typedef void (*kunit_try_catch_func_t)(void *);

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
	kunit_try_catch_func_t try;
	kunit_try_catch_func_t catch;
	void *context;
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
};

/**
 * struct kunit_case - represents an individual test case.
 *
 * @run_case: the function representing the actual test case.
 * @name:     the name of the test case.
 * @generate_params: the generator function for parameterized tests.
 *
 * A test case is a function with the signature,
 * ``void (*)(struct kunit *)``
 * that makes expectations and assertions (see KUNIT_EXPECT_TRUE() and
 * KUNIT_ASSERT_TRUE()) about code under test. Each test case is associated
 * with a &struct kunit_suite and will be run after the suite's init
 * function and followed by the suite's exit function.
 *
 * A test case should be static and should only be created with the
 * KUNIT_CASE() macro; additionally, every array of test cases should be
 * terminated with an empty test case.
 *
 * Example:
 *
 * .. code-block:: c
 *
 *	void add_test_basic(struct kunit *test)
 *	{
 *		KUNIT_EXPECT_EQ(test, 1, add(1, 0));
 *		KUNIT_EXPECT_EQ(test, 2, add(1, 1));
 *		KUNIT_EXPECT_EQ(test, 0, add(-1, 1));
 *		KUNIT_EXPECT_EQ(test, INT_MAX, add(0, INT_MAX));
 *		KUNIT_EXPECT_EQ(test, -1, add(INT_MAX, INT_MIN));
 *	}
 *
 *	static struct kunit_case example_test_cases[] = {
 *		KUNIT_CASE(add_test_basic),
 *		{}
 *	};
 *
 */
struct kunit_case {
	void (*run_case)(struct kunit *test);
	const char *name;
	const void* (*generate_params)(const void *prev, char *desc);

	/* private: internal use only. */
	enum kunit_status status;
	char *log;
};

/**
 * struct kunit_suite - describes a related collection of &struct kunit_case
 *
 * @name:	the name of the test. Purely informational.
 * @init:	called before every test case.
 * @exit:	called after every test case.
 * @test_cases:	a null terminated array of test cases.
 *
 * A kunit_suite is a collection of related &struct kunit_case s, such that
 * @init is called before every test case and @exit is called after every
 * test case, similar to the notion of a *test fixture* or a *test class*
 * in other unit testing frameworks like JUnit or Googletest.
 *
 * Every &struct kunit_case must be associated with a kunit_suite for KUnit
 * to run it.
 */
struct kunit_suite {
	const char name[256];
	int (*init)(struct kunit *test);
	void (*exit)(struct kunit *test);
	struct kunit_case *test_cases;

	/* private: internal use only */
	char status_comment[KUNIT_STATUS_COMMENT_SIZE];
#if 0
	struct dentry *debugfs;
	char *log;
#endif
};

struct kunit_result_stats {
	unsigned long passed;
	unsigned long skipped;
	unsigned long failed;
	unsigned long total;
};

/*============================================================================
  LOCAL FUNCTION PROTOTYPES
  ============================================================================*/
static void vu_test(struct kunit *test);

/*============================================================================
  LOCAL DATA
  ============================================================================*/
/* A test case should be static and should only be created with the KUNIT_CASE()
 * macro; additionally, every array of test cases should be terminated with an
 * empty test case. */
static struct kunit_case vu_test_cases[] = {
        KUNIT_CASE(vu_test),
        {},
};

/* Define a related collection of struct kunit_case. */
static struct kunit_suite vu_test_suite = {
        .name = "vu_test",
        .test_cases = vu_test_cases,
};

/*============================================================================
  LOCAL FUNCTIONS
  ============================================================================*/
static void kunit_binary_assert_format(const struct kunit_assert *assert,
				       const struct va_format *message,
				       struct string_stream *stream)
{
	struct kunit_binary_assert *binary_assert;

	binary_assert = container_of(assert, struct kunit_binary_assert,
				     assert);
#if 0
	string_stream_add(stream,
			  KUNIT_SUBTEST_INDENT "Expected %s %s %s, but\n",
			  binary_assert->text->left_text,
			  binary_assert->text->operation,
			  binary_assert->text->right_text);
	if (!is_literal(stream->test, binary_assert->text->left_text,
			binary_assert->left_value, stream->gfp))
		string_stream_add(stream, KUNIT_SUBSUBTEST_INDENT "%s == %lld\n",
				  binary_assert->text->left_text,
				  binary_assert->left_value);
	if (!is_literal(stream->test, binary_assert->text->right_text,
			binary_assert->right_value, stream->gfp))
		string_stream_add(stream, KUNIT_SUBSUBTEST_INDENT "%s == %lld",
				  binary_assert->text->right_text,
				  binary_assert->right_value);
	kunit_assert_print_msg(message, stream);
#endif
}

static void kunit_print_subtest_start(struct kunit_suite *suite)
{
	kunit_log(suite, KUNIT_SUBTEST_INDENT "# Subtest: %s",
		  suite->name);
#if 0
	kunit_log(KERN_INFO, suite, KUNIT_SUBTEST_INDENT "1..%zd",
		  kunit_suite_num_test_cases(suite));
#endif
}

static int kunit_run_tests(struct kunit_suite *suite)
{
	char param_desc[KUNIT_PARAM_DESC_SIZE];
	struct kunit_case *test_case;
	struct kunit_result_stats suite_stats = { 0 };
	struct kunit_result_stats total_stats = { 0 };

	kunit_print_subtest_start(suite);

#if 0
	kunit_suite_for_each_test_case(suite, test_case) {
		struct kunit test = { .param_value = NULL, .param_index = 0 };
		struct kunit_result_stats param_stats = { 0 };
		test_case->status = KUNIT_SKIPPED;

		if (!test_case->generate_params) {
			/* Non-parameterised test. */
			kunit_run_case_catch_errors(suite, test_case, &test);
			kunit_update_stats(&param_stats, test.status);
		} else {
			/* Get initial param. */
			param_desc[0] = '\0';
			test.param_value = test_case->generate_params(NULL, param_desc);
			kunit_log(KERN_INFO, &test, KUNIT_SUBTEST_INDENT KUNIT_SUBTEST_INDENT
				  "# Subtest: %s", test_case->name);

			while (test.param_value) {
				kunit_run_case_catch_errors(suite, test_case, &test);

				if (param_desc[0] == '\0') {
					snprintf(param_desc, sizeof(param_desc),
						 "param-%d", test.param_index);
				}

				kunit_log(KERN_INFO, &test,
					  KUNIT_SUBTEST_INDENT KUNIT_SUBTEST_INDENT
					  "%s %d - %s",
					  kunit_status_to_ok_not_ok(test.status),
					  test.param_index + 1, param_desc);

				/* Get next param. */
				param_desc[0] = '\0';
				test.param_value = test_case->generate_params(test.param_value, param_desc);
				test.param_index++;

				kunit_update_stats(&param_stats, test.status);
			}
		}


		kunit_print_test_stats(&test, param_stats);

		kunit_print_ok_not_ok(&test, true, test_case->status,
				      kunit_test_case_num(suite, test_case),
				      test_case->name,
				      test.status_comment);

		kunit_update_stats(&suite_stats, test_case->status);
		kunit_accumulate_stats(&total_stats, param_stats);
	}

	kunit_print_suite_stats(suite, suite_stats, total_stats);
	kunit_print_subtest_end(suite);
#endif
	return 0;
}

static void vu_test(struct kunit *test)
{
	// KUNIT_EXPECT_EQ(test, 1, vu_add(1, 0));
}

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
	
	/* Execute a single test set directly. */
	kunit_run_tests(&vu_test_suite);
	
	return (0);
}
