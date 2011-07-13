/* -*- c-basic-offset: 4 -*- */
/*
 * clickfs_vnops.cc -- Click configuration filesystem for BSD
 * Nickolai Zeldovich, Luigi Rizzo, Eddie Kohler, Marko Zec
 *
 * Copyright (c) 2001 Massachusetts Institute of Technology
 * Copyright (c) 2001-2004 International Computer Science Institute
 * Copyright (c) 2004 University of Zagreb
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
#include <sys/namei.h>
#include <sys/dirent.h>
#include <sys/stat.h>
#include <sys/uio.h>
#include <sys/vnode.h>
CLICK_CXX_UNPROTECT
#include <click/cxxunprotect.h>

#include <click/string.hh>
#include <click/error.hh>
CLICK_USING_DECLS

#define UIO_MX			32

static int clickfs_lookup(struct vop_lookup_args *ap);
static int clickfs_open(struct vop_open_args *ap);
static int clickfs_close(struct vop_close_args *ap);
static int clickfs_access(struct vop_access_args *ap);
static int clickfs_getattr(struct vop_getattr_args *ap);
static int clickfs_setattr(struct vop_setattr_args *ap);
static int clickfs_read(struct vop_read_args *ap);
static int clickfs_write(struct vop_write_args *ap);
static int clickfs_fsync(struct vop_fsync_args *ap);
static int clickfs_readdir(struct vop_readdir_args *ap);
static int clickfs_readlink(struct vop_readlink_args *ap);
static int clickfs_inactive(struct vop_inactive_args *ap);
static int clickfs_reclaim(struct vop_reclaim_args *ap);

/* XXX: Blatant kludge as c++ does not like c99 initializers. */
struct vop_vector clickfs_vnodeops = {
	&default_vnodeops,
	NULL,
	NULL,
	vfs_cache_lookup,
	clickfs_lookup,
	NULL,
	NULL,
	NULL,
	clickfs_open,
	clickfs_close,
	clickfs_access,
#if __FreeBSD_version >= 800000
	NULL, /* accessx */
#endif
	clickfs_getattr,
	clickfs_setattr,
#if __FreeBSD_version >= 800000
	NULL, /* markatime */
#endif
	clickfs_read,
	clickfs_write,
#if __FreeBSD_version < 800000
	NULL, /* lease */
#endif
	NULL,
	NULL,
	NULL,
	NULL,
	clickfs_fsync,
	NULL,
	NULL,
	NULL,
	NULL,
	NULL,
	NULL,
	clickfs_readdir,
	clickfs_readlink,
	clickfs_inactive,
	clickfs_reclaim,
	NULL, NULL, NULL, NULL,  NULL, NULL, NULL, NULL,
	NULL, NULL, NULL, NULL,  NULL, NULL, NULL, NULL,
	NULL, NULL, NULL, NULL,  NULL, NULL, NULL, /* lock1, ..., vptofh */
#if __FreeBSD_version >= 800000
	NULL, /* vptocnp */
#endif
};

static enum vtype clickfs_vtype[] = {
    VDIR,		/* CLICKFS_DIRENT_DIR */
    VREG,		/* CLICKFS_DIRENT_HANDLE */
    VLNK,		/* CLICKFS_DIRENT_SYMLINK */
};

#define	VTOCDE(vn)	((struct clickfs_dirent *)(vn)->v_data)

int
clickfs_rootvnode(struct mount *mp, struct vnode **vpp)
{
    struct vnode *vp;
    struct clickfs_dirent *de;
    int error;

    error = getnewvnode("click", mp, &clickfs_vnodeops, vpp);
    if (error)
	return error;
    de = clickfs_tree_root;

    vp = *vpp;
    vp->v_data = de;
    vp->v_type = clickfs_vtype[de->type];
    vp->v_vflag = VV_ROOT;
    insmntque(*vpp, mp);
    return 0;
}

static int
clickfs_lookup(struct vop_lookup_args *ap)
{
    struct componentname *cnp = ap->a_cnp;
    struct vnode **vpp = ap->a_vpp;
    struct vnode *dvp = ap->a_dvp;
    char *pname = cnp->cn_nameptr;
    int plen = cnp->cn_namelen;
    struct thread *td = cnp->cn_thread;
    struct clickfs_dirent *cde = VTOCDE(dvp);
    int error = 0;

    *vpp = NULLVP;

    if (dvp->v_type != VDIR)
	return ENOTDIR;
    if (cnp->cn_nameiop == DELETE || cnp->cn_nameiop == RENAME)
	return EROFS;

    if (plen == 1 && *pname == '.') {
	*vpp = dvp;
	VREF(dvp);
	return (0);
    }

    if (cnp->cn_flags & ISDOTDOT)
	cde = cde->data.dir.parent;
    else
	for (cde = cde->data.dir.head; cde; cde = cde->next)
	    if (plen == strlen(cde->name) &&
		!strncmp(pname, cde->name, plen))
		break;

    if (!cde) {
	error = (cnp->cn_nameiop == LOOKUP) ? ENOENT : EROFS;
	goto done;
    }

    error = getnewvnode("click", dvp->v_mount, &clickfs_vnodeops, vpp);
    if (error)
	goto done;

    (*vpp)->v_data = cde;
    (*vpp)->v_type = clickfs_vtype[cde->type];
    insmntque(*vpp, dvp->v_mount);
    if (cde == clickfs_tree_root)
	(*vpp)->v_vflag = VV_ROOT;
#if __FreeBSD_version >= 800000
    vn_lock(*vpp, LK_EXCLUSIVE | LK_RETRY);
#else
    vn_lock(*vpp, LK_SHARED | LK_RETRY, td);
#endif

    return 0;

done:
    return error;
}

static int
clickfs_getattr(struct vop_getattr_args *ap)
{
    struct vnode *vp = ap->a_vp;
    struct vattr *vap = ap->a_vap;
    struct clickfs_dirent *cde = VTOCDE(vp);

    VATTR_NULL(vap);
    vap->va_type = vp->v_type;
    vap->va_mode = cde->perm;
    vap->va_fileid = cde->fileno;
    vap->va_nlink = cde->file_refcnt;
    vap->va_flags = 0;
    vap->va_blocksize = PAGE_SIZE;
    vap->va_fsid = vp->v_mount->mnt_stat.f_fsid.val[0];
    nanotime(&vap->va_ctime);
    vap->va_atime = vap->va_mtime = vap->va_ctime;
    vap->va_uid = vap->va_gid = 0;
    switch (cde->type) {
	case CLICKFS_DIRENT_DIR:
	    vap->va_bytes = vap->va_size = (cde->file_refcnt-2)*sizeof(*cde);
	    break;
	case CLICKFS_DIRENT_HANDLE:
	    vap->va_bytes = vap->va_size = 0;
	    break;
	case CLICKFS_DIRENT_SYMLINK:
	    vap->va_bytes = vap->va_size = strlen(cde->data.slink.name);
	    break;
    }

    return 0;
}

static int
clickfs_setattr(struct vop_setattr_args *ap)
{
    /*
     * This doesn't do anything, so we just pretend that it worked.
     */
    return 0;
}

static int
clickfs_access(struct vop_access_args *ap)
{
    struct vnode *vp = ap->a_vp;
    struct ucred *cred = ap->a_cred;
#if __FreeBSD_version >= 800000
    mode_t mode = ap->a_accmode;
#else
    mode_t mode = ap->a_mode;
#endif
    struct clickfs_dirent *cde = VTOCDE(vp);
    int perm = cde->perm;

    /* XXX fixme: also allow others, not just root */
    //if (cred->cr_uid != 0)
    //	return EPERM;
    if ( (mode & VWRITE) && !(perm & S_IWUSR) )
	return EPERM;
    if ( (mode & VREAD)  && !(perm & S_IRUSR) )
	return EPERM;
    return 0;
}

static int
clickfs_int_send_dirent(const char *name, int *skip, int fileno, struct uio *uio)
{
    struct dirent d;

    bzero((caddr_t) &d, sizeof(d));
    d.d_namlen = strlen(name);
    bcopy(name, d.d_name, d.d_namlen + 1);
    d.d_reclen = UIO_MX;
    d.d_fileno = fileno;
    d.d_type = DT_UNKNOWN;

    if (*skip > 0) {
	(*skip)--;
	return 0;
    }

    uio->uio_offset += UIO_MX;
    return uiomove((caddr_t) &d, UIO_MX, uio);
}

static int
clickfs_readdir(struct vop_readdir_args *ap)
{
    int skip, off, error = 0;
    struct uio *uio = ap->a_uio;
    struct vnode *vp = ap->a_vp;
    struct clickfs_dirent *cde = VTOCDE(vp);
    struct clickfs_dirent *de;

    if (vp->v_type != VDIR)
	return ENOTDIR;

    off = (int) uio->uio_offset;
    if (off < 0 || off % UIO_MX != 0 || uio->uio_resid < UIO_MX)
	return EINVAL;

    skip = (u_int) off / UIO_MX;

    de = cde->data.dir.head;
    error = clickfs_int_send_dirent(".", &skip, cde->fileno, uio);
    if (!error)
	error = clickfs_int_send_dirent("..", &skip, 2, uio);
    while (de && !error) {
	error = clickfs_int_send_dirent(de->name, &skip, de->fileno, uio);
	de = de->next;
    }

    return error;
}

const Handler *
clickfs_int_get_handler(struct clickfs_dirent *cde)
{
    int handle = cde->data.handle.handle;
    const Handler *h = Router::handler(click_router, handle);
    return h;
}

static Element *
clickfs_int_get_element(struct clickfs_dirent *cde)
{
    int eindex = cde->data.handle.eindex;
    Element *e = eindex >= 0 ? click_router->element(eindex) : 0;

    return e;
}

static int
clickfs_open(struct vop_open_args *ap)
{
    struct vnode *vp = ap->a_vp;
    struct clickfs_dirent *cde = VTOCDE(vp);

    if (cde->type == CLICKFS_DIRENT_HANDLE) {
	Element *e = clickfs_int_get_element(cde);
	const Handler *h = clickfs_int_get_handler(cde);

	if (!h)
	    return ENOENT;
    }
    return 0;
}

static int
clickfs_read(struct vop_read_args *ap)
{
    struct vnode *vp = ap->a_vp;
    struct clickfs_dirent *cde = VTOCDE(vp);
    struct uio *uio = ap->a_uio;
    int len, off;

    /*
     * can only read from regular files
     */
    if (ap->a_vp->v_type != VREG)
	return EOPNOTSUPP;

    off = uio->uio_offset - cde->data.handle.r_offset;
    if (off < 0) {
	/*
	 * seek back. Free the old string and reload.
	 */
	if (cde->data.handle.rbuf) {
	    delete cde->data.handle.rbuf;
	    cde->data.handle.rbuf = NULL;
	}
	cde->data.handle.r_offset = uio->uio_offset;
    }
    if (!cde->data.handle.rbuf) {   /* try to read */
	Element *e = clickfs_int_get_element(cde);
	const Handler *h = clickfs_int_get_handler(cde);
	if (!h)
	    return ENOENT;
	if (!h->read_visible())
	    return EPERM;
	cde->data.handle.rbuf = new String(h->call_read(e));
    }
    if (!cde->data.handle.rbuf || cde->data.handle.rbuf->out_of_memory()) {
	delete cde->data.handle.rbuf;
	cde->data.handle.rbuf = NULL;
	return ENOMEM;
    }

    len = cde->data.handle.rbuf->length();
    if (off >= len) {
	/*
	 * no more data. Return 0, but get rid of the string
	 * so future reads will refresh the data.
	 */
	cde->data.handle.r_offset += len ;
	delete cde->data.handle.rbuf;
	cde->data.handle.rbuf = NULL;
	return 0;
    }
    len = uiomove((char *)cde->data.handle.rbuf->data() + off, len - off, uio);
    return len;
}

static int
clickfs_write(struct vop_write_args *ap)
{
    struct vnode *vp = ap->a_vp;
    struct clickfs_dirent *cde = VTOCDE(vp);
    struct uio *uio = ap->a_uio;
    int off = uio->uio_offset - cde->data.handle.w_offset;
    int len = uio->uio_resid;

    /*
     * can only write to regular files
     */
    if (ap->a_vp->v_type != VREG)
	return EOPNOTSUPP;

    if (cde->data.handle.wbuf == NULL) {
	const Handler *h = clickfs_int_get_handler(cde);

	if (!h)
	    return ENOENT;
	if (!h->write_visible())
	    return EPERM;

	cde->data.handle.wbuf = new String();
	if (cde->data.handle.wbuf == NULL)
	    return ENOMEM;
    }

    int last_len = cde->data.handle.wbuf->length();
    int end_pos = off + len;
    if (end_pos > last_len)
	cde->data.handle.wbuf->append_fill(0, end_pos - last_len);

    char *x = cde->data.handle.wbuf->mutable_data() + off;
    if (end_pos > last_len)
	memset(x, 0, end_pos - last_len);
    return uiomove(x, len, uio);
}

static int
clickfs_fsync_body(struct clickfs_dirent *cde)
{
    int retval = 0;

    if (cde->type == CLICKFS_DIRENT_HANDLE) {
	const Handler *h = clickfs_int_get_handler(cde);

	if (cde->data.handle.rbuf == NULL && cde->data.handle.wbuf == NULL) {
	    // empty write, prepare something.
	    cde->data.handle.wbuf = new String("");
	}
	if (!h || !h->writable())
	    retval = EINVAL;
	else if (cde->data.handle.wbuf != NULL) {
	    Element *e = clickfs_int_get_element(cde);
	    const char *fmt;
	    if (e)
		fmt = "In write handler %<%s%> for %<%{element}%>:";
	    else
		fmt = "In write handler %<%s%>:";
	    ContextErrorHandler cerrh(click_logged_errh, fmt, h->name().c_str(), e);

	    retval = h->call_write(*cde->data.handle.wbuf, e, &cerrh);
	    retval = (retval >= 0 ? 0 : -retval);
	}
	/*
	 * now dispose read buffer if any
	 */
	if (cde->data.handle.rbuf) {
	    delete cde->data.handle.rbuf;
	    cde->data.handle.rbuf = NULL;
	}
	/*
	 * skip current buffer
	 */
	if (cde->data.handle.wbuf) {
	    cde->data.handle.w_offset += cde->data.handle.wbuf->length();
	    delete cde->data.handle.wbuf;
	    cde->data.handle.wbuf = NULL;
	}
    }

    return retval;
}

static int
clickfs_close(struct vop_close_args *ap)
{
    struct vnode *vp = ap->a_vp;
    struct clickfs_dirent *cde = VTOCDE(vp);
    int flags = ap->a_fflag;
    int retval = 0;

    if (flags & FWRITE)
	retval = clickfs_fsync_body(cde);

    return retval;
}

static int
clickfs_readlink(struct vop_readlink_args *ap)
{
    struct vnode *vp = ap->a_vp;
    struct clickfs_dirent *cde = VTOCDE(vp);

    if (cde->type != CLICKFS_DIRENT_SYMLINK)
	return EINVAL;

    return uiomove(cde->data.slink.name,
		   strlen(cde->data.slink.name), ap->a_uio);
}

static int
clickfs_fsync(struct vop_fsync_args *ap)
{
    struct vnode *vp = ap->a_vp;
    struct clickfs_dirent *cde = VTOCDE(vp);

    return(clickfs_fsync_body(cde));
}

static int
clickfs_inactive(struct vop_inactive_args *ap)
{
    struct vnode *vp = ap->a_vp;
    struct thread *td = ap->a_td;
    //struct clickfs_dirent *cde = VTOCDE(vp);

    /*
     * XXX: Recycling always is probably inefficient, but makes
     *      unmounting easier (and more robust).
     */
    //if (cde->file_refcnt == 0) {
    vrecycle(vp, td);

    return 0;
}

static int
clickfs_reclaim(struct vop_reclaim_args *ap)
{
    struct vnode *vp = ap->a_vp;

    vnode_destroy_vobject(vp);
    cache_purge(vp);
    vp->v_data = NULL;

   /* Dirents freed when unloading. */

   return 0;
}
