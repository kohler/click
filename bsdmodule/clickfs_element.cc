/*
 * clickfs_element.cc -- Click configuration filesystem for BSD
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

void
init_router_element_procs()
{
    /*
     * Iterate over the current router's elements and create element
     * directories and symlinks for them.
     */
    int nelements = click_router->nelements();

    for (int curelem = 0; curelem<nelements; curelem++) {
	const Element *e = click_router->element(curelem);
	const String &id = e->name();
	char namebuf[CLICKFS_DIRENT_NAMLEN], linkbuf[CLICKFS_DIRENT_NAMLEN];
	char *lbp, *nbp;
	struct clickfs_dirent *cde;

	/* element indexes start from 0, directory entries start from 1 */
	snprintf(namebuf, sizeof(namebuf), "%d", curelem + 1);
	struct clickfs_dirent *elemdp =
		clickfs_tree_add_dir(clickfs_tree_root, namebuf, 0755);

	/*
	 * Create a symlink. This might require traversing / creating
	 * nested subdirectories for those nasty compound elements.
	 */
	snprintf(linkbuf, sizeof(linkbuf), "%s", id.data());
	linkbuf[id.length()] = '\0';
	nbp = namebuf;
	if ((lbp = rindex(linkbuf, '/'))) {
	    char *c = linkbuf;
	    while ((c = index(c, '/'))) {
		sprintf(nbp, "../");
		nbp += 3;
		c++;
	    }
	    *lbp++ = '\0';
	    cde = clickfs_tree_add_dir(clickfs_tree_root, linkbuf, 0755);
	}
	else {
	    lbp = linkbuf;
	    cde = clickfs_tree_root;
	}
	sprintf(nbp, "%d", curelem + 1);
	clickfs_tree_add_link(cde, lbp, namebuf);

	/* Now iterate over all handlers in this router element */
	Vector<int> handlers;
	handlers.clear();
	click_router->element_hindexes(e, handlers);
	for (int h_idx=0; h_idx < handlers.size(); h_idx++)
	    clickfs_tree_add_handle(
		  elemdp,
		  Router::handler(click_router, handlers[h_idx]),
		  curelem,
		  handlers[h_idx]);
    }
}


void
cleanup_router_element_procs()
{
    struct clickfs_dirent *de = clickfs_tree_root->data.dir.head;

    /* Unlink all root directory entries, except global handlers */
    while (de)
	if (de->type != CLICKFS_DIRENT_HANDLE) {
	    clickfs_tree_unlink(clickfs_tree_root, de->name);
	    de = clickfs_tree_root->data.dir.head;
	} else
	    de = de->next;
}
