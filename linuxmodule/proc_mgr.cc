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

/*
 * Find a proc_dir_entry based on name
 */

proc_dir_entry *
click_find_pde(proc_dir_entry *parent, const String &s)
{
  int len = s.length();
  if (!parent)
    parent = &proc_root;
  for (proc_dir_entry *pde = parent->subdir; pde; pde = pde->next)
    if (pde->namelen == len && memcmp(pde->name, s.data(), len) == 0)
      return pde;
  return 0;
}

void
remove_proc_entry_recursive(proc_dir_entry *pde, proc_dir_entry *parent)
{
  if (pde) {
    while (pde->subdir)
      remove_proc_entry_recursive(pde->subdir, pde);
    remove_proc_entry(pde->name, parent);
  }
}
