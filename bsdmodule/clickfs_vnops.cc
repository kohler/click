/*
 * clickfs_vnops.cc -- Click configuration filesystem for BSD
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
#include <sys/namei.h>
#include <sys/vnode.h>
#include <sys/dir.h>
#include <sys/dirent.h>
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
    String *rwbuf;
};

String *current_config = 0;

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

    cp = (struct clickfs_node *) malloc(sizeof(*cp), M_TEMP, M_WAITOK);
    cp->dirent = clickfs_tree_root;
    cp->rwbuf = NULL;
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
    struct clickfs_node *cp = (struct clickfs_node *) dvp->v_data;
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
    MALLOC((*vpp)->v_data, void *, sizeof(struct clickfs_node),
	   M_TEMP, M_WAITOK);
    cp = (struct clickfs_node *) (*vpp)->v_data;

    cp->dirent = cde;
    cp->rwbuf = NULL;
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
    struct clickfs_node *cp = (struct clickfs_node *)vp->v_data;
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
    struct clickfs_node *cp = (struct clickfs_node *)vp->v_data;
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
    struct clickfs_node *cp = (struct clickfs_node *)vp->v_data;

    if (cp) {
	if(cp->rwbuf)
	    delete cp->rwbuf;
	free(cp, M_TEMP);
    }

    return 0;
}

int
clickfs_inactive(struct vop_inactive_args *ap)
{
    struct vnode *vp = ap->a_vp;

    VOP_UNLOCK(vp, 0, ap->a_p);
    vp->v_type = VNON;

    return 0;
}

int
clickfs_access(struct vop_access_args *ap)
{
    struct vnode *vp = ap->a_vp;
    struct ucred *cred = ap->a_cred;
    mode_t amode = ap->a_mode;

    /* For now, root can do anything. */
    if (cred->cr_uid == 0)
	return 0;

    return EPERM;
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
    struct clickfs_node *cp = (struct clickfs_node *)vp->v_data;
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

    if (!error) error = clickfs_int_send_dirent(".", &skip, &filecnt, uio);
    if (!error) error = clickfs_int_send_dirent("..", &skip, &filecnt, uio);

    dp = (struct clickfs_dir *)cp->dirent->param;
    de = dp->ent_head;
    while (de && !error) {
	error = clickfs_int_send_dirent(de->name, &skip, &filecnt, uio);
	de = de->next;
    }
 
    uio->uio_offset = filecnt * UIO_MX;

    return error;
}

static const Router::Handler *
find_handler(int eindex, int handlerno)
{
    if (current_router && current_router->handler_ok(handlerno))
	return &current_router->handler(handlerno);
    if (Router::global_handler_ok(handlerno))
	return &Router::global_handler(handlerno);
    return 0;
}

static const Router::Handler *
clickfs_int_get_handler(struct clickfs_node *cp)
{
    int *handler_params = (int *) cp->dirent->param;
    int eindex = handler_params[1];
    const Router::Handler *h = find_handler(eindex, handler_params[0]);

    return h;
}

static Element *
clickfs_int_get_element(struct clickfs_node *cp)
{
    int *handler_params = (int *) cp->dirent->param;
    int eindex = handler_params[1];
    Element *e = eindex >= 0 ? current_router->element(eindex) : 0;

    return e;
}

int
clickfs_open(struct vop_open_args *ap)
{
    struct vnode *vp = ap->a_vp;
    struct clickfs_node *cp = (struct clickfs_node *) vp->v_data;
    int mode = ap->a_mode;

    if (cp->dirent->type == CLICKFS_DIRENT_CONFIG) {
	if (mode & FWRITE) {
	    cp->rwbuf = new String();
	    if (!cp->rwbuf)
		return ENOMEM;
	}
	if (mode & FREAD) {
	    cp->rwbuf = current_config;
	}
	return 0;
    }

    if (cp->dirent->type == CLICKFS_DIRENT_EHANDLE) {
	Element *e = clickfs_int_get_element(cp);
	const Router::Handler *h = clickfs_int_get_handler(cp);

	if (!h) return ENOENT;

	if (mode & FWRITE) {
	    if (!h->write_visible()) return EPERM;
	    cp->rwbuf = new String();
	    if (!cp->rwbuf) return ENOMEM;
	}
	if (mode & FREAD) {
	    if (!h->read_visible()) return EPERM;
	    String *s = new String();
	    if (!s) return ENOMEM;
	    *s = h->call_read(e);
	    cp->rwbuf = s;
	}
	return 0;
    }

    if (mode & FWRITE)
	return EROFS;
    return 0;
}

int
clickfs_read(struct vop_read_args *ap)
{
    struct vnode *vp = ap->a_vp;
    struct uio *uio = ap->a_uio;
    struct clickfs_node *cp = (struct clickfs_node *) vp->v_data;
    int len, off = uio->uio_offset;
    String *read_str = cp->rwbuf;

    if (vp->v_type != VREG)
	return EOPNOTSUPP;
    if (!read_str)
	return 0;

    len = read_str->length();
    if (off > len)
	return 0;
    return uiomove((char *)read_str->data() + off, len - off, uio);
}

int
clickfs_write(struct vop_write_args *ap)
{
    struct vnode *vp = ap->a_vp;
    struct uio *uio = ap->a_uio;
    struct clickfs_node *cp = (struct clickfs_node *) vp->v_data;
    int off = uio->uio_offset;
    int len = uio->uio_resid;
    String *write_str = cp->rwbuf;

    if (!write_str)
	return ENOMEM;

    int last_len = write_str->length();
    int end_pos = off + len;
    if (end_pos > last_len)
	write_str->append_fill(0, end_pos - last_len);

    char *x = write_str->mutable_data() + off;
    if (end_pos > last_len)
	memset(x, 0, end_pos - last_len);
    return uiomove(x, len, uio);
}

int
clickfs_close(struct vop_close_args *ap)
{
    struct vnode *vp = ap->a_vp;
    struct clickfs_node *cp = (struct clickfs_node *) vp->v_data;
    int flags = ap->a_fflag;

    if (cp->dirent->type == CLICKFS_DIRENT_CONFIG && (flags & FWRITE)) {
	*current_config = *cp->rwbuf;
	cp->rwbuf = NULL;
	kill_current_router();

	Router *r = parse_router(*current_config);
	if (r) {
	    r->preinitialize();
	    r->initialize(kernel_errh);
	    install_current_router(r);
	    return r->initialized() ? 0 : EINVAL;
	} else {
	    return EINVAL;
	}
    }

    if (cp->dirent->type == CLICKFS_DIRENT_EHANDLE && (flags & FWRITE)) {
	Element *e = clickfs_int_get_element(cp);
	const Router::Handler *h = clickfs_int_get_handler(cp);

	String context_string = "In write handler `" + h->name() + "'";
	if (e) context_string += String(" for `") + e->declaration() + "'";
	ContextErrorHandler cerrh(kernel_errh, context_string + ":");

	int result = h->call_write(*cp->rwbuf, e, &cerrh);
	if (result < 0)
	    return EINVAL;
    }

    return 0;
}

int
clickfs_readlink(struct vop_readlink_args *ap)
{
    struct vnode *vp = ap->a_vp;
    struct clickfs_node *cp = (struct clickfs_node *) vp->v_data;
    struct uio *uio = ap->a_uio;

    if (cp->dirent->type == CLICKFS_DIRENT_SYMLINK)
	return uiomove(cp->dirent->lnk_name,
		       strlen(cp->dirent->lnk_name), uio);

    return EINVAL;
}

int
clickfs_default(struct vop_generic_args *ap)
{
    int ret = vop_defaultop(ap);
#if CLICKFS_DEBUG_DEF
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
    { &vop_open_desc,			(vop_t *) clickfs_open		},
    { &vop_read_desc,			(vop_t *) clickfs_read		},
    { &vop_write_desc,			(vop_t *) clickfs_write		},
    { &vop_close_desc,			(vop_t *) clickfs_close		},
    { &vop_readlink_desc,		(vop_t *) clickfs_readlink	},
    { (struct vnodeop_desc *) NULL,	(int (*) (void *)) NULL		}
};

struct vnodeopv_desc clickfs_vnodeop_opv_desc =
{ &clickfs_vnops, clickfs_root_vnop_entries };
