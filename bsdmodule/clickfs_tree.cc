/*
 * clickfs_tree.cc -- Click configuration filesystem for BSD
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

#include <click/string.hh>
#include <click/straccum.hh>

struct clickfs_dirent *clickfs_tree_root;

int
clickfs_tree_init()
{
    struct clickfs_dir *d;
    struct clickfs_dirent *de;

    MALLOC(d, struct clickfs_dir *, sizeof(*d), M_TEMP, M_WAITOK);
    d->ent_head = NULL;

    MALLOC(de, struct clickfs_dirent *, sizeof(*de), M_TEMP, M_WAITOK);
    de->type = CLICKFS_DIRENT_DIR;
    de->perm = 0755;
    *de->name = '\0';		/* The root doesn't have a useful name */
    de->param = d;
    de->next = NULL;
    de->refcnt = 1;
    clickfs_tree_root = de;

    clickfs_tree_add_file(d, "config", CLICKFS_DIRENT_CONFIG, 0644, 0);

    return 0;
}

struct clickfs_dir *
clickfs_tree_add_dir(struct clickfs_dir *d, char *name, int perm)
{
    struct clickfs_dir *nd;
    struct clickfs_dirent *de;

    /* Create a new directory.. */
    MALLOC(nd, struct clickfs_dir *, sizeof(*nd), M_TEMP, M_WAITOK);
    nd->ent_head = NULL;

    /* Create a new directory entry that will correspond to the new
     * directory, and add it to the parent directory.
     */
    MALLOC(de, struct clickfs_dirent *, sizeof(*de), M_TEMP, M_WAITOK);
    de->type = CLICKFS_DIRENT_DIR;
    strncpy(de->name, name, sizeof(de->name));
    de->perm = perm;
    de->param = nd;
    de->next = d->ent_head;
    de->refcnt = 1;
    d->ent_head = de;

    return nd;
}

int
clickfs_tree_add_file(struct clickfs_dir *d, char *name,
		      enum clickfs_dirent_type t, int perm, void *param)
{
    struct clickfs_dirent *de;

    MALLOC(de, struct clickfs_dirent *, sizeof(*de), M_TEMP, M_WAITOK);
    de->type = t;
    de->perm = perm;
    de->param = param;
    strncpy(de->name, name, sizeof(de->name));
    de->next = d->ent_head;
    de->refcnt = 1;
    d->ent_head = de;

    return 0;
}

int
clickfs_tree_add_link(struct clickfs_dir *d, char *name, char *lnk_name)
{
    struct clickfs_dirent *de;

    MALLOC(de, struct clickfs_dirent *, sizeof(*de), M_TEMP, M_WAITOK);
    de->type = CLICKFS_DIRENT_SYMLINK;
    de->perm = 0777;
    strncpy(de->name, name, sizeof(de->name));
    strncpy(de->lnk_name, lnk_name, sizeof(de->lnk_name));
    de->param = NULL;
    de->next = d->ent_head;
    de->refcnt = 1;
    d->ent_head = de;

    return 0;
}

static void
clickfs_tree_int_free_dirent(struct clickfs_dirent *de)
{
    if (!de) {
	printf("clickfs_tree: trying to free null dirent!\n");
	return;
    }

    if (de->refcnt)
	printf("clickfs_tree: nonzero refcnt %d for a dirent!\n", de->refcnt);
    if (de->param)
	FREE(de->param, M_TEMP);
    FREE(de, M_TEMP);
}

int
clickfs_tree_put_dirent(struct clickfs_dirent *de)
{
    if (--de->refcnt <= 0)
	clickfs_tree_int_free_dirent(de);
}

int
clickfs_tree_ref_dirent(struct clickfs_dirent *de)
{
    de->refcnt++;
}

static void
clickfs_tree_int_put_dirent_r(struct clickfs_dirent *de)
{
    if (de->type == CLICKFS_DIRENT_DIR) {
	/*
	 * Must traverse the directory list and free all the
	 * sub-directory entries.
	 */
	struct clickfs_dir *dp = (struct clickfs_dir *)de->param;
	struct clickfs_dirent *tde = dp->ent_head;

	while (tde) {
	    struct clickfs_dirent *nde = tde->next;
	    clickfs_tree_unlink(dp, tde->name);
	    tde = nde;
	}
    }

    clickfs_tree_put_dirent(de);
}

int
clickfs_tree_unlink(struct clickfs_dir *d, char *name)
{
    struct clickfs_dirent **depp;

    depp = &d->ent_head;
    while (*depp) {
	if (!strcmp(name, (*depp)->name)) {
	    struct clickfs_dirent *tde;
	    tde = *depp;
	    *depp = tde->next;

	    clickfs_tree_int_put_dirent_r(tde);
	    return 0;
	}

	depp = &((*depp)->next);
    }

    return ENOENT;
}

struct clickfs_dir *
clickfs_tree_rootdir()
{
    return (clickfs_dir *) clickfs_tree_root->param;
}

int
clickfs_tree_cleanup()
{
    clickfs_tree_unlink(clickfs_tree_rootdir(), "config");
    clickfs_tree_put_dirent(clickfs_tree_root);
}
