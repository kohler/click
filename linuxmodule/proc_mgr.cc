/*
 * proc_mgr.cc -- improve interface to /proc entry creation and deletion
 * Eddie Kohler
 *
 * Copyright (c) 1999-2000 Massachusetts Institute of Technology
 * Copyright (c) 2000 Mazu Networks, Inc.
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
