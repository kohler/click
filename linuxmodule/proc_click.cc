/*
 * proc_element.cc -- support /proc/click/ELEMENT directories
 * Eddie Kohler
 *
 * Copyright (c) 1999-2000 Massachusetts Institute of Technology
 * Copyright (c) 2000 Mazu Networks, Inc.
 * Copyright (c) 2002 International Computer Science Institute
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

static proc_dir_entry *proc_click_entry = 0;
static proc_dir_entry **element_pdes = 0;

static struct file_operations proc_element_handler_operations;
#ifdef LINUX_2_2
static struct inode_operations proc_element_handler_inode_operations;
#endif


/************************ General /proc manipulation *************************/

static proc_dir_entry *
find_pde(proc_dir_entry *parent, const String &s)
{
  int len = s.length();
  if (!parent)
    parent = &proc_root;
  for (proc_dir_entry *pde = parent->subdir; pde; pde = pde->next)
    if (pde->namelen == len && memcmp(pde->name, s.data(), len) == 0)
      return pde;
  return 0;
}

static void
remove_proc_entry_recursive(proc_dir_entry *pde, proc_dir_entry *parent)
{
  if (pde) {
    while (pde->subdir)
      remove_proc_entry_recursive(pde->subdir, pde);
    remove_proc_entry(pde->name, parent);
  }
}


/***************************** handler_strings *******************************/

struct HandlerStringInfo {
  int next;
  int flags;
};

static String *handler_strings = 0;
static HandlerStringInfo *handler_strings_info = 0;
static int handler_strings_cap = 0;
static int handler_strings_free = -1;
static spinlock_t handler_strings_lock;

static int
increase_handler_strings()
{
  // must be called with handler_strings_lock held

  if (handler_strings_cap < 0)	// in process of cleaning up module
    return -1;
  
  int new_cap = (handler_strings_cap ? 2*handler_strings_cap : 16);
  String *new_strs = new String[new_cap];
  if (!new_strs)
    return -1;
  HandlerStringInfo *new_infos = new HandlerStringInfo[new_cap];
  if (!new_infos) {
    delete[] new_strs;
    return -1;
  }
  
  for (int i = 0; i < handler_strings_cap; i++)
    new_strs[i] = handler_strings[i];
  for (int i = handler_strings_cap; i < new_cap; i++)
    new_infos[i].next = i + 1;
  new_infos[new_cap - 1].next = handler_strings_free;
  memcpy(new_infos, handler_strings_info, sizeof(HandlerStringInfo) * handler_strings_cap);

  delete[] handler_strings;
  delete[] handler_strings_info;
  handler_strings_free = handler_strings_cap;
  handler_strings_cap = new_cap;
  handler_strings = new_strs;
  handler_strings_info = new_infos;

  return 0;
}

static int
next_handler_string()
{
  spin_lock(&handler_strings_lock);
  if (handler_strings_free < 0)
    increase_handler_strings();
  int hs = handler_strings_free;
  if (hs >= 0)
    handler_strings_free = handler_strings_info[hs].next;
  spin_unlock(&handler_strings_lock);
  return hs;
}

static void
free_handler_string(int hs)
{
  spin_lock(&handler_strings_lock);
  if (hs >= 0 && hs < handler_strings_cap) {
    handler_strings[hs] = String();
    handler_strings_info[hs].next = handler_strings_free;
    handler_strings_free = hs;
  }
  spin_unlock(&handler_strings_lock);
}


/*********************** Operations on handler files *************************/

static const Router::Handler *
find_handler(int eindex, int handlerno)
{
  if (Router::handler_ok(click_router, handlerno))
    return &Router::handler(click_router, handlerno);
  else
    return 0;
}

static int
prepare_handler_read(int eindex, int handlerno, int stringno)
{
  Element *e = (eindex >= 0 ? click_router->element(eindex) : 0);
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
    handler_strings_info[stringno].flags = h->flags();
    return 0;
  } else
    return -EINVAL;
}

static int
prepare_handler_write(int eindex, int handlerno, int stringno)
{
  const Router::Handler *h = find_handler(eindex, handlerno);
  if (!h)
    return -ENOENT;
  else if (!h->write_visible())
    return -EPERM;
  else {
    handler_strings[stringno] = String();
    handler_strings_info[stringno].flags = h->flags();
    return 0;
  }
}

static int
finish_handler_write(int eindex, int handlerno, int stringno)
{
  Element *e = (eindex >= 0 ? click_router->element(eindex) : 0);
  const Router::Handler *h = find_handler(eindex, handlerno);
  if (!h)
    return -ENOENT;
  else if (!h->write_visible())
    return -EPERM;
  else if (stringno < 0 || stringno >= handler_strings_cap)
    return -EINVAL;
  
  String context_string = "In write handler `" + h->name() + "'";
  if (e) context_string += String(" for `") + e->declaration() + "'";
  ContextErrorHandler cerrh(click_logged_errh, context_string + ":");
  
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
    if (!click_router || eindex < 0
	|| eindex >= click_router->nelements())
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
  if (writing)
    retval = prepare_handler_write(eindex, (int)pde->data, stringno);
  else
    retval = prepare_handler_read(eindex, (int)pde->data, stringno);
  
  if (retval >= 0) {
    filp->private_data = (void *)stringno;
    return 0;
  } else {
    free_handler_string(stringno);
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

  // reread string, if necessary
  if (handler_strings_info[stringno].flags & HANDLER_REREAD) {
    proc_dir_entry *pde = (proc_dir_entry *)filp->f_dentry->d_inode->u.generic_ip;
    int eindex = parent_proc_dir_eindex(pde);
    int retval = prepare_handler_read(eindex, (int)pde->data, stringno);
    if (retval < 0)
      return retval;
  }
  
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
  if (f_pos + count > LARGEST_HANDLER_WRITE
      && !(handler_strings_info[stringno].flags & HANDLER_WRITE_UNLIMITED))
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
  if (stringno >= 0)
    free_handler_string(stringno);
  return 0;
}

static int
proc_element_handler_ioctl(struct inode *ino, struct file *filp,
			   unsigned command, unsigned long address)
{
  proc_dir_entry *pde = (proc_dir_entry *)ino->u.generic_ip;
  int eindex = parent_proc_dir_eindex(pde);
  if (eindex < 0) return eindex;
  
  Element *e = click_router->element(eindex);
  if (click_router->initialized())
    return e->llrpc(command, reinterpret_cast<void *>(address));
  else
    return e->Element::llrpc(command, reinterpret_cast<void *>(address));
}

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


/************************ Creating handler entries ***************************/

static void
register_handler(proc_dir_entry *directory, int handlerno)
{
  const proc_dir_entry *pattern = 0;
  const Router::Handler *h = &Router::handler(click_router, handlerno);
  
  mode_t mode = S_IFREG;
  if (h->read_visible())
    mode |= click_mode_r;
  if (h->write_visible())
    mode |= click_mode_w;

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

void
cleanup_router_element_procs()
{
  if (!click_router)
    return;
  int nelements = click_router->nelements();
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
  const String &id = click_router->element(ei)->id();
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
  int nelements = click_router->nelements();
  
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
    proc_dir_entry *subdir = find_pde(parent_dir, component);
    if (!subdir) {
      // make the directory
      subdir = create_proc_entry(component.cc(), click_mode_dir, parent_dir);
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
  if (find_pde(parent_dir, component))
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
  int nelements = click_router->nelements();
  
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
    click_router->element_handlers(i, handlers);
    for (int j = 0; j < handlers.size(); j++) {
      const Router::Handler &h = click_router->handler(handlers[j]);
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
    proc_dir_entry *pde = create_proc_entry(namebuf, click_mode_dir, proc_click_entry);
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


/********************* Initialization and cleanup ****************************/

int
init_proc_click()
{
  proc_click_entry = create_proc_entry("click", click_mode_dir, 0);
  if (!proc_click_entry)
    return -ENOMEM;

  // operations structures
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

  // handler_strings
  spin_lock_init(&handler_strings_lock);

  // /proc/click entries for global handlers
  for (int i = 0; i < Router::nglobal_handlers(); i++)
    register_handler(proc_click_entry, Router::FIRST_GLOBAL_HANDLER + i);
}

void
cleanup_proc_click()
{
  // remove /proc/click entries for global and local handlers
  cleanup_router_element_procs();
  for (int i = 0; i < Router::nglobal_handlers(); i++) {
    const Router::Handler &h = Router::global_handler(Router::FIRST_GLOBAL_HANDLER + i);
    remove_proc_entry(String(h.name()).cc(), proc_click_entry);
  }

  // remove the `/proc/click' directory
  remove_proc_entry("click", 0);

  // invalidate any remaining `/proc/click' dentry, which would be hanging
  // around because someone has a handler open
#ifdef LINUX_2_2
  struct dentry *click_de = lookup_dentry("/proc/click", 0, LOOKUP_DIRECTORY);
#else
  struct nameidata lookup_data;
  struct dentry *click_de = 0;
  if (user_path_walk("/proc/click", &lookup_data) >= 0)
    click_de = lookup_data.dentry;
#endif
  if (click_de && !IS_ERR(click_de)) {
    d_drop(click_de);		// XXX ok for 2.4?
    dput(click_de);
  }
  
  // delete handler_strings
  spin_lock(&handler_strings_lock);
  delete[] handler_strings;
  delete[] handler_strings_info;
  handler_strings = 0;
  handler_strings_info = 0;
  handler_strings_cap = -1;
  handler_strings_free = -1;
  spin_unlock(&handler_strings_lock);
}
