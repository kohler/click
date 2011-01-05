/*
 * clickfs.cc -- Click configuration filesystem for BSD
 * Nickolai Zeldovich
 *
 * Copyright (c) 2001 Massachusetts Institute of Technology
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
#include "modulepriv.hh"
#include "clickfs_tree.hh"

#include <click/cxxprotect.h>
CLICK_CXX_PROTECT
#include <sys/mount.h>
#include <sys/types.h>
#include <sys/time.h>
CLICK_CXX_UNPROTECT
#include <click/cxxunprotect.h>

#include <click/string.hh>
CLICK_USING_DECLS

struct clickfs_mount {
    struct vnode *click_root;
};

static int
clickfs_mount(struct mount *mp, struct thread *td)
{
    size_t count;
    int error;
    struct clickfs_mount *cmp;

    if (mp->mnt_flag & MNT_UPDATE)
	return EOPNOTSUPP;

    vfs_getnewfsid(mp);

    MALLOC(cmp, struct clickfs_mount *, sizeof(struct clickfs_mount),
	   M_CLICKFS, M_WAITOK | M_ZERO);
    mp->mnt_data = (qaddr_t) cmp;

    mp->mnt_stat.f_bsize  = DEV_BSIZE;
    mp->mnt_stat.f_iosize = DEV_BSIZE;
    mp->mnt_stat.f_owner  = 0;
    mp->mnt_stat.f_blocks = 1;
    mp->mnt_stat.f_bfree  = 1;
    mp->mnt_stat.f_bavail = 1;
    mp->mnt_stat.f_files  = 1;
    mp->mnt_stat.f_ffree  = 1;
    mp->mnt_stat.f_flags  = mp->mnt_flag;

    vfs_mountedfrom(mp, "clickfs");

    error = clickfs_rootvnode(mp, &cmp->click_root);
    if (error < 0) {
	free(cmp, M_CLICKFS);
	return error;
    }

    return 0;
}

static int
clickfs_unmount(struct mount *mp, int mntflags, struct thread *td)
{
    struct clickfs_mount *cmp = (struct clickfs_mount *)mp->mnt_data;
    int error;
    int flags = 0;

    if (mntflags & MNT_FORCE)
	flags |= FORCECLOSE;

    error = vflush(mp, 1, flags, td); // there is 1 extra vnode ref.
    if (error)
	return error;

    free(mp->mnt_data, M_CLICKFS);
    mp->mnt_data = 0;

    return 0;
}

static int
clickfs_root(struct mount *mp, int flags, struct vnode **vpp, struct thread *td)
{
    struct clickfs_mount *cmp = (struct clickfs_mount *)mp->mnt_data;

    *vpp = cmp->click_root;
    VREF(*vpp);
#if __FreeBSD_version >= 800000
    vn_lock(*vpp, LK_EXCLUSIVE | LK_RETRY);
#else
    vn_lock(*vpp, LK_EXCLUSIVE | LK_RETRY, curthread);
#endif

    return 0;
}

static int
clickfs_statfs(struct mount *mp, struct statfs *sbp, struct thread *td)
{
    memcpy(sbp, &mp->mnt_stat, sizeof(*sbp));
    return 0;
}

static int
clickfs_sync(struct mount *mp, int waitfor, struct thread *td)
{
    return 0;
}

static int
clickfs_init(struct vfsconf *vfsp)
{
    return 0;
}

static int
clickfs_uninit(struct vfsconf *vfsp)
{
    return 0;
}

struct vfsops clickfs_vfsops = {
	clickfs_mount,
	NULL,
	clickfs_unmount,
	clickfs_root,
	NULL,
	clickfs_statfs,
	clickfs_sync,
	NULL,
	NULL,
	NULL,
	clickfs_init,
	clickfs_uninit,
	NULL,
	NULL,
	NULL, /* susp_clean */
};
