/*
 * clickfs_element.cc -- Click configuration filesystem for BSD
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

void
init_router_element_procs()
{
    /*
     * Iterate over the current router's elements and create element
     * directories and symlinks for them.
     */
    struct clickfs_dir *rootdp = clickfs_tree_rootdir();
    int nelements = click_router->nelements();

    for (int curelem = 0; curelem<nelements; curelem++) {
	const String &id = click_router->element(curelem)->id();
	char buf[64], nbuf[64];

	/*
	 * element indexes start from 0, directory entries start from 1
	 */
	snprintf(buf, sizeof(buf), "%d", curelem+1);
	snprintf(nbuf, sizeof(nbuf), "%s", id.data());
	nbuf[id.length()] = '\0';
	clickfs_tree_add_link(rootdp, nbuf, buf);
	struct clickfs_dir *elemdp = clickfs_tree_add_dir(rootdp, buf, 0755);

	/*
	 * Now iterate over the handlers in this router element...
	 */
	Vector<int> handlers;
	handlers.clear();
	click_router->element_handlers(curelem, handlers);
	for (int h_idx=0; h_idx < handlers.size(); h_idx++) {
	    int *handler_params;
	    const Router::Handler &h = click_router->handler(handlers[h_idx]);
	    if (!h.read_visible() && !h.write_visible()) continue;

	    MALLOC(handler_params, int *, 2*sizeof(int), M_TEMP, M_WAITOK);
	    handler_params[0] = handlers[h_idx];
	    handler_params[1] = curelem;
	    String name = h.name();
	    int perm = 0;
	    if (h.read_visible()) perm |= 0444;
	    if (h.write_visible()) perm |= 0200;
	    clickfs_tree_add_file(elemdp, (char *) name.cc(),
				  CLICKFS_DIRENT_EHANDLE, perm, handler_params);
	}
    }
}

void
cleanup_router_element_procs()
{
    struct clickfs_dir *rootdp = clickfs_tree_rootdir();
    int nelements = click_router->nelements();

    for (int curelem = 0; curelem<nelements; curelem++) {
	const String &id = click_router->element(curelem)->id();
	char buf[64], nbuf[64];

	snprintf(buf, sizeof(buf), "%d", curelem+1);
	snprintf(nbuf, sizeof(nbuf), "%s", id.data());
	nbuf[id.length()] = '\0';
	clickfs_tree_unlink(rootdp, buf);
	clickfs_tree_unlink(rootdp, nbuf);
    }
}
