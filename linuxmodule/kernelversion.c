/*
 * Make sure we get a Linux module kernel version variable.
 *
 * Use C, because g++ seems to ignore the version declaration, which
 * ends up looking like
 *  const char __module_kernel_version[] __attribute__((section(".modinfo"))) =
 *  "kernel_version=" "2.2.6" ;
 */

#define __KERNEL__
#define MODULE
#include <linux/module.h>
