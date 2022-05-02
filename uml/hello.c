// SPDX-License-Identifier: GPL-2.0

#include <kunit/test.h>
#include <linux/init.h>
#include <linux/module.h>

/* KUnit test function. */
static int hello_add(int left, int right)
{
        return left + right;
}

/* KUnit test list. */
static void hello_add_test_basic(struct kunit *test)
{
        KUNIT_EXPECT_EQ(test, 1, hello_add(1, 0));
        KUNIT_EXPECT_EQ(test, 2, hello_add(1, 1));
        KUNIT_EXPECT_EQ(test, 0, hello_add(-1, 1));
        KUNIT_EXPECT_EQ(test, INT_MAX, hello_add(0, INT_MAX));
        KUNIT_EXPECT_EQ(test, -1, hello_add(INT_MAX, INT_MIN));
}

/* A test case should be static and should only be created with the KUNIT_CASE()
 * macro; additionally, every array of test cases should be terminated with an
 * empty test case. */
static struct kunit_case hello_test_cases[] = {
        KUNIT_CASE(hello_add_test_basic),
        {},
};


/* Define a related collection of struct kunit_case. */
static struct kunit_suite hello_test_suite = {
        .name = "hello_test",
        .test_cases = hello_test_cases,
};

#if 1
static int __init hello_init(void)
{
	printk(KERN_ALERT "Hello, world.\n");

	/* Execute a single test set directly. */
	kunit_run_tests(&hello_test_suite);

	return 0;
}

static void __exit hello_exit(void)
{
	printk(KERN_ALERT "Goodbye.\n");
}

module_init(hello_init);
module_exit(hello_exit);

#else
/* Implicitely module_init and module_exit habe been generated, see
 * <linux-kernel-version>/include/kunit/test.h:
 *    #define kunit_test_suites_for_module(__suites) 
 */
kunit_test_suite(hello_test_suite);	
#endif

MODULE_LICENSE("GPL v2");
MODULE_AUTHOR("Gerald Schueller <gerald.schueller@web.de>");
