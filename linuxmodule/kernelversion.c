/*
 * kernelversion.c -- make sure we get a Linux module kernel version variable
 * Robert Morris
 *
 * Copyright (c) 1999 Massachusetts Institute of Technology.
 *
 * This software is being provided by the copyright holders under the GNU
 * General Public License, either version 2 or, at your discretion, any later
 * version. For more information, see the `COPYRIGHT' file in the source
 * distribution.
 */

/*
 * Use C, because g++ seems to ignore the version declaration, which
 * ends up looking like
 *  const char __module_kernel_version[] __attribute__((section(".modinfo"))) =
 *  "kernel_version=" "2.2.6" ;
 */

#define __KERNEL__
#define MODULE
#include <linux/module.h>
