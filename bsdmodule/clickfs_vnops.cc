/* -*- c-basic-offset: 4 -*- */
/*
 * clickfs_vnops.cc -- Click configuration filesystem for BSD
 * Nickolai Zeldovich, Luigi Rizzo, Eddie Kohler
 *
 * Copyright (c) 2001 Massachusetts Institute of Technology
 * Copyright (c) 2001-2002 International Computer Science Institute
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
#include <sys/dir.h>
#include <sys/dirent.h>
#include <sys/stat.h>
#include <sys/uio.h>
CLICK_CXX_UNPROTECT
#include <click/cxxunprotect.h>

#include <click/string.hh>
#include <click/error.hh>

#define UIO_MX			32
#define	CLICKFS_DEBUG		0
#define	CLICKFS_DEBUG_DEF	0

vop_t **clickfs_vnops;

static enum vtype clickfs_vtype[] = {
    VDIR,		/* CLICKFS_DIRENT_DIR */
    VREG,		/* CLICKFS_DIRENT_EHANDLE */
    VLNK,		/* CLICKFS_DIRENT_ELINK */
    VREG		/* CLICKFS_DIRENT_CONFIG */
};

struct clickfs_node {
    struct clickfs_dirent *dirent;	/* Directory tree node */
    String	*wbuf;
    off_t	w_offset;
    String	*rbuf;
    off_t	r_offset;
    off_t	next_read;		/* pos. of next read */
};

#define	VTON(vn)	((struct clickfs_node *)(vn)->v_data)

static struct clickfs_node *
new_clickfs_node()
{
    struct clickfs_node *cp;
    cp = (struct clickfs_node *) malloc(sizeof(*cp), M_TEMP, M_WAITOK);
    if (cp) {
	cp->dirent = NULL;
	cp->wbuf = cp->rbuf = NULL;
	cp->w_offset = cp->r_offset = 0;
    }
    return cp;
}

int
clickfs_rootvnode(struct mount *mp, struct vnode **vpp)
{
    int ret;
    struct vnode *vp;
    struct clickfs_node *cp;

    ret = getnewvnode(VT_NON, mp, clickfs_vnops, vpp);
    if (ret)
	return ret;
    vp = *vpp;

    cp = new_clickfs_node();
    cp->dirent = clickfs_tree_root;
    clickfs_tree_ref_dirent(cp->dirent);

    vp->v_data = cp;
    vp->v_type = clickfs_vtype[cp->dirent->type];
    vp->v_flag = VROOT;
    return 0;
}

int
clickfs_lookup(struct vop_lookup_args *ap)
{
    struct componentname *cnp = ap->a_cnp;
    struct vnode **vpp = ap->a_vpp;
    struct vnode *dvp = ap->a_dvp;
    char *pname = cnp->cn_nameptr;
    int plen = cnp->cn_namelen;
    struct proc *p = cnp->cn_proc;
    struct clickfs_node *cp = VTON(dvp);
    struct clickfs_dir *cdp;
    struct clickfs_dirent *cde;
    int error = 0;

    *vpp = NULLVP;

    if (dvp->v_type != VDIR)
	return ENOTDIR;
    if (cnp->cn_nameiop == DELETE || cnp->cn_nameiop == RENAME)
	return EROFS;
    VOP_UNLOCK(dvp, 0, p);

#if CLICKFS_DEBUG
    {
	char *p;
	int i;

	printf("lookup(");
	for (p=pname, i=0; *p && i<plen; p++, i++)
	    printf("%c", *p);
	printf(") on a %d type node\n", cp->dirent->type);
    }
#endif

    if (plen == 1 && *pname == '.') {
	*vpp = dvp;
	VREF(dvp);
	goto done;
    }

    cdp = (struct clickfs_dir *)cp->dirent->param;
    cde = cdp->ent_head;
    while (cde) {
	if (plen == strlen(cde->name) &&
	    !strncmp(pname, cde->name, plen)) break;

	cde = cde->next;
    }

    if (!cde) {
	error = (cnp->cn_nameiop == LOOKUP) ? ENOENT : EROFS;
	goto done;
    }

    error = getnewvnode(VT_NON, dvp->v_mount, clickfs_vnops, vpp);
    if (error)
	goto done;
    (*vpp)->v_data = cp = new_clickfs_node();

    clickfs_tree_ref_dirent(cde);
    cp->dirent = cde;
    (*vpp)->v_type = clickfs_vtype[cp->dirent->type];
    vn_lock(*vpp, LK_SHARED | LK_RETRY, p);
    return 0;

done:
    vn_lock(dvp, LK_SHARED | LK_RETRY, p);
    return error;
}

int
clickfs_getattr(struct vop_getattr_args *ap)
{
    struct vnode *vp = ap->a_vp;
    struct clickfs_node *cp = VTON(vp);
    struct vattr *vap = ap->a_vap;

    bzero(vap, sizeof(*vap));
    vap->va_type = vp->v_type;
    vap->va_mode = cp->dirent->perm;
    vap->va_nlink = 1;
    vap->va_fsid = VNOVAL;
    vap->va_fileid = VNOVAL;
    vap->va_blocksize = 1024;

    return 0;
}

int
clickfs_setattr(struct vop_setattr_args *ap)
{
    struct vnode *vp = ap->a_vp;
    struct clickfs_node *cp = VTON(vp);
    struct vattr *vap = ap->a_vap;

    /*
     * This doesn't do anything, so we just pretend that it worked.
     */
    return 0;
}

int
clickfs_reclaim(struct vop_reclaim_args *ap)
{
    struct vnode *vp = ap->a_vp;
    struct clickfs_node *cp = VTON(vp);

    if (cp) {
	if (cp->wbuf)
	    delete cp->wbuf;
	if (cp->rbuf)
	    delete cp->rbuf;
	if (cp->dirent)
	    clickfs_tree_put_dirent(cp->dirent);
	free(cp, M_TEMP);
    }
    vp->v_data = NULL;

    return 0;
}

int
clickfs_inactive(struct vop_inactive_args *ap)
{
    struct vnode *vp = ap->a_vp;
    struct clickfs_node *cp = VTON(vp);

    if (cp) {
	if (cp->dirent) {
	    clickfs_tree_put_dirent(cp->dirent);
	    cp->dirent = NULL;
	}
    }

    vp->v_type = VNON;
    VOP_UNLOCK(vp, 0, ap->a_p);

    return 0;
}

int
clickfs_access(struct vop_access_args *ap)
{
    struct ucred *cred = ap->a_cred;
    mode_t mode = ap->a_mode;
    struct clickfs_node *cp = VTON(ap->a_vp);
    int perm = cp->dirent->perm;

printf("access: mode 0x%08x perm 0x%08x VR %d RD %d\n",
	mode, perm, VREAD, VWRITE);
    /* XXX fixme: also allow others, not just root */
    if (cred->cr_uid != 0)
	return EPERM;
    if ( (mode & VWRITE) && !(perm & S_IWUSR) )
	return EPERM;
    if ( (mode & VREAD)  && !(perm & S_IRUSR) )
	return EPERM;
    return 0;
}

static int
clickfs_int_send_dirent(char *name, int *skip, int *filecnt, struct uio *uio)
{
    struct dirent d;

    bzero((caddr_t) &d, UIO_MX);
    d.d_namlen = strlen(name);
    bcopy(name, d.d_name, d.d_namlen+1);
    d.d_reclen = UIO_MX;
    d.d_fileno = ((*filecnt)++) + 3;
    d.d_type = DT_UNKNOWN;

    if (*skip > 0) {
	(*skip)--;
	return 0;
    }

    return uiomove((caddr_t) &d, UIO_MX, uio);
}

int
clickfs_readdir(struct vop_readdir_args *ap)
{
    int skip, filecnt, off, error = 0;
    struct uio *uio = ap->a_uio;
    struct vnode *vp = ap->a_vp;
    struct clickfs_node *cp = VTON(vp);
    struct clickfs_dir *dp;
    struct clickfs_dirent *de;

#if CLICKFS_DEBUG
    printf("clickfs_readdir(type=%d)\n", cp->dirent->type);
#endif

    if (vp->v_type != VDIR)
	return ENOTDIR;

    off = (int) uio->uio_offset;
    if (off < 0 || off % UIO_MX != 0 || uio->uio_resid < UIO_MX)
	return EINVAL;

    skip = (u_int) off / UIO_MX;
    filecnt = 0;

    if (!error)
	error = clickfs_int_send_dirent(".", &skip, &filecnt, uio);
#if 0 /* XXX check this, it does not work! */
    if (!error)
	error = clickfs_int_send_dirent("..", &skip, &filecnt, uio);
#endif
    dp = (struct clickfs_dir *)cp->dirent->param;
    de = dp->ent_head;
    while (de && !error) {
	error = clickfs_int_send_dirent(de->name, &skip, &filecnt, uio);
	de = de->next;
    }
 
    uio->uio_offset = filecnt * UIO_MX;

    return error;
}

const Router::Handler *
clickfs_int_get_handler(struct clickfs_node *cp)
{
    printf("clickfs_int_get_handler 1\n");
    if (cp->dirent == NULL) {
	printf("clickfs_int_get_handler: null dirent\n");
	return NULL;
    }
    if (cp->dirent->param == NULL) {
	printf("clickfs_int_get_handler: null param\n");
	return NULL;
    }
    int *handler_params = (int *) cp->dirent->param;
    int eindex = handler_params[1];
    const Router::Handler *h = Router::handlerp(click_router, handler_params[0]);

    return h;
}

static Element *
clickfs_int_get_element(struct clickfs_node *cp)
{
    int *handler_params = (int *) cp->dirent->param;
    int eindex = handler_params[1];
    Element *e = eindex >= 0 ? click_router->element(eindex) : 0;

    return e;
}

#if 0
int
clickfs_open(struct vop_open_args *ap)
{
    struct vnode *vp = ap->a_vp;
    struct clickfs_node *cp = VTON(vp);
    cp->mode = ap->a_mode;

printf("file has perm 0x%08x opened in mode 0x%08x\n",
	cp->dirent->perm, ap->a_mode);

    if (cp->dirent->type == CLICKFS_DIRENT_CONFIG) {
	/* nothing to do here */
    } else if (cp->dirent->type == CLICKFS_DIRENT_EHANDLE) {
	Element *e = clickfs_int_get_element(cp);
	const Router::Handler *h = clickfs_int_get_handler(cp);

	if (!h)
	    return ENOENT;
    }
    return 0;
}
#endif

int
clickfs_read(struct vop_read_args *ap)
{
    struct clickfs_node *cp = VTON(ap->a_vp);
    struct uio *uio = ap->a_uio;
    int len, off;

    /*
     * can only read from regular files
     */
    if (ap->a_vp->v_type != VREG)
	return EOPNOTSUPP;
    off = uio->uio_offset - cp->r_offset;
    if (off < 0) {
	/*
	 * seek back. Free the old string and reload.
	 */
	if (cp->rbuf) {
	    delete cp->rbuf;
	    cp->rbuf = NULL;
	}
	cp->r_offset = uio->uio_offset;
    }
    if (!cp->rbuf) {   /* try to read */
	if (cp->dirent->type == CLICKFS_DIRENT_CONFIG) {
	    int h = Router::find_global_handler("config");
	    cp->rbuf = new String(Router::global_handler(h).call_read(0));
	} else if (cp->dirent->type == CLICKFS_DIRENT_EHANDLE) {
	    Element *e = clickfs_int_get_element(cp);
	    const Router::Handler *h = clickfs_int_get_handler(cp);
	    if (!h)
		return ENOENT;
	    if (!h->read_visible())
		return EPERM;
	    cp->rbuf = new String(h->call_read(e));
	}
    }
    if (!cp->rbuf || cp->rbuf->out_of_memory()) {
	delete cp->rbuf;
	cp->rbuf = 0;
	return ENOMEM;
    }

    len = cp->rbuf->length();
    if (off >= len) {
	/*
	 * no more data. Return 0, but get rid of the string
	 * so future reads will refresh the data.
	 */
	if (cp->dirent->type == CLICKFS_DIRENT_EHANDLE) {
	    cp->r_offset += len ;
	    delete cp->rbuf;
	    cp->rbuf = NULL;
	}
	return 0;
    }
    len = uiomove((char *)cp->rbuf->data() + off, len - off, uio);
    return len;
}

int
clickfs_write(struct vop_write_args *ap)
{
    struct clickfs_node *cp = VTON(ap->a_vp);
    struct uio *uio = ap->a_uio;
    int off = uio->uio_offset - cp->w_offset;
    int len = uio->uio_resid;

    printf("clickfs_write: 1\n");
    if (cp->wbuf == NULL) {
	if (cp->dirent->type == CLICKFS_DIRENT_EHANDLE) {
	    const Router::Handler *h = clickfs_int_get_handler(cp);

	    if (!h)
		return ENOENT;
	    if (!h->write_visible())
		return EPERM;
	}
	cp->wbuf = new String();
	if (cp->wbuf == NULL)
	    return ENOMEM;
    }
    printf("clickfs_write: 3\n");

    int last_len = cp->wbuf->length();
    int end_pos = off + len;
    if (end_pos > last_len)
	cp->wbuf->append_fill(0, end_pos - last_len);

    char *x = cp->wbuf->mutable_data() + off;
    if (end_pos > last_len)
	memset(x, 0, end_pos - last_len);
    return uiomove(x, len, uio);
}

int
clickfs_fsync_body(struct clickfs_node *cp)
{
    int retval = 0;

    printf("clickfs_fsync_body: 1\n");
    if (cp->dirent->type == CLICKFS_DIRENT_CONFIG) {
	/*
	 * config is special. If reading, do not write,
	 */
	if (cp->rbuf)
	    return 0;
	if (cp->wbuf == NULL)
	    cp->wbuf = new String("");

	int h = Router::find_global_handler("config");
	retval = Router::global_handler(h).call_write(*cp->wbuf, 0, click_logged_errh);
	retval = (retval >= 0 ? 0 : -retval);

	printf("clickfs_fsync_body: 2\n");
    } else if (cp->dirent->type == CLICKFS_DIRENT_EHANDLE) {
	const Router::Handler *h = clickfs_int_get_handler(cp);
 
	if (cp->rbuf == NULL && cp->wbuf == NULL) {
	    // empty write, prepare something.
	    cp->wbuf = new String("");
	}
	if (!h || !h->writable())
	    retval = EINVAL;
	else if (cp->wbuf != NULL) {
	    Element *e = clickfs_int_get_element(cp);
	    String context_string = "In write handler `" + h->name() + "'";
	    if (e)
		context_string += String(" for `") + e->declaration() + "'";
	    ContextErrorHandler cerrh(click_logged_errh, context_string + ":");

	    retval = h->call_write(*cp->wbuf, e, &cerrh);
	    retval = (retval >= 0 ? 0 : -retval);
	}
	/*
	 * now dispose read buffer if any
	 */
	if (cp->rbuf) {
	    delete cp->rbuf;
	    cp->rbuf = NULL;
	}
    }
    /*
     * skip current buffer
     */
    if (cp->wbuf) {
	cp->w_offset += cp->wbuf->length();
	delete cp->wbuf;
	cp->wbuf = NULL;
    }

    return retval;
}

int
clickfs_close(struct vop_close_args *ap)
{
    struct clickfs_node *cp = VTON(ap->a_vp);
    int flags = ap->a_fflag;
    int retval = 0;

    printf("clickfs_close(): %s node %p\n",
	ap->a_desc->vdesc_name, cp);

    if (flags & FWRITE)
	retval = clickfs_fsync_body(cp);
    return retval;
}


int
clickfs_readlink(struct vop_readlink_args *ap)
{
    struct clickfs_node *cp = VTON(ap->a_vp);

    if (cp->dirent->type == CLICKFS_DIRENT_SYMLINK)
	return uiomove(cp->dirent->lnk_name,
		       strlen(cp->dirent->lnk_name), ap->a_uio);

    return EINVAL;
}

int
clickfs_ioctl(struct vop_ioctl_args *ap)
{   
    struct clickfs_node *cp = VTON(ap->a_vp);

    printf("clickfs_ioctl(): %s node %p\n",
	    ap->a_desc->vdesc_name, cp);
    return EINVAL;
}

int
clickfs_fsync(struct vop_fsync_args *ap)
{   
    struct clickfs_node *cp = VTON(ap->a_vp);
    int retval = 0;

    printf("clickfs_fsync(): %s node %p\n",
	    ap->a_desc->vdesc_name, cp);
    retval = clickfs_fsync_body(cp);
    return retval;
}

int
clickfs_default(struct vop_generic_args *ap)
{
    int ret = vop_defaultop(ap);
#if 0
    printf("clickfs_default: %s() = %d\n", ap->a_desc->vdesc_name, ret);
#endif
    return ret;
}

static struct vnodeopv_entry_desc clickfs_root_vnop_entries[] =
{
    { &vop_default_desc,		(vop_t *) clickfs_default	},
    { &vop_lookup_desc,			(vop_t *) clickfs_lookup	},
    { &vop_getattr_desc,		(vop_t *) clickfs_getattr	},
    { &vop_setattr_desc,		(vop_t *) clickfs_setattr	},
    { &vop_reclaim_desc,		(vop_t *) clickfs_reclaim	},
    { &vop_inactive_desc,		(vop_t *) clickfs_inactive	},
    { &vop_access_desc,			(vop_t *) clickfs_access	},
    { &vop_readdir_desc,		(vop_t *) clickfs_readdir	},
//  { &vop_open_desc,			(vop_t *) clickfs_open		},
    { &vop_read_desc,			(vop_t *) clickfs_read		},
    { &vop_write_desc,			(vop_t *) clickfs_write		},
    { &vop_close_desc,			(vop_t *) clickfs_close		},
    { &vop_ioctl_desc,			(vop_t *) clickfs_ioctl		},
    { &vop_fsync_desc,			(vop_t *) clickfs_fsync		},
    { &vop_readlink_desc,		(vop_t *) clickfs_readlink	},
    { (struct vnodeop_desc *) NULL,	(int (*) (void *)) NULL		}
};

struct vnodeopv_desc clickfs_vnodeop_opv_desc =
{ &clickfs_vnops, clickfs_root_vnop_entries };
