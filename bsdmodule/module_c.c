/*
 * module_c.c -- FreeBSD kernel module glue
 *
 * Copyright (c) 2006 Bruce M. Simpson.
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

#include <click/config.h>
#include <sys/param.h>
#include <sys/systm.h>
#include <sys/kernel.h>
#include <sys/lock.h>
#include <sys/mutex.h>
#include <sys/malloc.h>
#include <sys/mount.h>
#include <sys/vnode.h>

extern int init_module(void);
extern void cleanup_module(void);

extern struct vfsops clickfs_vfsops;

static int
click_modevent(module_t mod, int type, void *data)
{
	int ret;

	/* Load and unload the VFS part first */
	ret = vfs_modevent(mod, type, data);
	if (ret != 0) {
		return ret;
	}

	switch (type) {
	case MOD_LOAD:
		printf("Click module loading\n");
		if (init_module()) {
			ret = EINVAL;
			break;
		}
		ret = 0;
		break;
	case MOD_UNLOAD:
		printf("Click module unloading\n");
		cleanup_module();
		ret = 0;
		break;
	case MOD_SHUTDOWN:
		printf("Click module shutdown\n");
		ret = 0;
		break;
	default:
		printf("Click: unknown module command %d\n", type);
		ret = EOPNOTSUPP;
		break;
	}

	return ret;
}

/*
 * We have to break the rules of VFS_SET() here somewhat.
 */
static struct vfsconf click_vfsconf = {
	VFS_VERSION,
	"click",
	&clickfs_vfsops,
	-1,
	0,
	VFCF_SYNTHETIC,
	NULL,
	{ NULL, NULL },
};

static moduledata_t click_mod = {
	"click",
	click_modevent,
	&click_vfsconf
};

DECLARE_MODULE(click, click_mod, SI_SUB_VFS, SI_ORDER_MIDDLE);

MODULE_VERSION(click, 1);
