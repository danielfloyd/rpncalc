#include <linux/module.h> 
#include <linux/kernel.h>
#include <linux/init.h>

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Daniel Floyd <daniel.m.floyd at gmail.com");
MODULE_DESCRIPTION("An RPN calculator.");

/*

	Module parameters
		create
		delete
	Device parameters
		push - w
		pop - r
		op - w
		size - r
		at - rw

*/

static int __init rpncalc_init(void)
{
    printk(KERN_INFO "rpncalc_init\n");
    return 0;    // Non-zero return means that the module couldn't be loaded.
}

static void __exit rpncalc_cleanup(void)
{
    printk(KERN_INFO "rpncalc_cleanup\n");
}

module_init(rpncalc_init);
module_exit(rpncalc_cleanup);