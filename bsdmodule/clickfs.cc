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
#include <sys/vnode.h>
CLICK_CXX_UNPROTECT
#include <click/cxxunprotect.h>

#include <click/string.hh>

extern vop_t **clickfs_root_vnops;
extern struct vnodeopv_desc clickfs_vnodeop_opv_desc;

struct clickfs_mount {
    struct vnode *click_root;
};

static int
clickfs_mount(struct mount *mp, char *user_path, caddr_t data,
	      struct nameidata *ndp, struct proc *p)
{
    char path[MAXPATHLEN];
    size_t count;
    int error;
    struct clickfs_mount *cmp;

    if (mp->mnt_flag & MNT_UPDATE)
	return EOPNOTSUPP;

    vfs_getnewfsid(mp);

    error = copyinstr(user_path, path, MAXPATHLEN, &count);
    if (error)
	return error;

    MALLOC(cmp, struct clickfs_mount *, sizeof(struct clickfs_mount),
	   M_TEMP, M_WAITOK);
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

    strcpy(mp->mnt_stat.f_mntonname, path);
    strcpy(mp->mnt_stat.f_mntfromname, "clickfs");
    strcpy(mp->mnt_stat.f_fstypename, "clickfs");

    error = clickfs_rootvnode(mp, &cmp->click_root);
    if (error < 0) {
	FREE(cmp, M_TEMP);
	return error;
    }

    return 0;
}

static int
clickfs_start(struct mount *mp, int flags, struct proc *p)
{
    return 0;
}

static int
clickfs_unmount(struct mount *mp, int mntflags, struct proc *p)
{
    struct clickfs_mount *cmp = (struct clickfs_mount *)mp->mnt_data;
    struct vnode *rootvp = cmp->click_root;
    int error;
    int flags = 0;

    if (mntflags & MNT_FORCE)
	flags |= FORCECLOSE;

    if (rootvp->v_usecount > 1)
	return EBUSY;

    error = vflush(mp, rootvp, flags);
    if (error)
	return error;

    vrele(rootvp);
    vgone(rootvp);

    free(mp->mnt_data, M_TEMP);
    mp->mnt_data = 0;

    return 0;
}

static int
clickfs_root(struct mount *mp, struct vnode **vpp)
{
    struct clickfs_mount *cmp = (struct clickfs_mount *)mp->mnt_data;

    *vpp = cmp->click_root;
    VREF(*vpp);
    vn_lock(*vpp, LK_EXCLUSIVE | LK_RETRY, curproc);

    return 0;
}

static int
clickfs_statfs(struct mount *mp, struct statfs *sbp, struct proc *p)
{
    memcpy(sbp, &mp->mnt_stat, sizeof(*sbp));
    return 0;
}

static int
clickfs_sync(struct mount *mp, int waitfor, struct ucred *cred,
	     struct proc *p)
{
    return 0;
}

static int
clickfs_init(struct vfsconf *vfsp)
{
    String::static_initialize();

    current_config = new String;
    return 0;
}

static int
clickfs_uninit(struct vfsconf *vfsp)
{
    delete current_config;
    current_config = 0;
    return 0;
}

struct vfsops clickfs_vfsops = {
    clickfs_mount,
    clickfs_start,
    clickfs_unmount,
    clickfs_root,
    vfs_stdquotactl,
    clickfs_statfs,
    clickfs_sync,
    vfs_stdvget,
    vfs_stdfhtovp,
    vfs_stdcheckexp,
    vfs_stdvptofh,
    clickfs_init,
    clickfs_uninit,
    vfs_stdextattrctl
};
