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
#include <click/straccum.hh>

#define UIO_MX		32

vop_t **clickfs_vnops;
enum clickfs_nodetype {
    CLICKFS_ROOT = 0,
    CLICKFS_CONFIG,
    CLICKFS_ELEMENT,
    CLICKFS_ELEMENT_SYMLINK
};

static char *clickfs_root_fixedfiles[] = {
    ".",
    "..",
    "config",
    NULL
};

static enum vtype clickfs_vtype[] = {
    VDIR,	/* CLICKFS_ROOT */
    VREG,	/* CLICKFS_CONFIG */
    VDIR,	/* CLICKFS_ELEMENT */
    VLNK	/* CLICKFS_ELEMENT_SYMLINK */
};

struct clickfs_node {
    enum clickfs_nodetype type;		/* Type of this node */
    int idx;				/* Router element index */
};

String *current_config = 0;
static StringAccum *build_config = 0;

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
    cp->type = CLICKFS_ROOT;
    vp->v_data = cp;
    vp->v_type = VDIR;
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
    struct proc *p = cnp->cn_proc;
    struct clickfs_node *cp = (struct clickfs_node *) dvp->v_data;
    int error = 0;

    enum clickfs_nodetype newtype;
    int newidx;

    *vpp = NULLVP;

    if (dvp->v_type != VDIR)
	return ENOTDIR;
    if (cnp->cn_nameiop == DELETE || cnp->cn_nameiop == RENAME)
	return EROFS;
    VOP_UNLOCK(dvp, 0, p);

#ifdef CLICKFS_DEBUG
    {
	char *p;

	printf("lookup(");
	for (p=pname; *p; p++)
	    printf("%c", *p);
	printf(") on a %d type node\n", cp->type);
    }
#endif

    if (cnp->cn_namelen == 1 && *pname == '.') {
	*vpp = dvp;
	VREF(dvp);
	goto done;
    }

    if (cp->type == CLICKFS_ROOT) {
	if (cnp->cn_namelen == 6 && !strncmp(pname, "config", 6)) {
	    newtype = CLICKFS_CONFIG;
	    goto found;
	}

	/*
	 * See if this is a number -- if so, it's probably a router
	 * element.
	 */
	int all_numeric = 1;
	int nelements = current_router->nelements();

	for (int i=0; i<cnp->cn_namelen; i++)
	    if (!isdigit(pname[i])) all_numeric = 0;
	if (all_numeric) {
	    int idx = (int)strtol(pname, 0, 10);

	    if (idx > 0 && idx <= nelements) {
		newtype = CLICKFS_ELEMENT;
		newidx = idx;
		goto found;
	    }
	}

	/*
	 * Check if it's a name of an element -- which is actually
	 * a symlink to the numeric name.
	 */
	for (int i=0; i<nelements; i++) {
	    const String &id = current_router->element(i)->id();
	    const char *data = id.data();

	    if (cnp->cn_namelen == id.length() &&
			!strncmp(data, pname, cnp->cn_namelen)) {
		newtype = CLICKFS_ELEMENT_SYMLINK;
		newidx = i+1;
		goto found;
	    }
	}
    }

    error = (cnp->cn_nameiop == LOOKUP) ? ENOENT : EROFS;
    goto done;

found:
    error = getnewvnode(VT_NON, dvp->v_mount, clickfs_vnops, vpp);
    if (error)
	goto done;
    MALLOC((*vpp)->v_data, void *, sizeof(struct clickfs_node),
	   M_TEMP, M_WAITOK);
    cp = (struct clickfs_node *) (*vpp)->v_data;

    cp->type = newtype;
    cp->idx = newidx;

    (*vpp)->v_type = clickfs_vtype[cp->type];
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
    vap->va_mode = (vp->v_type == VREG) ? 0644 : 0755;
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

    if (cp->type == CLICKFS_CONFIG)
	return 0;
    return EOPNOTSUPP;
}

int
clickfs_reclaim(struct vop_reclaim_args *ap)
{
    struct vnode *vp = ap->a_vp;

    if (vp->v_data)
	free(vp->v_data, M_TEMP);

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
    int error, skip, filecnt, off;
    struct uio *uio = ap->a_uio;
    struct vnode *vp = ap->a_vp;
    struct clickfs_node *cp = (struct clickfs_node *)vp->v_data;
    char **entry_ptr;

    if (vp->v_type != VDIR)
	return ENOTDIR;

    off = (int) uio->uio_offset;
    if (off < 0 || off % UIO_MX != 0 || uio->uio_resid < UIO_MX)
	return EINVAL;

    skip = (u_int) off / UIO_MX;
    error = 0;

    entry_ptr = clickfs_root_fixedfiles;
    filecnt = 0;

    while (*entry_ptr && uio->uio_resid >= UIO_MX) {
	error = clickfs_int_send_dirent(*(entry_ptr++), &skip, &filecnt, uio);
	if (error) break;
    }

    if (current_router) {
	int nelements = current_router->nelements();
	int curelem;

	for (curelem=0; curelem<nelements && uio->uio_resid >= UIO_MX;
			curelem++) {
	    char buf[64];

	    sprintf(buf, "%d", curelem+1);
	    error = clickfs_int_send_dirent(buf, &skip, &filecnt, uio);
	    if (error) break;

	    const String &id = current_router->element(curelem)->id();
	    char *data = (char *) id.data();
	    strncpy(buf, data, sizeof(buf));
	    buf[sizeof(buf)-1] = '\0';
	    buf[id.length()] = '\0';
	    error = clickfs_int_send_dirent(buf, &skip, &filecnt, uio);
	    if (error) break;
	}
    }

    uio->uio_offset = filecnt * UIO_MX;

    return error;
}

int
clickfs_open(struct vop_open_args *ap)
{
    struct vnode *vp = ap->a_vp;
    struct clickfs_node *cp = (struct clickfs_node *) vp->v_data;
    int mode = ap->a_mode;

    if (cp->type == CLICKFS_CONFIG) {
	if (mode & FWRITE) {
	    if (!build_config)
		build_config = new StringAccum;
	    if (!build_config)
		return ENOMEM;
	    build_config->clear();
	    if (!build_config->reserve(1024))
		return ENOMEM;
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
    int off = uio->uio_offset;
    int error;

    if (vp->v_type == VDIR)
	return EOPNOTSUPP;

    error = EOPNOTSUPP;
    if (cp->type == CLICKFS_CONFIG) {
	error = 0;
	int len;

	if (!current_config)
	    return 0;

	len = current_config->length();
	if (off > len)
	    return 0;
	error = uiomove((char *)current_config->data() + off, len - off, uio);
    }

    return error;
}

int
clickfs_write(struct vop_write_args *ap)
{
    struct vnode *vp = ap->a_vp;
    struct uio *uio = ap->a_uio;
    struct clickfs_node *cp = (struct clickfs_node *) vp->v_data;
    int off = uio->uio_offset;
    int len = uio->uio_resid;

    if (!build_config)
	return ENOMEM;

    int last_len = build_config->length();
    int end_pos = off + len;
    if (end_pos > last_len && !build_config->extend(end_pos - last_len))
	return ENOMEM;

    char *x = build_config->data() + off;
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

    if (cp->type == CLICKFS_CONFIG && (flags & FWRITE)) {
	*current_config = build_config->take_string();
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

    return 0;
}

int
clickfs_readlink(struct vop_readlink_args *ap)
{
    struct vnode *vp = ap->a_vp;
    struct clickfs_node *cp = (struct clickfs_node *) vp->v_data;
    struct uio *uio = ap->a_uio;

    if (cp->type == CLICKFS_ELEMENT_SYMLINK) {
	int len;
	char buf[64];

	len = sprintf(buf, "%d", cp->idx);
	return uiomove(buf, len, uio);
    }

    return EINVAL;
}

int
clickfs_default(struct vop_generic_args *ap)
{
    int ret = vop_defaultop(ap);
#ifdef CLICKFS_DEBUG
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
