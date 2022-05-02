// SPDX-License-Identifier: GPL-2.0

/* van unit test system.
 *
 * Copyright (C) 2022 Gerald Schueller <gerald.schueller@web.de> */

/*============================================================================
  IMPORTED INCLUDE REFERENCES
  ============================================================================*/
#include "os.h"       /* Van Operating system: printf(). */
#include <stddef.h>   /* Standard type definitions: offsetof(). */
#include <stdbool.h>  /* C99 allows bool. true is 1 and false is 0. */
#include <errno.h>    /* OS interface errors like ENOMEM=12 or EFAULT=14.  */
#include <limits.h>   /* Sizes of integral or integer types: INT_MIN. */

/*============================================================================
  EXPORTED INCLUDE REFERENCES
  ============================================================================*/
/*============================================================================
  LOCAL NAME CONSTANTS DEFINITIONS
  ============================================================================*/
/* Controller prompt. */
#define P "U>"

/* Architectures might want to move the poison pointer offset
 * into some well-recognized area such as 0xdead000000000000,
 * that is also not mappable by user-space exploits: */
#define POISON_POINTER_DELTA 0

/* These are non-NULL pointers that will result in page faults
 * under normal circumstances, used to verify that nobody uses
 * non-initialized list entries. */
#define LIST_POISON1  ((void *) 0x100 + POISON_POINTER_DELTA)
#define LIST_POISON2  ((void *) 0x200 + POISON_POINTER_DELTA)

/*   gcc: https://gcc.gnu.org/onlinedocs/gcc/Common-Function-Attributes.html#index-noreturn-function-attribute
 * clang: https://clang.llvm.org/docs/AttributeReference.html#noreturn
 * clang: https://clang.llvm.org/docs/AttributeReference.html#id1

 * The noreturn function specifier is used to tell to the compiler that the
 * function will not return anything. If the program uses some return statement
 * inside it, the compiler will generate compile time error. */
#define __noreturn  __attribute__((__noreturn__))

/*   gcc: https://gcc.gnu.org/onlinedocs/gcc/Common-Function-Attributes.html#index-warn_005funused_005fresult-function-attribute
 * clang: https://clang.llvm.org/docs/AttributeReference.html#nodiscard-warn-unused-result */
#define __must_check  __attribute__((__warn_unused_result__))

#define REFCOUNT_SATURATED  (INT_MIN / 2)

/* Size of log associated with test. */
#define KUNIT_LOG_SIZE	512

/* Maximum size of a status comment. */
#define KUNIT_STATUS_COMMENT_SIZE  256

/* Maximum size of parameter description string. */
#define KUNIT_PARAM_DESC_SIZE  128

/* TAP specifies subtest stream indentation of 4 spaces, 8 spaces for a
 * sub-subtest.  See the "Subtests" section in
 * https://node-tap.org/tap-protocol/ */
#define KUNIT_SUBTEST_INDENT     "    "
#define KUNIT_SUBSUBTEST_INDENT  "        "

#define KUNIT_CURRENT_LOC { .file = __FILE__, .line = __LINE__ }

/*============================================================================
  MACROS
  ============================================================================*/
/*
 * Some subsystems have their own custom printk that applies a va_format to a
 * generic format, for example, to include a device number or other metadata
 * alongside the format supplied by the caller.
 *
 * In order to store these in the way they would be emitted by the printk
 * infrastructure, the subsystem provides us with the start, fixed string, and
 * any subsequent text in the format string.
 *
 * We take a variable argument list as pr_fmt/dev_fmt/etc are sometimes passed
 * as multiple arguments (eg: `"%s: ", "blah"`), and we must only take the
 * first one.
 *
 * subsys_fmt_prefix must be known at compile time, or compilation will fail
 * (since this is a mistake). If fmt or level is not known at compile time, no
 * index entry will be made (since this can legitimately happen).
 */
#define __printk_index_emit(...) do {} while (0)

#define printk_index_wrap(_p_func, _fmt, ...)				\
	({								\
		__printk_index_emit(_fmt, NULL, NULL);			\
		_p_func(_fmt, ##__VA_ARGS__);				\
	})


/**
 * printk - print a kernel message
 * @fmt: format string
 *
 * This is printk(). It can be called from any context. We want it to work.
 *
 * If printk indexing is enabled, _printk() is called from printk_index_wrap.
 * Otherwise, printk is simply #defined to _printk.
 *
 * We try to grab the console_lock. If we succeed, it's easy - we log the
 * output and call the console drivers.  If we fail to get the semaphore, we
 * place the output into the log buffer and return. The current holder of
 * the console_sem will notice the new output in console_unlock(); and will
 * send it to the consoles before releasing the lock.
 *
 * One effect of this deferred printing is that code which calls printk() and
 * then changes console_loglevel may break. This is because console_loglevel
 * is inspected when the actual printing occurs.
 *
 * See also:
 * printf(3)
 *
 * See the vsnprintf() documentation for format string extensions over C99.
 */
#define printk(fmt, ...) printk_index_wrap(_printk, fmt, ##__VA_ARGS__)

/**
 * pr_info - Print an info-level message
 * @fmt: format string
 * @...: arguments for the format string
 *
 * This macro expands to a printk with KERN_INFO loglevel. It uses pr_fmt() to
 * generate the format string.
 */
#define pr_fmt(fmt) "VUnit: " fmt

#define pr_info(fmt, ...) \
	printk(pr_fmt(fmt), ##__VA_ARGS__)

#define __WARN_printf(arg...)	do { fprintf(stderr, arg); } while (0)

#define WARN_ON(condition) ({					\
	int __ret_warn_on = !! (condition);			\
	if (unlikely(__ret_warn_on))				\
		__WARN_printf("assertion failed at %s:%d\n",	\
				__FILE__, __LINE__);		\
	unlikely(__ret_warn_on);				\
})

#define BUG_ON(cond)  OS_TRAP_IF(! (cond))
#define BUG()	      BUG_ON(1)

/* Are two types/vars the same type (ignoring qualifiers)? */
#define __same_type(a, b)  __builtin_types_compatible_p(typeof(a), typeof(b))

#define check_mul_overflow(a, b, d) __must_check_overflow(({	\
	typeof(a) __a = (a);			\
	typeof(b) __b = (b);			\
	typeof(d) __d = (d);			\
	(void) (&__a == &__b);			\
	(void) (&__a == __d);			\
	__builtin_mul_overflow(__a, __b, __d);	\
}))

/* The "volatile" is due to gcc bugs: see
 * https://gcc.gnu.org/onlinedocs/gcc/Extended-Asm.html
 *
 * With extended asm you can read and write C variables from assembler and
 * perform jumps from assembler code to C labels. Extended asm syntax uses
 * colons (:) to delimit the operand parameters after the assembler template.
 * See:
 * https://gcc.gnu.org/onlinedocs/gcc/Using-Assembly-Language-with-C.html
 *
 * In it find recommondattions:
 * https://stackoverflow.com/questions/19965076/gcc-memory-barrier-sync-synchronize-vs-asm-volatile-memory
 *
 * The first barriers does nothing at runtime. It's called a SW barrier.
 * The second barrier would translate into a HW barrier, probably a fence
 * (mfence/sfence) operations if you're on x86.
 * This instruction tells the micro code of a CPU network to make sure that
 * loads or stores can't pass this point and must be observed in the correct
 * side of the sync point. */
#define barrier()  __asm__ __volatile__("": : :"memory")

#define WARN(condition, format...) ({					\
	int __ret_warn_on = !! (condition);				\
	if (unlikely(__ret_warn_on))					\
		printf(format);			                        \
	unlikely(__ret_warn_on);					\
})

#define WARN_ONCE(condition, format...)  WARN(condition, format)

/* Prevent the compiler from merging or refetching reads or writes. The
 * compiler is also forbidden from reordering successive instances of
 * READ_ONCE and WRITE_ONCE, but only when the compiler is aware of some
 * particular ordering. One way to make the compiler aware of ordering is to
 * put the two invocations of READ_ONCE or WRITE_ONCE in different C
 * statements.
 *
 * These two macros will also work on aggregate data types like structs or
 * unions. If the size of the accessed data type exceeds the word size of
 * the machine (e.g., 32 bits or 64 bits) READ_ONCE() and WRITE_ONCE() will
 * fall back to memcpy and print a compile-time warning.
 *
 * Their two major use cases are: (1) Mediating communication between
 * process-level code and irq/NMI handlers, all running on the same CPU,
 * and (2) Ensuring that the compiler does not fold, spindle, or otherwise
 * mutilate accesses that either do not require ordering or that interact
 * with an explicit memory barrier or atomic instruction that provides the
 * required ordering. */
#define READ_ONCE(x)					\
({							\
	union { typeof(x) __val; char __c[1]; } __u =	\
		{ .__c = { 0 } };			\
	__read_once_size(&(x), __u.__c, sizeof(x));	\
	__u.__val;					\
})

#define WRITE_ONCE(x, val)				\
({							\
	union { typeof(x) __val; char __c[1]; } __u =	\
		{ .__val = (val) }; 			\
	__write_once_size(&(x), __u.__c, sizeof(x));	\
	__u.__val;					\
})

#define REFCOUNT_WARN(str)  WARN_ONCE(1, "refcount_t: " str ".\n")

/*
 * see https://gcc.gnu.org/onlinedocs/gcc/Other-Builtins.html
 *
 * Built-in Function: long __builtin_expect (long exp, long c)
 * You may use __builtin_expect to provide the compiler with branch prediction
 * information. In general, you should prefer to use actual profile feedback for
 * this (-fprofile-arcs), as programmers are notoriously bad at predicting how
 * their programs actually perform. However, there are applications in which
 * this data is hard to collect.
 *
 * The return value is the value of exp, which should be an integral expression.
 * The semantics of the built-in are that it is expected that exp == c. For
 * example:
 *
 * if (__builtin_expect (x, 0))
 *         foo ();
 * indicates that we do not expect to call foo, since we expect x to be zero.
 * Since you are limited to integral expressions for exp, you should use
 * constructions such as
 *
 * if (__builtin_expect (ptr != NULL, 1))
 *         foo (*ptr);
 * when testing pointer or floating-point values.
 *
 * For the purposes of branch prediction optimizations, the probability that a
 * --builtinexpect expression is true is controlled by GCCs
 * builtin-expect-probability parameter, which defaults to 90%.
 *
 * You can also use --builtin-expect-with-probability to explicitly assign a
 * probability value to individual expressions. If the built-in is used in a
 * loop construct, the provided probability will influence the expected number
 * of iterations made by loop optimizations. */

/* The attributes likely and unlikely may be applied to labels and statements
 * (other than declaration-statements). They may not be simultaneously applied
 * to the same label or statement.
 *
 * 1) Applies to a statement to allow the compiler to optimize for the case
 * where paths of execution including that statement are more likely than any
 * alternative path of execution that does not include such a statement.
 * 2) Applies to a statement to allow the compiler to optimize for the case
 * where paths of execution including that statement are less likely than any
 *  alternative path of execution that does not include such a statement. */
#define unlikely(x)	__builtin_expect(!! (x), 0)

/**
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

/**
 * list_entry - get the struct for this entry
 * @ptr:	the &struct list_head pointer.
 * @type:	the type of the struct this is embedded in.
 * @member:	the name of the list_head within the struct.
 */
#define list_entry(ptr, type, member) \
	container_of(ptr, type, member)

/**
 * list_prev_entry - get the prev element in list
 * @pos:	the type * to cursor
 * @member:	the name of the list_head within the struct.
 */
#define list_prev_entry(pos, member) \
	list_entry((pos)->member.prev, typeof(*(pos)), member)


/**
 * list_next_entry - get the next element in list
 * @pos:	the type * to cursor
 * @member:	the name of the list_head within the struct.
 */
#define list_next_entry(pos, member) \
	list_entry((pos)->member.next, typeof(*(pos)), member)

/**
 * list_last_entry - get the last element from a list
 * @ptr:	the list head to take the element from.
 * @type:	the type of the struct this is embedded in.
 * @member:	the name of the list_head within the struct.
 *
 * Note, that list is expected to be not empty.
 */
#define list_last_entry(ptr, type, member) \
	list_entry((ptr)->prev, type, member)

/**
 * list_first_entry - get the first element from a list
 * @ptr:	the list head to take the element from.
 * @type:	the type of the struct this is embedded in.
 * @member:	the name of the list_head within the struct.
 *
 * Note, that list is expected to be not empty.
 */
#define list_first_entry(ptr, type, member) \
	list_entry((ptr)->next, type, member)

/**
 * list_entry_is_head - test if the entry points to the head of the list
 * @pos:	the type * to cursor
 * @head:	the head for your list.
 * @member:	the name of the list_head within the struct.
 */
#define list_entry_is_head(pos, head, member)				\
	(&pos->member == (head))

/**
 * list_for_each_entry	-	iterate over list of given type
 * @pos:	the type * to use as a loop cursor.
 * @head:	the head for your list.
 * @member:	the name of the list_head within the struct.
 */
#define list_for_each_entry(pos, head, member)				\
	for (pos = list_first_entry(head, typeof(*pos), member);	\
	     ! list_entry_is_head(pos, head, member);			\
	     pos = list_next_entry(pos, member))

/**
 * list_for_each_entry_reverse - iterate backwards over list of given type.
 * @pos:	the type * to use as a loop cursor.
 * @head:	the head for your list.
 * @member:	the name of the list_head within the struct.
 */
#define list_for_each_entry_reverse(pos, head, member)			\
	for (pos = list_last_entry(head, typeof(*pos), member);		\
	     ! list_entry_is_head(pos, head, member); 			\
	     pos = list_prev_entry(pos, member))

/**
 * list_for_each_entry_safe - iterate over list of given type safe against removal of list entry
 * @pos:	the type * to use as a loop cursor.
 * @n:		another type * to use as temporary storage
 * @head:	the head for your list.
 * @member:	the name of the list_head within the struct.
 */
#define list_for_each_entry_safe(pos, n, head, member)			\
	for (pos = list_first_entry(head, typeof(*pos), member),	\
		n = list_next_entry(pos, member);			\
	     ! list_entry_is_head(pos, head, member); 			\
	     pos = n, n = list_next_entry(n, member))


#define kunit_suite_for_each_test_case(suite, test_case)		\
	for (test_case = suite->test_cases; test_case->run_case; test_case++)

/**
 * KUNIT_INIT_BINARY_ASSERT_STRUCT() - Initializes a binary assert like
 *	kunit_binary_assert, kunit_binary_ptr_assert, etc.
 *
 * @format_func: a function which formats the assert to a string.
 * @text_: Pointer to a kunit_binary_assert_text.
 * @left_val: The actual evaluated value of the expression in the left slot.
 * @right_val: The actual evaluated value of the expression in the right slot.
 *
 * Initializes a binary assert like kunit_binary_assert,
 * kunit_binary_ptr_assert, etc. This relies on these structs having the same
 * fields but with different types for left_val/right_val.
 * This is ultimately used by binary assertion macros like KUNIT_EXPECT_EQ, etc.
 */
#define KUNIT_INIT_BINARY_ASSERT_STRUCT(format_func,			       \
					text_,				       \
					left_val,			       \
					right_val) {			       \
	.assert = { .format = format_func },				       \
	.text = text_,							       \
	.left_value = left_val,						       \
	.right_value = right_val					       \
}

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
#define kunit_log(test_or_suite, fmt, ...)				\
	do {								\
		printf(fmt "\n", ##__VA_ARGS__);				\
		kunit_log_append((test_or_suite)->log,	fmt "\n",	\
				 ##__VA_ARGS__);			\
	} while (0)

#define kunit_printk(test, fmt, ...)				\
	kunit_log(test, KUNIT_SUBTEST_INDENT "# %s: " fmt,		\
		  (test)->name,	##__VA_ARGS__)

/**
 * kunit_err() - Prints an ERROR level message associated with @test.
 *
 * @test: The test context object.
 * @fmt:  A printk() style format string.
 *
 * Prints an error level message.
 */
#define kunit_err(test, fmt, ...) \
	kunit_printk(test, fmt, ##__VA_ARGS__)

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
typedef unsigned char       __u8;
typedef unsigned short      __u16;
typedef unsigned int        __u32;
typedef unsigned long long  __u64;

/* Following functions are taken from kernel sources and
 * break aliasing rules in their original form.
 *
 * While kernel is compiled with -fno-strict-aliasing,
 * perf uses -Wstrict-aliasing=3 which makes build fail
 * under gcc 4.4.
 *
 * Using extra __may_alias__ type to allow aliasing
 * in this case. */
typedef __u8  __attribute__((__may_alias__))  __u8_alias_t;
typedef __u16 __attribute__((__may_alias__)) __u16_alias_t;
typedef __u32 __attribute__((__may_alias__)) __u32_alias_t;
typedef __u64 __attribute__((__may_alias__)) __u64_alias_t;

enum refcount_saturation_type {
	REFCOUNT_ADD_NOT_ZERO_OVF,
	REFCOUNT_ADD_OVF,
	REFCOUNT_ADD_UAF,
	REFCOUNT_SUB_UAF,
	REFCOUNT_DEC_LEAK,
};

/** typedef refcount_t - variant of atomic_t specialized for reference counts
 * @refs: atomic_t counter field
 *
 * The counter saturates at REFCOUNT_SATURATED and will not move once
 * there. This avoids wrapping the counter and causing 'spurious'
 * use-after-free bugs.
 */
typedef struct refcount_struct {
	atomic_int  refs;
} refcount_t;

struct kref {
	refcount_t refcount;
};

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

struct kunit_kmalloc_array_params {
	size_t n;
	size_t size;
};

struct kunit;
struct kunit_resource;

typedef bool (*kunit_resource_match_t)(struct kunit *test,
				       struct kunit_resource *res,
				       void *match_data);
typedef void (*kunit_resource_free_t)(struct kunit_resource *);
typedef int (*kunit_resource_init_t)(struct kunit_resource *, void *);

/**
 * struct kunit_resource - represents a *test managed resource*
 * @data: for the user to store arbitrary data.
 * @name: optional name
 * @free: a user supplied function to free the resource. Populated by
 * kunit_resource_alloc().
 *
 * Represents a *test managed resource*, a resource which will automatically be
 * cleaned up at the end of a test case.
 *
 * Resources are reference counted so if a resource is retrieved via
 * kunit_alloc_and_get_resource() or kunit_find_resource(), we need
 * to call kunit_put_resource() to reduce the resource reference count
 * when finished with it.  Note that kunit_alloc_resource() does not require a
 * kunit_resource_put() because it does not retrieve the resource itself.
 *
 * Example:
 *
 * .. code-block:: c
 *
 *	struct kunit_kmalloc_params {
 *		size_t size;
 *		gfp_t gfp;
 *	};
 *
 *	static int kunit_kmalloc_init(struct kunit_resource *res, void *context)
 *	{
 *		struct kunit_kmalloc_params *params = context;
 *		res->data = kmalloc(params->size, params->gfp);
 *
 *		if (!res->data)
 *			return -ENOMEM;
 *
 *		return 0;
 *	}
 *
 *	static void kunit_kmalloc_free(struct kunit_resource *res)
 *	{
 *		kfree(res->data);
 *	}
 *
 *	void *kunit_kmalloc(struct kunit *test, size_t size, gfp_t gfp)
 *	{
 *		struct kunit_kmalloc_params params;
 *
 *		params.size = size;
 *		params.gfp = gfp;
 *
 *		return kunit_alloc_resource(test, kunit_kmalloc_init,
 *			kunit_kmalloc_free, &params);
 *	}
 *
 * Resources can also be named, with lookup/removal done on a name
 * basis also.  kunit_add_named_resource(), kunit_find_named_resource()
 * and kunit_destroy_named_resource().  Resource names must be
 * unique within the test instance.
 */
struct kunit_resource {
	void *data;
	const char *name;
	kunit_resource_free_t free;

	/* private: internal use only. */
	struct kref refcount;
	struct list_head node;
};

struct string_stream_fragment_alloc_context {
	struct kunit *test;
	int len;
};

struct string_stream_alloc_context {
	struct kunit *test;
};

struct string_stream_fragment {
	struct kunit *test;
	struct list_head node;
	char *fragment;
};

struct string_stream {
	size_t length;
	struct list_head fragments;
	/* length and fragments are protected by this lock */
	spinlock_t lock;
	struct kunit *test;
};

/**
 * struct kunit_loc - Identifies the source location of a line of code.
 * @line: the line number in the file.
 * @file: the file name.
 */
struct kunit_loc {
	int line;
	const char *file;
};

/**
 * struct kunit_binary_assert_text - holds strings for &struct
 *	kunit_binary_assert and friends to try and make the structs smaller.
 * @operation: A string representation of the comparison operator (e.g. "==").
 * @left_text: A string representation of the left expression (e.g. "2+2").
 * @right_text: A string representation of the right expression (e.g. "2+2").
 */
struct kunit_binary_assert_text {
	const char *operation;
	const char *left_text;
	const char *right_text;
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
	const struct kunit_binary_assert_text *text;
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

struct kunit_try_catch_context {
	struct kunit *test;
	struct kunit_suite *suite;
	struct kunit_case *test_case;
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
#endif
	char *log;
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
/* KUnit statistic mode:
 * 0 - disabled
 * 1 - only when there is more than one subtest
 * 2 - enabled */
static int kunit_stats_enabled = 2;

static size_t kunit_suite_counter = 1;

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
static int _printk(const char *fmt, ...)
{
	va_list args;
	int r;

	va_start(args, fmt);
	r = vprintf(fmt, args);
	va_end(args);

	return r;
}

/**
 * strlcat - Append a length-limited, C-string to another
 * @dest: The string to be appended to
 * @src: The string to append to it
 * @count: The size of the destination buffer.
 */
static size_t strlcat(char *dest, const char *src, size_t count)
{
	size_t dsize = os_strlen(dest);
	size_t len = os_strlen(src);
	size_t res = dsize + len;

	/* This would be a bug */
	BUG_ON(dsize >= count);

	dest += dsize;
	count -= dsize;
	if (len >= count)
		len = count-1;
	os_memcpy(dest, dsize, src, len);
	dest[len] = 0;
	return res;
}

/**
 * refcount_set - set a refcount's value
 * @r: the refcount
 * @n: value to which the refcount will be set
 */
static void refcount_set(refcount_t *r, int n)
{
	atomic_store(&r->refs, n);
}

static void refcount_warn_saturate(refcount_t *r, enum refcount_saturation_type t)
{
	refcount_set(r, REFCOUNT_SATURATED);

	switch (t) {
	case REFCOUNT_ADD_NOT_ZERO_OVF:
		REFCOUNT_WARN("saturated; leaking memory");
		break;
	case REFCOUNT_ADD_OVF:
		REFCOUNT_WARN("saturated; leaking memory");
		break;
	case REFCOUNT_ADD_UAF:
		REFCOUNT_WARN("addition on 0; use-after-free");
		break;
	case REFCOUNT_SUB_UAF:
		REFCOUNT_WARN("underflow; use-after-free");
		break;
	case REFCOUNT_DEC_LEAK:
		REFCOUNT_WARN("decrement hit 0; leaking memory");
		break;
	default:
		REFCOUNT_WARN("unknown saturation event!?");
	}
}

/**
 * refcount_sub_and_test - subtract from a refcount and test if it is 0
 * @i: amount to subtract from the refcount
 * @r: the refcount
 *
 * Similar to atomic_dec_and_test(), but it will WARN, return false and
 * ultimately leak on underflow and will fail to decrement when saturated
 * at REFCOUNT_SATURATED.
 *
 * Provides release memory ordering, such that prior loads and stores are done
 * before, and provides an acquire ordering on success such that free()
 * must come after.
 *
 * Use of this function is not recommended for the normal reference counting
 * use case in which references are taken and released one at a time.  In these
 * cases, refcount_dec(), or one of its variants, should instead be used to
 * decrement a reference count.
 *
 * Return: true if the resulting refcount is 0, false otherwise
 */
static bool __refcount_sub_and_test(int i, refcount_t *r, int *oldp)
{
	int old = atomic_fetch_sub(&r->refs, 1);

	if (oldp)
		*oldp = old;

	if (old == i) {
		barrier();
		return true;
	}

	if (unlikely(old < 0 || old - i < 0))
		refcount_warn_saturate(r, REFCOUNT_SUB_UAF);

	return false;
}

static bool __refcount_dec_and_test(refcount_t *r, int *oldp)
{
	return __refcount_sub_and_test(1, r, oldp);
}

/**
 * refcount_dec_and_test - decrement a refcount and test if it is 0
 * @r: the refcount
 *
 * Similar to atomic_dec_and_test(), it will WARN on underflow and fail to
 * decrement when saturated at REFCOUNT_SATURATED.
 *
 * Provides release memory ordering, such that prior loads and stores are done
 * before, and provides an acquire ordering on success such that free()
 * must come after.
 *
 * Return: true if the resulting refcount is 0, false otherwise
 */
static bool refcount_dec_and_test(refcount_t *r)
{
	return __refcount_dec_and_test(r, NULL);
}

/**
 * kref_put - decrement refcount for object.
 * @kref: object.
 * @release: pointer to the function that will clean up the object when the
 *	     last reference to the object is released.
 *	     This pointer is required, and it is not acceptable to pass kfree
 *	     in as this function.
 *
 * Decrement the refcount, and if 0, call release().
 * Return 1 if the object was removed, otherwise return 0.  Beware, if this
 * function returns 0, you still can not count on the kref from remaining in
 * memory.  Only use the return value if you want to see if the kref is now
 * gone, not present.
 */
static int kref_put(struct kref *kref, void (*release)(struct kref *kref))
{
	if (refcount_dec_and_test(&kref->refcount)) {
		release(kref);
		return 1;
	}

	return 0;
}

/**
 * refcount_inc - increment a refcount
 * @r: the refcount to increment
 *
 * Similar to atomic_inc(), but will saturate at REFCOUNT_SATURATED and WARN.
 *
 * Provides no memory ordering, it is assumed the caller already has a
 * reference on the object.
 *
 * Will WARN if the refcount is 0, as this represents a possible use-after-free
 * condition.
 */
static inline void refcount_inc(refcount_t *r)
{
	atomic_fetch_add(&r->refs, 1);
}

/**
 * kref_get - increment refcount for object.
 * @kref: object.
 */
static void kref_get(struct kref *kref)
{
	refcount_inc(&kref->refcount);
}

/**
 * kref_init - set a refcount's value to 1.
 * @r: the refcount
 */
static void kref_init(struct kref *kref)
{
	refcount_set(&kref->refcount, 1);
}

/*  
 * Allows for effectively applying __must_check to a macro so we can have
 * both the type-agnostic benefits of the macros while also being able to
 * enforce that the return value is, in fact, checked.
 */
static bool __must_check __must_check_overflow(bool overflow)
{
	return unlikely(overflow);
}

static void *kmalloc_array(size_t n, size_t size)
{
	size_t bytes;

	if (unlikely(check_mul_overflow(n, size, &bytes)))
		return NULL;
	
	if (__builtin_constant_p(n) && __builtin_constant_p(size))
		return malloc(bytes);
	
	return malloc(bytes);
}

static void __read_once_size(const volatile void *p, void *res, int size)
{
	switch (size) {
	case 1: *(__u8_alias_t  *) res = *(volatile __u8_alias_t  *) p; break;
	case 2: *(__u16_alias_t *) res = *(volatile __u16_alias_t *) p; break;
	case 4: *(__u32_alias_t *) res = *(volatile __u32_alias_t *) p; break;
	case 8: *(__u64_alias_t *) res = *(volatile __u64_alias_t *) p; break;
	default:
		barrier();
		__builtin_memcpy((void *)res, (const void *) p, size);
		barrier();
	}
}

static void __write_once_size(volatile void *p, void *res, int size)
{
	switch (size) {
	case 1: *(volatile  __u8_alias_t *) p = *(__u8_alias_t  *) res; break;
	case 2: *(volatile __u16_alias_t *) p = *(__u16_alias_t *) res; break;
	case 4: *(volatile __u32_alias_t *) p = *(__u32_alias_t *) res; break;
	case 8: *(volatile __u64_alias_t *) p = *(__u64_alias_t *) res; break;
	default:
		barrier();
		__builtin_memcpy((void *) p, (const void *) res, size);
		barrier();
	}
}

/**
 * list_empty - tests whether a list is empty
 * @head: the list to test.
 */
static int list_empty(const struct list_head *head)
{
	return READ_ONCE(head->next) == head;
}

/*
 * Delete a list entry by making the prev/next entries
 * point to each other.
 *
 * This is only for internal list manipulation where we know
 * the prev/next entries already!
 */
static void __list_del(struct list_head *prev, struct list_head *next)
{
	next->prev = prev;
	WRITE_ONCE(prev->next, next);
}

static void list_del(struct list_head *entry)
{
	__list_del(entry->prev, entry->next);
	entry->next = LIST_POISON1;
	entry->prev = LIST_POISON2;
}

/*
 * Insert a new entry between two known consecutive entries.
 *
 * This is only for internal list manipulation where we know
 * the prev/next entries already!
 */
static void __list_add(struct list_head *new,
		       struct list_head *prev,
		       struct list_head *next)
{
	next->prev = new;
	new->next = next;
	new->prev = prev;
	prev->next = new;
}

/**
 * list_add_tail - add a new entry
 * @new: new entry to be added
 * @head: list head to add it before
 *
 * Insert a new entry before the specified head.
 * This is useful for implementing queues.
 */
static void list_add_tail(struct list_head *new, struct list_head *head)
{
	__list_add(new, head->prev, head);
}

/**
 * INIT_LIST_HEAD - Initialize a list_head structure
 * @list: list_head structure to be initialized.
 *
 * Initializes the list_head to point to itself.  If it is a list header,
 * the result is an empty list.
 */
static void INIT_LIST_HEAD(struct list_head *list)
{
	WRITE_ONCE(list->next, list);
	list->prev = list;
}

/*
 * Used for static resources and when a kunit_resource * has been created by
 * kunit_alloc_resource().  When an init function is supplied, @data is passed
 * into the init function; otherwise, we simply set the resource data field to
 * the data value passed in.
 */
static int kunit_add_resource(struct kunit *test,
			      kunit_resource_init_t init,
			      kunit_resource_free_t free,
			      struct kunit_resource *res,
			      void *data)
{
	int ret = 0;

	res->free = free;
	kref_init(&res->refcount);

	if (init) {
		ret = init(res, data);
		if (ret)
			return ret;
	} else {
		res->data = data;
	}

	os_spin_lock(&test->lock);
	list_add_tail(&res->node, &test->resources);
	/* refcount for list is established by kref_init() */
	os_spin_unlock(&test->lock);

	return ret;
}

/**
 * kunit_alloc_resource() - Allocates a *test managed resource*.
 * @test: The test context object.
 * @init: a user supplied function to initialize the resource.
 * @free: a user supplied function to free the resource.
 * @internal_gfp: gfp to use for internal allocations, if unsure, use GFP_KERNEL
 * @context: for the user to pass in arbitrary data to the init function.
 *
 * Allocates a *test managed resource*, a resource which will automatically be
 * cleaned up at the end of a test case. See &struct kunit_resource for an
 * example.
 *
 * Note: KUnit needs to allocate memory for a kunit_resource object. You must
 * specify an @internal_gfp that is compatible with the use context of your
 * resource.
 */
static void *kunit_alloc_resource(struct kunit *test,
				  kunit_resource_init_t init,
				  kunit_resource_free_t free,
				  void *context)
{
	struct kunit_resource *res;

	res = calloc(1, sizeof(*res));
	if (! res)
		return NULL;

	if (! kunit_add_resource(test, init, free, res, context))
		return res->data;

	return NULL;
}


static void kunit_kmalloc_array_free(struct kunit_resource *res)
{
	free(res->data);
}

static int kunit_kmalloc_array_init(struct kunit_resource *res, void *context)
{
	struct kunit_kmalloc_array_params *params = context;

	res->data = kmalloc_array(params->n, params->size);
	if (! res->data)
		return -ENOMEM;

	return 0;
}

static void *kunit_kmalloc_array(struct kunit *test, size_t n, size_t size)
{
	struct kunit_kmalloc_array_params params = {
		.size = size,
		.n = n,
	};

	return kunit_alloc_resource(test,
				    kunit_kmalloc_array_init,
				    kunit_kmalloc_array_free,
				    &params);
}

/*
 * Called when refcount reaches zero via kunit_put_resources();
 * should not be called directly.
 */
static void kunit_release_resource(struct kref *kref)
{
	struct kunit_resource *res = container_of(kref, struct kunit_resource,
						  refcount);

	/* If free function is defined, resource was dynamically allocated. */
	if (res->free) {
		res->free(res);
		free(res);
	}
}

/**
 * kunit_put_resource() - When caller is done with retrieved resource,
 *			  kunit_put_resource() should be called to drop
 *			  reference count.  The resource list maintains
 *			  a reference count on resources, so if no users
 *			  are utilizing a resource and it is removed from
 *			  the resource list, it will be freed via the
 *			  associated free function (if any).  Only
 *			  needs to be used if we alloc_and_get() or
 *			  find() resource.
 * @res: resource
 */
static void kunit_put_resource(struct kunit_resource *res)
{
	kref_put(&res->refcount, kunit_release_resource);
}

static void kunit_remove_resource(struct kunit *test, struct kunit_resource *res)
{
	os_spin_lock(&test->lock);
	list_del(&res->node);
	os_spin_unlock(&test->lock);
	
	kunit_put_resource(res);
}

/**
 * kunit_get_resource() - Hold resource for use.  Should not need to be used
 *			  by most users as we automatically get resources
 *			  retrieved by kunit_find_resource*().
 * @res: resource
 */
static void kunit_get_resource(struct kunit_resource *res)
{
	kref_get(&res->refcount);
}

/**
 * kunit_find_resource() - Find a resource using match function/data.
 * @test: Test case to which the resource belongs.
 * @match: match function to be applied to resources/match data.
 * @match_data: data to be used in matching.
 */
static struct kunit_resource *kunit_find_resource(struct kunit *test,
						  kunit_resource_match_t match,
						  void *match_data)
{
	struct kunit_resource *res, *found = NULL;

	os_spin_lock(&test->lock);

	list_for_each_entry_reverse(res, &test->resources, node) {
		if (match(test, res, (void *)match_data)) {
			found = res;
			kunit_get_resource(found);
			break;
		}
	}

	os_spin_unlock(&test->lock);

	return found;
}

/**
 * kunit_resource_instance_match() - Match a resource with the same instance.
 * @test: Test case to which the resource belongs.
 * @res: The resource.
 * @match_data: The resource pointer to match against.
 *
 * An instance of kunit_resource_match_t that matches a resource whose
 * allocation matches @match_data.
 */
static bool kunit_resource_instance_match(struct kunit *test,
					  struct kunit_resource *res,
					  void *match_data)
{
	return res->data == match_data;
}

static void kunit_kfree(struct kunit *test, const void *ptr)
{
	struct kunit_resource *res;

	res = kunit_find_resource(test, kunit_resource_instance_match,
				  (void *)ptr);

	/*
	 * Removing the resource from the list of resources drops the
	 * reference count to 1; the final put will trigger the free.
	 */
	kunit_remove_resource(test, res);

	kunit_put_resource(res);
}

/**
 * kunit_kmalloc() - Like kmalloc() except the allocation is *test managed*.
 * @test: The test context object.
 * @size: The size in bytes of the desired memory.
 *
 * See kmalloc() and kunit_kmalloc_array() for more information.
 */
static void *kunit_kmalloc(struct kunit *test, size_t size)
{
	return kunit_kmalloc_array(test, 1, size);
}

/**
 * kunit_kzalloc() - Just like kunit_kmalloc(), but zeroes the allocation.
 * @test: The test context object.
 * @size: The size in bytes of the desired memory.
 *
 * See kzalloc() and kunit_kmalloc_array() for more information.
 */
static void *kunit_kzalloc(struct kunit *test, size_t size)
{
	void  *p;

	p = kunit_kmalloc(test, size);
	os_memset(p, 0 , size);

	return p;
}

static char *string_stream_get_string(struct string_stream *stream)
{
	struct string_stream_fragment *frag_container;
	size_t buf_len = stream->length + 1; /* +1 for null byte. */
	char *buf;

	buf = kunit_kzalloc(stream->test, buf_len);
	if (! buf)
		return NULL;

	os_spin_lock(&stream->lock);

	list_for_each_entry(frag_container, &stream->fragments, node)
		strlcat(buf, frag_container->fragment, buf_len);

	os_spin_unlock(&stream->lock);

	return buf;
}

static bool string_stream_is_empty(struct string_stream *stream)
{
	return list_empty(&stream->fragments);
}

/*
 * Append formatted message to log, size of which is limited to
 * KUNIT_LOG_SIZE bytes (including null terminating byte).
 */
static void kunit_log_append(char *log, const char *fmt, ...)
{
	char line[KUNIT_LOG_SIZE];
	va_list args;
	int len_left;

	if (! log)
		return;

	len_left = KUNIT_LOG_SIZE - os_strlen(log) - 1;
	if (len_left <= 0)
		return;

	va_start(args, fmt);
	vsnprintf(line, sizeof(line), fmt, args);
	va_end(args);

	os_strncat(log, line, len_left);
}

static void kunit_print_string_stream(struct kunit *test,
				      struct string_stream *stream)
{
	struct string_stream_fragment *fragment;
	char *buf;

	if (string_stream_is_empty(stream))
		return;

	buf = string_stream_get_string(stream);
	if (!  buf) {
		kunit_err(test,
			  "Could not allocate buffer, dumping stream:\n");
		list_for_each_entry(fragment, &stream->fragments, node) {
			kunit_err(test, "%s", fragment->fragment);
		}
		kunit_err(test, "\n");
	} else {
		kunit_err(test, "%s", buf);
		kunit_kfree(test, buf);
	}
}

static int kunit_destroy_resource(struct kunit *test, kunit_resource_match_t match,
				  void *match_data)
{
	struct kunit_resource *res = kunit_find_resource(test, match,
							 match_data);
	if (! res)
		return -ENOENT;

	kunit_remove_resource(test, res);

	/* We have a reference also via _find(); drop it. */
	kunit_put_resource(res);

	return 0;
}

static int string_stream_fragment_destroy(struct string_stream_fragment *frag)
{
	return kunit_destroy_resource(frag->test,
				      kunit_resource_instance_match,
				      frag);
}

static void string_stream_fragment_free(struct kunit_resource *res)
{
	struct string_stream_fragment *frag = res->data;

	list_del(&frag->node);
	kunit_kfree(frag->test, frag->fragment);
	kunit_kfree(frag->test, frag);
}

static int string_stream_fragment_init(struct kunit_resource *res,
				       void *context)
{
	struct string_stream_fragment_alloc_context *ctx = context;
	struct string_stream_fragment *frag;

	frag = kunit_kzalloc(ctx->test, sizeof(*frag));
	if (! frag)
		return -ENOMEM;

	frag->test = ctx->test;
	frag->fragment = kunit_kmalloc(ctx->test, ctx->len);
	if (! frag->fragment)
		return -ENOMEM;

	res->data = frag;

	return 0;
}

static struct string_stream_fragment *alloc_string_stream_fragment(
		struct kunit *test, int len)
{
	struct string_stream_fragment_alloc_context context = {
		.test = test,
		.len = len,
	};

	return kunit_alloc_resource(test,
				    string_stream_fragment_init,
				    string_stream_fragment_free,
				    &context);
}

static int string_stream_vadd(struct string_stream *stream,
			      const char *fmt,
			      va_list args)
{
	struct string_stream_fragment *frag_container;
	int len;
	va_list args_for_counting;

	/* Make a copy because `vsnprintf` could change it */
	va_copy(args_for_counting, args);

	/* Need space for null byte. */
	len = vsnprintf(NULL, 0, fmt, args_for_counting) + 1;

	va_end(args_for_counting);

	frag_container = alloc_string_stream_fragment(stream->test,
						      len);
	if (! frag_container)
		return -ENOMEM;

	len = vsnprintf(frag_container->fragment, len, fmt, args);
	os_spin_lock(&stream->lock);
	stream->length += len;
	list_add_tail(&frag_container->node, &stream->fragments);
	os_spin_unlock(&stream->lock);

	return 0;
}

static int string_stream_add(struct string_stream *stream, const char *fmt, ...)
{
	va_list args;
	int result;

	va_start(args, fmt);
	result = string_stream_vadd(stream, fmt, args);
	va_end(args);

	return result;
}

static void string_stream_clear(struct string_stream *stream)
{
	struct string_stream_fragment *frag_container, *frag_container_safe;

	os_spin_lock(&stream->lock);
	list_for_each_entry_safe(frag_container,
				 frag_container_safe,
				 &stream->fragments,
				 node) {
		string_stream_fragment_destroy(frag_container);
	}

	stream->length = 0;
	os_spin_unlock(&stream->lock);
}

static void string_stream_free(struct kunit_resource *res)
{
	struct string_stream *stream = res->data;

	string_stream_clear(stream);
}

static int string_stream_init(struct kunit_resource *res, void *context)
{
	struct string_stream *stream;
	struct string_stream_alloc_context *ctx = context;

	stream = kunit_kzalloc(ctx->test, sizeof(*stream));
	if (! stream)
		return -ENOMEM;

	res->data = stream;
	stream->test = ctx->test;

	INIT_LIST_HEAD(&stream->fragments);
	os_spin_init(&stream->lock);

	return 0;
}

struct string_stream *alloc_string_stream(struct kunit *test)
{
	struct string_stream_alloc_context context = {
		.test = test,
	};

	return kunit_alloc_resource(test,
				    string_stream_init,
				    string_stream_free,
				    &context);
}

/* XXX */
static void /* __noreturn */ kunit_try_catch_throw(struct kunit_try_catch *try_catch)
{
	try_catch->try_result = -EFAULT;
#if 0
	kthread_complete_and_exit(try_catch->try_completion, -EFAULT);
#endif
}

/* XXX */
static void /* __noreturn */ kunit_abort(struct kunit *test)
{
	kunit_try_catch_throw(&test->try_catch); /* Does not return. */

	/* Throw could not abort from test.
	 *
	 * XXX: we should never reach this line! As kunit_try_catch_throw is
	 * marked __noreturn. */
	WARN_ONCE(true, "Throw could not abort from test!\n");
}

static void kunit_assert_prologue(const struct kunit_loc *loc,
				  enum kunit_assert_type type,
				  struct string_stream *stream)
{
	const char *expect_or_assert = NULL;

	switch (type) {
	case KUNIT_EXPECTATION:
		expect_or_assert = "EXPECTATION";
		break;
	case KUNIT_ASSERTION:
		expect_or_assert = "ASSERTION";
		break;
	}

	string_stream_add(stream, "%s FAILED at %s:%d\n",
			  expect_or_assert, loc->file, loc->line);
}

static void kunit_set_failure(struct kunit *test)
{
	WRITE_ONCE(test->status, KUNIT_FAILURE);
}

static int string_stream_destroy(struct string_stream *stream)
{
	return kunit_destroy_resource(stream->test,
				      kunit_resource_instance_match,
				      stream);
}

static void kunit_fail(struct kunit *test, const struct kunit_loc *loc,
		       enum kunit_assert_type type, struct kunit_assert *assert,
		       const struct va_format *message)
{
	struct string_stream *stream;

	kunit_set_failure(test);

	stream = alloc_string_stream(test);
	if (! stream) {
		WARN(true,
		     "Could not allocate stream to print failed assertion in %s:%d\n",
		     loc->file,
		     loc->line);
		return;
	}

	kunit_assert_prologue(loc, type, stream);
	assert->format(assert, message, stream);

	kunit_print_string_stream(test, stream);

	WARN_ON(string_stream_destroy(stream));
}

static void kunit_do_failed_assertion(struct kunit *test,
				      const struct kunit_loc *loc,
				      enum kunit_assert_type type,
				      struct kunit_assert *assert,
				      const char *fmt, ...)
{
	va_list args;
	struct va_format message;
	va_start(args, fmt);

	message.fmt = fmt;
	message.va = &args;

	kunit_fail(test, loc, type, assert, &message);

	va_end(args);

	if (type == KUNIT_ASSERTION)
		kunit_abort(test);
}

/* Checks if `text` is a literal representing `value`, e.g. "5" and 5 */
static bool is_literal(struct kunit *test, const char *text, long long value)
{
	char *buffer;
	int len;
	bool ret;

	len = snprintf(NULL, 0, "%lld", value);
	if (os_strlen(text) != len)
		return false;

	buffer = kunit_kmalloc(test, len + 1);
	if (!buffer)
		return false;

	snprintf(buffer, len+1, "%lld", value);
	ret = os_strncmp(buffer, text, len) == 0;

	kunit_kfree(test, buffer);
	return ret;
}

static void kunit_assert_print_msg(const struct va_format *message,
				   struct string_stream *stream)
{
	if (message->fmt)
		string_stream_add(stream, "\n%pV", message);
}

static void  kunit_binary_assert_format(const struct kunit_assert *assert,
				       const struct va_format *message,
				       struct string_stream *stream)
{
	struct kunit_binary_assert *binary_assert;

	binary_assert = container_of(assert, struct kunit_binary_assert,
				     assert);
	string_stream_add(stream,
			  KUNIT_SUBTEST_INDENT "Expected %s %s %s, but\n",
			  binary_assert->text->left_text,
			  binary_assert->text->operation,
			  binary_assert->text->right_text);
	if (! is_literal(stream->test, binary_assert->text->left_text,
			binary_assert->left_value))
		string_stream_add(stream, KUNIT_SUBSUBTEST_INDENT "%s == %lld\n",
				  binary_assert->text->left_text,
				  binary_assert->left_value);
	if (! is_literal(stream->test, binary_assert->text->right_text,
			binary_assert->right_value))
		string_stream_add(stream, KUNIT_SUBSUBTEST_INDENT "%s == %lld",
				  binary_assert->text->right_text,
				  binary_assert->right_value);
	kunit_assert_print_msg(message, stream);
}

static char *kunit_status_to_ok_not_ok(enum kunit_status status)
{
	switch (status) {
	case KUNIT_SKIPPED:
	case KUNIT_SUCCESS:
		return "ok";
	case KUNIT_FAILURE:
		return "not ok";
	}
	return "invalid";
}

static void kunit_print_ok_not_ok(void *test_or_suite,
				  bool is_test,
				  enum kunit_status status,
				  size_t test_number,
				  const char *description,
				  const char *directive)
{
#if 1
	struct kunit_suite *suite = is_test ? NULL : test_or_suite;
	struct kunit *test = is_test ? test_or_suite : NULL;
	const char *directive_header = (status == KUNIT_SKIPPED) ? " # SKIP " : "";

	/*
	 * We do not log the test suite results as doing so would
	 * mean debugfs display would consist of the test suite
	 * description and status prior to individual test results.
	 * Hence directly printk the suite status, and we will
	 * separately seq_printf() the suite status for the debugfs
	 * representation.
	 */
	if (suite)
		pr_info("%s %zd - %s%s%s\n",
			kunit_status_to_ok_not_ok(status),
			test_number, description, directive_header,
			(status == KUNIT_SKIPPED) ? directive : "");
	else
		kunit_log(test,
			  KUNIT_SUBTEST_INDENT "%s %zd - %s%s%s",
			  kunit_status_to_ok_not_ok(status),
			  test_number, description, directive_header,
			  (status == KUNIT_SKIPPED) ? directive : "");
#endif
}

static enum kunit_status kunit_suite_has_succeeded(struct kunit_suite *suite)
{
	const struct kunit_case *test_case;
	enum kunit_status status = KUNIT_SKIPPED;

	kunit_suite_for_each_test_case(suite, test_case) {
		if (test_case->status == KUNIT_FAILURE)
			return KUNIT_FAILURE;
		else if (test_case->status == KUNIT_SUCCESS)
			status = KUNIT_SUCCESS;
	}

	return status;
}

static void kunit_print_subtest_end(struct kunit_suite *suite)
{
	kunit_print_ok_not_ok((void *)suite, false,
			      kunit_suite_has_succeeded(suite),
			      kunit_suite_counter++,
			      suite->name,
			      suite->status_comment);
}

static bool kunit_should_print_stats(struct kunit_result_stats stats)
{
	if (kunit_stats_enabled == 0)
		return false;

	if (kunit_stats_enabled == 2)
		return true;

	return (stats.total > 1);
}

static void kunit_print_suite_stats(struct kunit_suite *suite,
				    struct kunit_result_stats suite_stats,
				    struct kunit_result_stats param_stats)
{
	if (kunit_should_print_stats(suite_stats)) {
		kunit_log(suite,
			  KUNIT_SUBTEST_INDENT "# %s: pass:%lu fail:%lu skip:%lu total:%lu",
			  suite->name,
			  suite_stats.passed,
			  suite_stats.failed,
			  suite_stats.skipped,
			  suite_stats.total);
	}

	if (kunit_should_print_stats(param_stats)) {
		kunit_log(suite,
			  KUNIT_SUBTEST_INDENT "# Totals: pass:%lu fail:%lu skip:%lu total:%lu",
			  param_stats.passed,
			  param_stats.failed,
			  param_stats.skipped,
			  param_stats.total);
	}
}

static size_t kunit_suite_num_test_cases(struct kunit_suite *suite)
{
	struct kunit_case *test_case;
	size_t len = 0;

	kunit_suite_for_each_test_case(suite, test_case)
		len++;

	return len;
}

static void kunit_update_stats(struct kunit_result_stats *stats,
			       enum kunit_status status)
{
	switch (status) {
	case KUNIT_SUCCESS:
		stats->passed++;
		break;
	case KUNIT_SKIPPED:
		stats->skipped++;
		break;
	case KUNIT_FAILURE:
		stats->failed++;
		break;
	}

	stats->total++;
}

static void kunit_catch_run_case(void *data)
{
#if 0
	struct kunit_try_catch_context *ctx = data;
	struct kunit *test = ctx->test;
	struct kunit_suite *suite = ctx->suite;
	int try_exit_code = kunit_try_catch_get_result(&test->try_catch);

	if (try_exit_code) {
		kunit_set_failure(test);
		/*
		 * Test case could not finish, we have no idea what state it is
		 * in, so don't do clean up.
		 */
		if (try_exit_code == -ETIMEDOUT) {
			kunit_err(test, "test case timed out\n");
		/*
		 * Unknown internal error occurred preventing test case from
		 * running, so there is nothing to clean up.
		 */
		} else {
			kunit_err(test, "internal error occurred preventing test case from running: %d\n",
				  try_exit_code);
		}
		return;
	}

	/*
	 * Test case was run, but aborted. It is the test case's business as to
	 * whether it failed or not, we just need to clean up.
	 */
	kunit_run_case_cleanup(test, suite);
#endif
}

static void kunit_cleanup(struct kunit *test)
{
	struct kunit_resource *res;

	/*
	 * test->resources is a stack - each allocation must be freed in the
	 * reverse order from which it was added since one resource may depend
	 * on another for its entire lifetime.
	 * Also, we cannot use the normal list_for_each constructs, even the
	 * safe ones because *arbitrary* nodes may be deleted when
	 * kunit_resource_free is called; the list_for_each_safe variants only
	 * protect against the current node being deleted, not the next.
	 */
	while (true) {
		os_spin_lock(&test->lock);
		if (list_empty(&test->resources)) {
			os_spin_unlock(&test->lock);
			break;
		}
		res = list_last_entry(&test->resources,
				      struct kunit_resource,
				      node);
		/*
		 * Need to unlock here as a resource may remove another
		 * resource, and this can't happen if the test->lock
		 * is held.
		 */
		os_spin_unlock(&test->lock);
		kunit_remove_resource(test, res);
	}
#if 0
	/* see linux-xxx/./include/linux/sched.h:
	   struct task_struct { ...
	   #if IS_ENABLED(CONFIG_KUNIT)
	   struct kunit                    *kunit_test;
	   #endif
	   ...*/
	current->kunit_test = NULL;
#endif
}

static void kunit_case_internal_cleanup(struct kunit *test)
{
	kunit_cleanup(test);
}

/*
 * Performs post validations and cleanup after a test case was run.
 * XXX: Should ONLY BE CALLED AFTER kunit_run_case_internal!
 */
static void kunit_run_case_cleanup(struct kunit *test,
				   struct kunit_suite *suite)
{
	if (suite->exit)
		suite->exit(test);

	kunit_case_internal_cleanup(test);
}

/*
 * Initializes and runs test case. Does not clean up or do post validations.
 */
static void kunit_run_case_internal(struct kunit *test,
				    struct kunit_suite *suite,
				    struct kunit_case *test_case)
{
	if (suite->init) {
		int ret;

		ret = suite->init(test);
		if (ret) {
			kunit_err(test, "failed to initialize: %d\n", ret);
			kunit_set_failure(test);
			return;
		}
	}

	test_case->run_case(test);
}

static void kunit_try_run_case(void *data)
{
	struct kunit_try_catch_context *ctx = data;
	struct kunit *test = ctx->test;
	struct kunit_suite *suite = ctx->suite;
	struct kunit_case *test_case = ctx->test_case;

	/* XXX */
#if 0
	/* see linux-xxx/./include/linux/sched.h:
	   struct task_struct { ...
	   #if IS_ENABLED(CONFIG_KUNIT)
	   struct kunit                    *kunit_test;
	   #endif
	   ...*/
	current->kunit_test = test;
#endif

	/*
	 * kunit_run_case_internal may encounter a fatal error; if it does,
	 * abort will be called, this thread will exit, and finally the parent
	 * thread will resume control and handle any necessary clean up.
	 */
	kunit_run_case_internal(test, suite, test_case);

	/* This line may never be reached. */
	kunit_run_case_cleanup(test, suite);
}

static void kunit_try_catch_init(struct kunit_try_catch *try_catch,
				 struct kunit *test,
				 kunit_try_catch_func_t try,
				 kunit_try_catch_func_t catch)
{
	try_catch->test = test;
	try_catch->try = try;
	try_catch->catch = catch;
}

static void kunit_init_test(struct kunit *test, const char *name, char *log)
{
	os_spin_init(&test->lock);
	INIT_LIST_HEAD(&test->resources);
	test->name = name;
	test->log = log;
	if (test->log)
		test->log[0] = '\0';
	test->status = KUNIT_SUCCESS;
	test->status_comment[0] = '\0';
}

/*
 * Performs all logic to run a test case. It also catches most errors that
 * occur in a test case and reports them as failures.
 */
static void kunit_run_case_catch_errors(struct kunit_suite *suite,
					struct kunit_case *test_case,
					struct kunit *test)
{
	struct kunit_try_catch_context context;
	struct kunit_try_catch *try_catch;

	kunit_init_test(test, test_case->name, test_case->log);
	try_catch = &test->try_catch;

	kunit_try_catch_init(try_catch,
			     test,
			     kunit_try_run_case,
			     kunit_catch_run_case);
	context.test = test;
	context.suite = suite;
	context.test_case = test_case;
#if 0
	kunit_try_catch_run(try_catch, &context);

	/* Propagate the parameter result to the test case. */
	if (test->status == KUNIT_FAILURE)
		test_case->status = KUNIT_FAILURE;
	else if (test_case->status != KUNIT_FAILURE && test->status == KUNIT_SUCCESS)
		test_case->status = KUNIT_SUCCESS;
#endif
}

static void kunit_print_subtest_start(struct kunit_suite *suite)
{
	kunit_log(suite, KUNIT_SUBTEST_INDENT "# Subtest: %s",
		  suite->name);
	
	kunit_log(suite, KUNIT_SUBTEST_INDENT "1..%zd",
		  kunit_suite_num_test_cases(suite));
}

static int kunit_run_tests(struct kunit_suite *suite)
{
	char param_desc[KUNIT_PARAM_DESC_SIZE];
	struct kunit_case *test_case;
	struct kunit_result_stats suite_stats = { 0 };
	struct kunit_result_stats total_stats = { 0 };

	kunit_print_subtest_start(suite);

	kunit_suite_for_each_test_case(suite, test_case) {
		struct kunit test = { .param_value = NULL, .param_index = 0 };
		struct kunit_result_stats param_stats = { 0 };
		test_case->status = KUNIT_SKIPPED;

		if (! test_case->generate_params) {
#if 0
			/* Non-parameterised test. */
			kunit_run_case_catch_errors(suite, test_case, &test);
			kunit_update_stats(&param_stats, test.status);
#endif
		} else {
#if 0
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
#endif
		}

#if 0
		kunit_print_test_stats(&test, param_stats);

		kunit_print_ok_not_ok(&test, true, test_case->status,
				      kunit_test_case_num(suite, test_case),
				      test_case->name,
				      test.status_comment);

		kunit_update_stats(&suite_stats, test_case->status);
		kunit_accumulate_stats(&total_stats, param_stats);
#endif
	}

	kunit_print_suite_stats(suite, suite_stats, total_stats);
	kunit_print_subtest_end(suite);

	return 0;
}

/* VUnit test function. */
static int vu_add(int left, int right)
{
        return left + right;
}

static void vu_test(struct kunit *test)
{
	KUNIT_EXPECT_EQ(test, 2, vu_add(1, 1));
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
