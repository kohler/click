/*
 * proc_element.cc -- support /proc/click/ELEMENT directories
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

#include <click/router.hh>
#include <click/error.hh>

#include <click/cxxprotect.h>
CLICK_CXX_PROTECT
#include <linux/spinlock.h>
CLICK_CXX_UNPROTECT
#include <click/cxxunprotect.h>

static proc_dir_entry **element_pdes = 0;

static String *handler_strings = 0;
static int *handler_strings_next = 0;
static int handler_strings_cap = 0;
static int handler_strings_free = -1;

static spinlock_t handler_strings_spinlock;


//
// ELEMENT NAME SYMLINK OPERATIONS
//

static int
proc_elementlink_readlink_proc(proc_dir_entry *pde, char *page)
{
  // must add 1 to (int)pde->data as directories are numbered from 1
  int pos = 0;
  proc_dir_entry *parent = pde->parent;
  while (parent != proc_click_entry) {
    sprintf(page + pos, "../");
    pos += 3;
    parent = parent->parent;
  }
  return pos + sprintf(page + pos, "%d", (int)pde->data + 1);
}


//
// PER-ELEMENT STUFF
//

static int proc_element_handler_open(struct inode *, struct file *);
static ssize_t proc_element_handler_read(struct file *, char *, size_t, loff_t *);
static ssize_t proc_element_handler_write(struct file *, const char *, size_t, loff_t *);
static int proc_element_handler_flush(struct file *);
static int proc_element_handler_release(struct inode *, struct file *);
static int proc_element_handler_ioctl(struct inode *, struct file *, unsigned, unsigned long);

static struct file_operations proc_element_handler_operations;
#ifdef LINUX_2_2
static struct inode_operations proc_element_handler_inode_operations;
#endif


// OPERATIONS

static int
increase_handler_strings()
{
  // must be called with handler_strings_spinlock held

  if (handler_strings_cap < 0)	// in process of cleaning up module
    return -1;
  
  int new_cap = (handler_strings_cap ? 2*handler_strings_cap : 16);
  String *new_strs = new String[new_cap];
  if (!new_strs)
    return -1;
  int *new_nexts = new int[new_cap];
  if (!new_nexts) {
    delete[] new_strs;
    return -1;
  }
  
  for (int i = 0; i < handler_strings_cap; i++)
    new_strs[i] = handler_strings[i];
  for (int i = handler_strings_cap; i < new_cap; i++)
    new_nexts[i] = i + 1;
  new_nexts[new_cap - 1] = handler_strings_free;
  memcpy(new_nexts, handler_strings_next, sizeof(int) * handler_strings_cap);

  delete[] handler_strings;
  delete[] handler_strings_next;
  handler_strings_free = handler_strings_cap;
  handler_strings_cap = new_cap;
  handler_strings = new_strs;
  handler_strings_next = new_nexts;

  return 0;
}

static int
next_handler_string()
{
  spin_lock(&handler_strings_spinlock);
  if (handler_strings_free < 0)
    increase_handler_strings();
  int hs = handler_strings_free;
  if (hs >= 0)
    handler_strings_free = handler_strings_next[hs];
  spin_unlock(&handler_strings_spinlock);
  return hs;
}

static const Router::Handler *
find_handler(int eindex, int handlerno)
{
  if (current_router && current_router->handler_ok(handlerno))
    return &current_router->handler(handlerno);
  else if (Router::global_handler_ok(handlerno))
    return &Router::global_handler(handlerno);
  else
    return 0;
}

static int
prepare_handler_read(int eindex, int handlerno, int stringno)
{
  Element *e = (eindex >= 0 ? current_router->element(eindex) : 0);
  String s;

  const Router::Handler *h = find_handler(eindex, handlerno);
  if (!h)
    return -ENOENT;
  else if (!h->read_visible())
    return -EPERM;
  
  // prevent interrupts
  unsigned cli_flags;
  save_flags(cli_flags);
  cli();

  s = h->call_read(e);
  
  // restore interrupts
  restore_flags(cli_flags);
  
  if (s.out_of_memory())
    return -ENOMEM;

  if (stringno >= 0 && stringno < handler_strings_cap) {
    handler_strings[stringno] = s;
    return 0;
  } else
    return -EINVAL;
}

static int
prepare_handler_write(int eindex, int handlerno)
{
  const Router::Handler *h = find_handler(eindex, handlerno);
  if (!h)
    return -ENOENT;
  else if (!h->write_visible())
    return -EPERM;
  else
    return 0;
}

static int
finish_handler_write(int eindex, int handlerno, int stringno)
{
  Element *e = (eindex >= 0 ? current_router->element(eindex) : 0);
  const Router::Handler *h = find_handler(eindex, handlerno);
  if (!h)
    return -ENOENT;
  else if (!h->write_visible())
    return -EPERM;
  else if (stringno < 0 || stringno >= handler_strings_cap)
    return -EINVAL;
  
  String context_string = "In write handler `" + h->name() + "'";
  if (e) context_string += String(" for `") + e->declaration() + "'";
  ContextErrorHandler cerrh(kernel_errh, context_string + ":");
  
  // prevent interrupts
  unsigned cli_flags;
  save_flags(cli_flags);
  cli();

  int result;
  if (handler_strings[stringno].out_of_memory())
    result = -ENOMEM;
  else
    result = h->call_write(handler_strings[stringno], e, &cerrh);

  // restore interrupts
  restore_flags(cli_flags);

  return result;
}

static int
parent_proc_dir_eindex(proc_dir_entry *pde)
{
  if (pde->parent == proc_click_entry)
    return -1;
  else {
    int eindex = (int)(pde->parent->data);
    if (!current_router || eindex < 0
	|| eindex >= current_router->nelements())
      return -ENOENT;
    else
      return eindex;
  }
}

static int
proc_element_handler_open(struct inode *ino, struct file *filp)
{
  bool writing = (filp->f_flags & O_ACCMODE) == O_WRONLY;
  if ((filp->f_flags & O_ACCMODE) == O_RDWR
      || (filp->f_flags & O_APPEND)
      || (writing && !(filp->f_flags & O_TRUNC)))
    return -EACCES;
  
  proc_dir_entry *pde = (proc_dir_entry *)ino->u.generic_ip;
  int eindex = parent_proc_dir_eindex(pde);
  if (eindex < -1)
    return eindex;

  int stringno = next_handler_string();
  if (stringno < 0)
    return -ENOMEM;
  int retval;
  if (writing) {
    retval = prepare_handler_write(eindex, (int)pde->data);
    handler_strings[stringno] = String();
  } else
    retval = prepare_handler_read(eindex, (int)pde->data, stringno);
  
  if (retval >= 0) {
    filp->private_data = (void *)stringno;
    return 0;
  } else {
    // free handler string
    spin_lock(&handler_strings_spinlock);
    handler_strings_next[stringno] = handler_strings_free;
    handler_strings_free = stringno;
    spin_unlock(&handler_strings_spinlock);
    filp->private_data = (void *)-1;
    return retval;
  }
}

static ssize_t
proc_element_handler_read(struct file *filp, char *buffer, size_t count, loff_t *store_f_pos)
{
  loff_t f_pos = *store_f_pos;
  int stringno = reinterpret_cast<int>(filp->private_data);
  if (stringno < 0 || stringno >= handler_strings_cap)
    return -EINVAL;
  const String &s = handler_strings[stringno];
  if (f_pos + count > s.length())
    count = s.length() - f_pos;
  if (copy_to_user(buffer, s.data() + f_pos, count) > 0)
    return -EFAULT;
  *store_f_pos += count;
  return count;
}

static ssize_t
proc_element_handler_write(struct file *filp, const char *buffer, size_t count, loff_t *store_f_pos)
{
  loff_t f_pos = *store_f_pos;
  int stringno = reinterpret_cast<int>(filp->private_data);
  if (stringno < 0 || stringno >= handler_strings_cap)
    return -EINVAL;
  String &s = handler_strings[stringno];
  int old_length = s.length();

#ifdef LARGEST_HANDLER_WRITE
  if (f_pos + count > LARGEST_HANDLER_WRITE)
    return -EFBIG;
#endif
  
  if (f_pos + count > old_length) {
    s.append_fill(0, f_pos + count - old_length);
    if (s.out_of_memory())
      return -ENOMEM;
  }
  
  int length = s.length();
  if (f_pos > length)
    return -EFBIG;
  else if (f_pos + count > length)
    count = length - f_pos;
  
  char *data = s.mutable_data();
  if (f_pos > old_length)
    memset(data + old_length, 0, f_pos - old_length);
  
  if (copy_from_user(data + f_pos, buffer, count) > 0)
    return -EFAULT;
  
  *store_f_pos += count;
  return count;
}

static int
proc_element_handler_flush(struct file *filp)
{
  bool writing = (filp->f_flags & O_ACCMODE) == O_WRONLY;
  int stringno = reinterpret_cast<int>(filp->private_data);
  int retval = 0;

#ifdef LINUX_2_2
  int f_count = filp->f_count;
#else
  int f_count = atomic_read(&filp->f_count);
#endif
  if (writing && f_count == 1) {
    proc_dir_entry *pde = (proc_dir_entry *)filp->f_dentry->d_inode->u.generic_ip;
    int eindex = parent_proc_dir_eindex(pde);
    if (eindex < -1)
      retval = eindex;
    else
      retval = finish_handler_write(eindex, (int)pde->data, stringno);
  }

  return retval;
}

static int
proc_element_handler_release(struct inode *, struct file *filp)
{
  int stringno = reinterpret_cast<int>(filp->private_data);

  // free handler string
  if (stringno >= 0 && stringno < handler_strings_cap) {
    spin_lock(&handler_strings_spinlock);
    handler_strings_next[stringno] = handler_strings_free;
    handler_strings_free = stringno;
    spin_unlock(&handler_strings_spinlock);
  }
  
  return 0;
}

static int
proc_element_handler_ioctl(struct inode *ino, struct file *filp,
			   unsigned command, unsigned long address)
{
  proc_dir_entry *pde = (proc_dir_entry *)ino->u.generic_ip;
  int eindex = parent_proc_dir_eindex(pde);
  if (eindex < 0) return eindex;
  
  Element *e = current_router->element(eindex);
  if (current_router->initialized())
    return e->llrpc(command, reinterpret_cast<void *>(address));
  else
    return e->Element::llrpc(command, reinterpret_cast<void *>(address));
}

//
// CREATING HANDLERS
//

void
register_handler(proc_dir_entry *directory, int handlerno)
{
  const proc_dir_entry *pattern = 0;
  const Router::Handler *h = &Router::handler(current_router, handlerno);
  
  mode_t mode = S_IFREG;
  if (h->read_visible())
    mode |= proc_click_mode_r;
  if (h->write_visible())
    mode |= proc_click_mode_w;

  String name = h->name();
  proc_dir_entry *pde = create_proc_entry(name.cc(), mode, directory);
  // XXX check for NULL
#ifdef LINUX_2_2
  pde->ops = &proc_element_handler_inode_operations;
#else
  pde->proc_fops = &proc_element_handler_operations;
#endif
  pde->data = (void *)handlerno;
}

//
// ROUTER ELEMENT PROCS
//

void
cleanup_router_element_procs()
{
  if (!current_router) return;
  int nelements = current_router->nelements();
  for (int i = 0; i < 2*nelements; i++) {
    if (proc_dir_entry *pde = element_pdes[i])
      remove_proc_entry_recursive(pde, proc_click_entry);
  }
  kfree(element_pdes);
  element_pdes = 0;
}

static void
make_compound_element_symlink(int ei)
{
  const String &id = current_router->element(ei)->id();
  const char *data = id.data();
  int len = id.length();
  if (len == 0)
    // empty name; do nothing
    return;

  // looking in `parent_dir'. if we create any proc_dir_entry at top level --
  // under `proc_click_entry' -- we must save it in
  // element_pdes[ei+nelements]. otherwise it will not be removed when the
  // router is destroyed.
  proc_dir_entry *parent_dir = proc_click_entry;
  int nelements = current_router->nelements();
  
  // divide into path components and create directories
  int first_pos = -1, last_pos = -1;
  int pos = 0;
  int ncomponents = 0;
  while (pos < len) {
    // skip slashes
    while (pos < len && data[pos] == '/')
      pos++;
    
    // find the next component; it starts at first_pos and ends before
    // last_pos
    first_pos = pos;
    while (pos < len && data[pos] != '/')
      pos++;
    last_pos = pos;
    
    // check to see if this was the last component. if so, use it as the link
    // name
    while (pos < len && data[pos] == '/')
      pos++;
    if (pos >= len)
      break;
    
    // otherwise, it was an intermediate component. make a directory for it
    assert(last_pos > first_pos && last_pos < len);
    String component = id.substring(first_pos, last_pos - first_pos);
    proc_dir_entry *subdir = click_find_pde(parent_dir, component);
    if (!subdir) {
      // make the directory
      subdir = create_proc_entry(component.cc(), proc_click_mode_dir, parent_dir);
      if (parent_dir == proc_click_entry)
	element_pdes[ei + nelements] = subdir;
    } else if (!S_ISDIR(subdir->mode))
      // not a directory; can't deal
      return;
    
    parent_dir = subdir;
    pos = last_pos;
    ncomponents++;
  }

  // if no component, nothing to do
  if (last_pos <= first_pos)
    return;
  
  // have a final component; it is a link.
  String component = id.substring(first_pos, last_pos - first_pos);
  if (click_find_pde(parent_dir, component))
    // name already exists; nothing to do
    return;

  // make the link
#ifdef LINUX_2_4
  char buf[200];
  if (ncomponents)
    sprintf(buf, "/proc/click/%d", ei + 1);
  else
    sprintf(buf, "%d", ei + 1);
  proc_dir_entry *link = proc_symlink(component.cc(), parent_dir, buf);
#else
  proc_dir_entry *link = create_proc_entry(component.cc(), S_IFLNK | S_IRUGO | S_IWUGO | S_IXUGO, parent_dir);
  link->data = (void *)ei;
  link->readlink_proc = proc_elementlink_readlink_proc;
#endif
  if (parent_dir == proc_click_entry)
    element_pdes[ei + nelements] = link;
}

void
init_router_element_procs()
{
  int nelements = current_router->nelements();
  
  element_pdes = (proc_dir_entry **)
    kmalloc(sizeof(proc_dir_entry *) * 2 * nelements, GFP_ATOMIC);
  if (!element_pdes) return;
  for (int i = 0; i < 2*nelements; i++)
    element_pdes[i] = 0;

  // add handler directories for elements with handlers
  Vector<int> handlers;
  char namebuf[200];
  for (int i = 0; i < nelements; i++) {
    // are there any visible handlers? if not, skip
    handlers.clear();
    current_router->element_handlers(i, handlers);
    for (int j = 0; j < handlers.size(); j++) {
      const Router::Handler &h = current_router->handler(handlers[j]);
      if (!h.read_visible() && !h.write_visible()) {
	handlers[j] = handlers.back();
	handlers.pop_back();
	j--;
      }
    }
    if (!handlers.size())
      continue;

    // otherwise, make EINDEX directory
    sprintf(namebuf, "%d", i + 1);
    proc_dir_entry *pde = create_proc_entry(namebuf, proc_click_mode_dir, proc_click_entry);
    if (!pde)
      continue;

    // got directory
    element_pdes[i] = pde;
    pde->data = (void *)i;
  
    // add symlink for ELEMENTNAME -> EINDEX
    make_compound_element_symlink(i);

    // hook up per-element proc entries
    for (int j = 0; j < handlers.size(); j++)
      register_handler(pde, handlers[j]);
  }
}


void
init_proc_click_elements()
{
#ifdef LINUX_2_4
  proc_element_handler_operations.owner = THIS_MODULE;
#endif
  proc_element_handler_operations.read = proc_element_handler_read;
  proc_element_handler_operations.write = proc_element_handler_write;
  proc_element_handler_operations.ioctl = proc_element_handler_ioctl;
  proc_element_handler_operations.open = proc_element_handler_open;
  proc_element_handler_operations.flush = proc_element_handler_flush;
  proc_element_handler_operations.release = proc_element_handler_release;

#ifdef LINUX_2_2
  proc_element_handler_inode_operations = proc_dir_inode_operations;
  proc_element_handler_inode_operations.default_file_ops = &proc_element_handler_operations;
#endif

  spin_lock_init(&handler_strings_spinlock);
}

void
cleanup_proc_click_elements()
{
  cleanup_router_element_procs();

  // clean up handler_strings
  spin_lock(&handler_strings_spinlock);
  delete[] handler_strings;
  delete[] handler_strings_next;
  handler_strings = 0;
  handler_strings_next = 0;
  handler_strings_cap = -1;
  handler_strings_free = -1;
  spin_unlock(&handler_strings_spinlock);
}
