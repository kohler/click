/*
 * kernelversion.c -- make sure we get a Linux module kernel version variable
 * Robert Morris
 *
 * Copyright (c) 1999 Massachusetts Institute of Technology
 *
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files (the "Software"),
 * to deal in the Software without restriction, subject to the conditions
 * listed in the Click LICENSE file. These conditions include: you must
 * preserve this copyright notice, and you cannot mention the copyright
 * holders in advertising related to the Software without their permission.
 * The Software is provided WITHOUT ANY WARRANTY, EXPRESS OR IMPLIED. This
 * notice is a summary of the Click LICENSE file; the license in that file is
 * legally binding.
 */

/*
 * Use C, because g++ seems to ignore the version declaration, which
 * ends up looking like
 *  const char __module_kernel_version[] __attribute__((section(".modinfo"))) =
 *  "kernel_version=" "2.2.6" ;
 */

#include <click/config.h>
#include <linux/module.h>

static int accessible = 1;
MODULE_PARM(accessible, "i");
MODULE_PARM_DESC(accessible, "make /proc/click world-readable [1]");

#if __MTCLICK__
static int threads = 1;
MODULE_PARM(threads, "i");
MODULE_PARM_DESC(threads, "number of Click threads per router [1]");
#endif

int
click_accessible(void)
{
  return accessible;
}

#if __MTCLICK__
int
click_threads(void)
{
  return threads;
}
#endif
