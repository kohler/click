/*
 * clickfs_tree.cc -- Click configuration filesystem for BSD
 * Nickolai Zeldovich, Marko Zec
 *
 * Copyright (c) 2001 Massachusetts Institute of Technology
 * Copyright (c) 2004 International Computer Science Institute
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

#include <click/string.hh>
#include <click/straccum.hh>
CLICK_USING_DECLS

MALLOC_DEFINE(M_CLICKFS, "clickfs", "Click filesystem node");

struct clickfs_dirent *clickfs_tree_root;
uint32_t clickfs_ino;		/* Just assume it will never rewind */

void
clickfs_tree_init()
{
    struct clickfs_dirent *de;

    clickfs_ino = 3;		/* Eddie will like this one ;^) */

    MALLOC(de, struct clickfs_dirent *, sizeof(*de), M_CLICKFS, M_WAITOK);
    bzero(de, sizeof(*de));
    de->data.dir.head = NULL;	/* Empty dir */
    de->data.dir.parent = NULL;	/* Root dir has no parent */
    de->type = CLICKFS_DIRENT_DIR;
    de->perm = 0755;
    *de->name = '\0';		/* The root doesn't have a useful name */
    de->next = NULL;
    de->file_refcnt = 1;	/* "." and ".." count as one in root dir */
    de->fileno = 1;
    clickfs_tree_root = de;

    /*
     * Create global handlers in clickfs root dir.
     */
    Vector<int> handlers;
    handlers.clear();
    click_router->element_hindexes(0, handlers);
    for (int h_idx=0; h_idx < handlers.size(); h_idx++)
	clickfs_tree_add_handle(de,
		Router::handler(click_router, handlers[h_idx]),
		0, handlers[h_idx]);
}

static struct clickfs_dirent *
clickfs_tree_find_file(struct clickfs_dirent *cde, char *name)
{
    struct clickfs_dirent *de;

    for (de = cde->data.dir.head; de; de = de->next)
	if (!strcmp(name, de->name)) {
	    if (de->type == CLICKFS_DIRENT_SYMLINK) {
		/* Our symlinks can only include ".." in the path */
		char *np = de->data.slink.name;
		while (!strncmp(np, "../", 3)) {
		    cde = cde->data.dir.parent;
		    np += 3;
		}
		return(clickfs_tree_find_file(cde, np));
	    } else
		return(de);
	}
    return NULL;
}

struct clickfs_dirent *
clickfs_tree_add_dir(struct clickfs_dirent *cde, char *name, int perm)
{
    struct clickfs_dirent *de, *tde;
    char *cp;

    /* If name contains a '/' character, recursively add the whole path. */
    if ((cp = rindex(name, '/'))) {
	*cp = '\0';
	cde = clickfs_tree_add_dir(cde, name, perm);
	*cp = '/';
	name = cp + 1;
    }

    /* Check if a directory node with the same name already exists */
    for (de = cde->data.dir.head; de; de = de->next)
	if ((tde = clickfs_tree_find_file(cde, name)) &&
	    tde->type == CLICKFS_DIRENT_DIR)
	    return(tde);

    /* Create a new directory entry and add/link it to the parent. */
    MALLOC(de, struct clickfs_dirent *, sizeof(*de), M_CLICKFS, M_WAITOK);
    bzero(de, sizeof(*de));
    de->data.dir.head = NULL;	/* Empty dir */
    de->data.dir.parent = cde;	/* Parent to the new dir */
    de->type = CLICKFS_DIRENT_DIR;
    strncpy(de->name, name, sizeof(de->name));
    de->perm = perm;
    de->next = cde->data.dir.head;
    cde->data.dir.head = de;
    de->file_refcnt = 2;	/* count both "." and ".." */
    de->fileno = clickfs_ino++;
    cde->file_refcnt++;

    return de;
}

void
clickfs_tree_add_handle(struct clickfs_dirent *cde,
			const CLICK_NAME(Handler) *h,
			int eindex, int handle)
{
    struct clickfs_dirent *de;
    String name = h->name();

    if (!h->read_visible() && !h->write_visible())
	return;

    MALLOC(de, struct clickfs_dirent *, sizeof(*de), M_CLICKFS, M_WAITOK);
    bzero(de, sizeof(*de));
    de->type = CLICKFS_DIRENT_HANDLE;
    if (h->read_visible()) de->perm |= 0444;
    if (h->write_visible()) de->perm |= 0200;
    strncpy(de->name, name.c_str(), sizeof(de->name));
    de->next = cde->data.dir.head;
    cde->data.dir.head = de;
    de->file_refcnt = 1;
    de->fileno = clickfs_ino++;
    cde->file_refcnt++;
    de->data.handle.handle = handle;
    de->data.handle.eindex = eindex;
}

void
clickfs_tree_add_link(struct clickfs_dirent *cde, char *name, char *lnk_name)
{
    struct clickfs_dirent *de;

    /* Do nothing if there's already a file with the same name */
    if (clickfs_tree_find_file(cde, name))
	return;

    MALLOC(de, struct clickfs_dirent *, sizeof(*de), M_CLICKFS, M_WAITOK);
    bzero(de, sizeof(*de));
    de->type = CLICKFS_DIRENT_SYMLINK;
    de->perm = 0777;
    strncpy(de->name, name, sizeof(de->name));
    strncpy(de->data.slink.name, lnk_name, sizeof(de->data.slink.name));
    de->next = cde->data.dir.head;
    cde->data.dir.head = de;
    de->file_refcnt = 1;
    de->fileno = clickfs_ino++;
    cde->file_refcnt++;
}

static void
clickfs_tree_free_dirent_r(struct clickfs_dirent *cde)
{
    if (cde->type == CLICKFS_DIRENT_DIR) {
	/*
	 * Must traverse the directory list and free all the
	 * sub-directory entries.
	 */
	while (cde->data.dir.head)
	    clickfs_tree_unlink(cde, cde->data.dir.head->name);
    }
    FREE(cde, M_CLICKFS);
}

void
clickfs_tree_unlink(struct clickfs_dirent *cde, char *name)
{
    struct clickfs_dirent *de, *prev = NULL;

    for (de = cde->data.dir.head; de; de = (prev=de)->next)
	if (!strcmp(name, de->name)) {
	    if (prev)
		prev->next = de->next;
	    else
		cde->data.dir.head = de->next;
	    clickfs_tree_free_dirent_r(de);
	    cde->file_refcnt--;
	    return;
	}
}

void
clickfs_tree_cleanup()
{
    clickfs_tree_free_dirent_r(clickfs_tree_root);
}
