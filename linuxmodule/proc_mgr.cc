/*
 * proc_mgr.cc -- improve interface to /proc entry creation and deletion
 * Eddie Kohler
 *
 * Copyright (c) 1999-2000 Massachusetts Institute of Technology
 * Copyright (c) 2000 Mazu Networks, Inc.
 *
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files (the "Software"),
 * to deal in the Software without restriction, subject to the following
 * conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * Further elaboration of this license, including a DISCLAIMER OF ANY
 * WARRANTY, EXPRESS OR IMPLIED, is provided in the LICENSE file, which is
 * also accessible at http://www.pdos.lcs.mit.edu/click/license.html
 */

#ifdef HAVE_CONFIG_H
# include <config.h>
#endif
#include "modulepriv.hh"
#include <click/string.hh>

struct click_proc_dir_set {
  click_proc_dir_set *next;
  click_proc_dir_entry e[48];
};

static click_proc_dir_set *all_pde = 0;
static click_proc_dir_entry *free_pde = 0;

/*
 * fill_inode procedure to keep track of inodes
 */

extern "C" {

static void
click_proc_fill_inode(struct inode *inode, int fill)
{
  struct proc_dir_entry *pde = reinterpret_cast<proc_dir_entry *>(inode->u.generic_ip);
  struct click_proc_dir_entry *cpde = static_cast<click_proc_dir_entry *>(pde);
  if (cpde) {
    if (fill) {
      //click_chatter("inode %.*s %p", pde->namelen, pde->name, inode);
      if (cpde->inode && cpde->inode != inode)
	click_chatter("inode reused for %.*s %p %p", pde->namelen, pde->name, cpde->inode, inode);
      else
	cpde->inode = inode;
    } else {
      //click_chatter("~inode %.*s %p", pde->namelen, pde->name, inode);
      cpde->inode = 0;
    }
  }
}

}

/*
 * Register a proc_dir_entry with Linux
 */

int
click_register_pde(proc_dir_entry *parent, click_proc_dir_entry *child)
{
  if (child->fill_inode)
    click_chatter("child %.*s fill inode on", child->namelen, child->name);
  child->fill_inode = click_proc_fill_inode;
  child->inode = 0;
  return proc_register(parent, child);
}

int
click_register_pde(proc_dir_entry *parent, click_x_proc_dir_entry *child)
{
  return click_register_pde(parent, reinterpret_cast<click_proc_dir_entry *>(child));
}

/*
 * Unregister a proc_dir_entry, and all its subdirectories
 */

int
click_unregister_pde(click_proc_dir_entry *child)
{
  while (child->subdir)
    click_unregister_pde(static_cast<click_proc_dir_entry *>(child->subdir));
  if (child->inode) {
    child->inode->i_op = 0;
    child->inode->u.generic_ip = 0;
  }
  int retval = proc_unregister(child->parent, child->low_ino);
}

/*
 * Return a new dynamically allocated proc_dir_entry. All such entries will
 * be freed when the module is unloaded
 */

click_proc_dir_entry *
click_new_dynamic_pde()
{
  if (!free_pde) {
    click_proc_dir_set *new_set = (click_proc_dir_set *)
      kmalloc(sizeof(click_proc_dir_set), GFP_ATOMIC);
    if (!new_set) return 0;
    new_set->next = all_pde;
    all_pde = new_set;
    for (int i = 0; i < 47; i++)
      new_set->e[i].next = &new_set->e[i+1];
    new_set->e[47].next = 0;
    free_pde = &new_set->e[0];
  }
  click_proc_dir_entry *result = free_pde;
  free_pde = static_cast<click_proc_dir_entry *>(free_pde->next);
  result->next = 0;
  result->subdir = 0;
  result->low_ino = 0;
  return result;
}

/*
 * Make and register a dynamically allocated proc_dir_entry in a single step
 */

click_proc_dir_entry *
click_register_new_dynamic_pde(proc_dir_entry *parent,
			       const proc_dir_entry *pattern,
			       int namelen, const char *name, void *data)
{
  click_proc_dir_entry *pde = click_new_dynamic_pde();
  if (!pde) return 0;
  memcpy(pde, pattern, sizeof(proc_dir_entry));
  if (namelen >= 0) {
    pde->namelen = namelen;
    pde->name = name;
    pde->data = data;
  }
  if (click_register_pde(parent, pde) < 0) {
    click_kill_dynamic_pde(pde);
    return 0;
  } else
    return pde;
}

/*
 * Kill a dynamically allocated proc_dir_entry by unregistering and freeing
 * it, and any nested files or directories.
 */

int
click_kill_dynamic_pde(click_proc_dir_entry *pde)
{
  while (pde->subdir)
    click_kill_dynamic_pde(static_cast<click_proc_dir_entry *>(pde->subdir));

  // clean up inode operations and proc_dir_entry pointer
  if (pde->inode) {
    pde->inode->i_op = 0;
    pde->inode->u.generic_ip = 0;
  }
  
  int result;
  if (pde->low_ino != 0)
    result = proc_unregister(pde->parent, pde->low_ino);
  else
    result = 0;
  
  pde->next = free_pde;
  free_pde = pde;
  return result;
}

/*
 * Find a proc_dir_entry based on name
 */

click_proc_dir_entry *
click_find_pde(proc_dir_entry *parent, const String &s)
{
  int len = s.length();
  for (proc_dir_entry *pde = parent->subdir; pde; pde = pde->next)
    if (pde->namelen == len && memcmp(pde->name, s.data(), len) == 0)
      return static_cast<click_proc_dir_entry *>(pde);
  return 0;
}


/*
 * Initialization and cleanup
 */

void
init_click_proc()
{
}

void
cleanup_click_proc()
{
  while (all_pde) {
    click_proc_dir_set *next = all_pde->next;
    kfree(all_pde);
    all_pde = next;
  }
}
