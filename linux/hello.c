/* make -C /home/gerald/uml/linux-5.18-rc3 ARCH=i386 M=`pwd` modules
 *   ERROR: Kernel configuration is invalid.
 *          include/generated/autoconf.h or include/config/auto.conf are missing.
 *          Run 'make oldconfig && make prepare' on kernel src to fix it. */
#include <linux/init.h>
#include <linux/module.h>

MODULE_LICENSE("Dual BSD/GPL");

static int hello_init(void)
{
	printk(KERN_ALERT "Hello, world.\n");
	return 0;
}

static void hello_exit(void)
{
	printk(KERN_ALERT "Goodbye.\n");
} 
module_init(hello_init);
module_exit(hello_exit);
