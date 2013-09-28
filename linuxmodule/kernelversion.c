/*
 * kernelversion.c -- make sure we get a Linux module kernel version variable
 * Robert Morris, Eddie Kohler
 *
 * Copyright (c) 1999 Massachusetts Institute of Technology
 * Copyright (c) 2005 Regents of the University of California
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
#include <linux/version.h>
#include <linux/module.h>
#include "moduleparm.h"

#define CLICK_INT_MODULE_PARAM(param)	module_param(param, int, 0)

static int accessible = 1;
CLICK_INT_MODULE_PARAM(accessible);
MODULE_PARM_DESC(accessible, "make /click world-readable [1]");

static int uid = 0;
static int gid = 0;
CLICK_INT_MODULE_PARAM(uid);
CLICK_INT_MODULE_PARAM(gid);
MODULE_PARM_DESC(uid, "UID owning /click [0]");
MODULE_PARM_DESC(gid, "GID owning /click [0]");

#if HAVE_MULTITHREAD
static int threads = 1;
CLICK_INT_MODULE_PARAM(threads);
MODULE_PARM_DESC(threads, "number of Click threads per router [1]");
#endif

#ifdef MODULE_LICENSE
MODULE_LICENSE("Dual BSD/GPL");
#endif

static int greedy = 0;
CLICK_INT_MODULE_PARAM(greedy);
MODULE_PARM_DESC(greedy, "Click takes a whole CPU [0]");

static int cpu = -1;
CLICK_INT_MODULE_PARAM(cpu);
MODULE_PARM_DESC(cpu, "Click thread preferred CPU [-1=any]");

int
click_parm(int which)
{
    switch (which) {
    case CLICKPARM_ACCESSIBLE:
	return accessible;
    case CLICKPARM_UID:
	return uid;
    case CLICKPARM_GID:
	return gid;
    case CLICKPARM_GREEDY:
	return greedy;
    case CLICKPARM_CPU:
	return cpu;
#if HAVE_MULTITHREAD
    case CLICKPARM_THREADS:
	return threads;
#endif
    default:
	return 0;
    }
}
