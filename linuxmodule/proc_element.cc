/*
 * proc_element.cc -- support /proc/click/ELEMENT directories
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
#include <click/router.hh>
#include <click/error.hh>

static proc_dir_entry **element_pdes = 0;

static String *handler_strings = 0;
static int *handler_strings_next = 0;
static int handler_strings_cap = 0;
static int handler_strings_free = -1;

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
static int proc_element_handler_release(struct inode *, struct file *);
static int proc_element_handler_ioctl(struct inode *, struct file *, unsigned, unsigned long);

static struct file_operations proc_element_handler_operations = {
    NULL,			// lseek
    proc_element_handler_read,	// read
    proc_element_handler_write,	// write
    NULL,			// readdir
    NULL,			// poll
    proc_element_handler_ioctl,	// ioctl
    NULL,			// mmap
    proc_element_handler_open,	// open
    NULL,			// flush
    proc_element_handler_release, // release
    NULL			// fsync
};

static struct inode_operations proc_element_handler_inode_operations;


// OPERATIONS

static int
increase_handler_strings()
{
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

  // atomic
  unsigned cli_flags;
  save_flags(cli_flags);
  cli();
  
  delete[] handler_strings;
  delete[] handler_strings_next;
  handler_strings_free = handler_strings_cap;
  handler_strings_cap = new_cap;
  handler_strings = new_strs;
  handler_strings_next = new_nexts;
  
  restore_flags(cli_flags);
  return 0;
}

static int
next_handler_string()
{
  if (handler_strings_free < 0)
    increase_handler_strings();
  if (handler_strings_free < 0)
    return -1;
  else {
    int i = handler_strings_free; // atomicity?
    handler_strings_free = handler_strings_next[i];
    return i;
  }
}

static const Router::Handler *
find_handler(int eindex, int handlerno)
{
  if (handlerno < 0)
    return 0;
  if (eindex < 0) {
    if (handlerno >= nroot_handlers)
      return 0;
    else
      return &root_handlers[handlerno];
  } else {
    if (handlerno >= current_router->nhandlers())
      return 0;
    else
      return &current_router->handler(handlerno);
  }
}

static int
prepare_handler_read(int eindex, int handlerno, int stringno)
{
  Element *e = (eindex >= 0 ? current_router->element(eindex) : 0);
  String s;
  int out_of_memory = String::out_of_memory_count();

  const Router::Handler *h = find_handler(eindex, handlerno);
  if (!h)
    return -ENOENT;
  else if (!h->read)
    return -EPERM;
  
  // prevent interrupts
  unsigned cli_flags;
  save_flags(cli_flags);
  cli();

  s = h->read(e, h->read_thunk);
  
  // restore interrupts
  restore_flags(cli_flags);
  
  if (String::out_of_memory_count() != out_of_memory)
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
  else if (!h->write)
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
  else if (!h->write)
    return -EPERM;
  else if (stringno < 0 || stringno >= handler_strings_cap)
    return -EINVAL;
  
  String context_string = "In write handler `" + h->name + "'";
  if (e) context_string += String(" for `") + e->declaration() + "'";
  ContextErrorHandler cerrh(kernel_errh, context_string + ":");
  
  // prevent interrupts
  unsigned cli_flags;
  save_flags(cli_flags);
  cli();

  int result = h->write(handler_strings[stringno], e, h->write_thunk, &cerrh);

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
    handler_strings_next[stringno] = handler_strings_free;
    handler_strings_free = stringno;
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
  
  if (f_pos + count > LARGEST_HANDLER_WRITE)
    return -EFBIG;
  
  if (f_pos + count > old_length) {
    int out_of_memory = String::out_of_memory_count();
    s.append(0, f_pos + count - old_length);
    if (String::out_of_memory_count() != out_of_memory)
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
proc_element_handler_release(struct inode *, struct file *filp)
{
  bool writing = (filp->f_flags & O_ACCMODE) == O_WRONLY;
  int stringno = reinterpret_cast<int>(filp->private_data);
  int retval = 0;
  
  if (writing) {
    proc_dir_entry *pde = (proc_dir_entry *)filp->f_dentry->d_inode->u.generic_ip;
    int eindex = parent_proc_dir_eindex(pde);
    if (eindex < -1)
      retval = eindex;
    else
      retval = finish_handler_write(eindex, (int)pde->data, stringno);
  }

  // free handler string
  if (stringno >= 0 && stringno < handler_strings_cap) {
    handler_strings_next[stringno] = handler_strings_free;
    handler_strings_free = stringno;
  }
  
  return retval;
}

static int
proc_element_handler_ioctl(struct inode *ino, struct file *filp,
			   unsigned command, unsigned long address)
{
  proc_dir_entry *pde = (proc_dir_entry *)ino->u.generic_ip;
  int eindex = parent_proc_dir_eindex(pde);
  if (eindex < 0) return eindex;
  
  Element *e = current_router->element(eindex);
  return e->llrpc(command, reinterpret_cast<void *>(address));
}

//
// CREATING HANDLERS
//

void
register_handler(proc_dir_entry *directory, int eindex, int handlerno)
{
  const proc_dir_entry *pattern = 0;
  const Router::Handler *h = (eindex >= 0 ? &current_router->handler(handlerno) : &root_handlers[handlerno]);
  
  mode_t mode;
  if (h->read && h->write)
    mode = S_IFREG | S_IRUGO | S_IWUSR | S_IWGRP;
  else if (h->write)
    mode = S_IFREG | S_IWUSR | S_IWGRP;
  else if (h->read)
    mode = S_IFREG | S_IRUGO;
  else
    return;

  String name = h->name;
  proc_dir_entry *pde = create_proc_entry(name.cc(), mode, directory);
  // XXX check for NULL
  pde->ops = &proc_element_handler_inode_operations;
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
      subdir = create_proc_entry(component.cc(), S_IFDIR, parent_dir);
      if (parent_dir == proc_click_entry)
	element_pdes[ei + nelements] = subdir;
    } else if (!S_ISDIR(subdir->mode))
      // not a directory; can't deal
      return;
    
    parent_dir = subdir;
    pos = last_pos;
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
  proc_dir_entry *link = create_proc_entry(component.cc(), S_IFLNK | S_IRUGO | S_IWUGO | S_IXUGO, parent_dir);
  link->data = (void *)ei;
  link->readlink_proc = proc_elementlink_readlink_proc;
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

  // make EINDEX directories
  char namebuf[200];
  for (int i = 0; i < nelements; i++) {
    sprintf(namebuf, "%d", i + 1);
    element_pdes[i] = create_proc_entry(namebuf, S_IFDIR, proc_click_entry);
    element_pdes[i]->data = (void *)i;
  }
  
  // add symlinks for ELEMENTNAME -> EINDEX
  for (int i = 0; i < nelements; i++)
    if (proc_dir_entry *pde = element_pdes[i]) {
      String id = current_router->element(i)->id();
      if (memchr(id.data(), '/', id.length()) != 0)
	make_compound_element_symlink(i);
      else {
	if (click_find_pde(proc_click_entry, id)) continue;
	element_pdes[i + nelements] = create_proc_entry(id.cc(), S_IFLNK | S_IRUGO | S_IWUGO | S_IXUGO, proc_click_entry);
	element_pdes[i + nelements]->data = (void *)i;
	element_pdes[i + nelements]->readlink_proc = proc_elementlink_readlink_proc;
      }
    }

  bool add_per_element = current_router->initialized();
  
  // add handlers
  for (int i = 0; i < nelements; i++)
    if (proc_dir_entry *fpde = element_pdes[i]) {
      Element *element = current_router->element(i);
      if (element->cast("Error")) continue;
      element->add_default_handlers(add_per_element);
      if (add_per_element) element->add_handlers();
    }

  // hook up per-element proc entries
  Vector<int> handlers;
  for (int i = 0; i < nelements; i++) {
    handlers.clear();
    current_router->element_handlers(i, handlers);
    for (int j = 0; j < handlers.size(); j++) {
      if (proc_dir_entry *fpde = element_pdes[i])
	register_handler(fpde, i, handlers[j]);
    }
  }
}


void
init_proc_click_elements()
{
  // work around proc_lookup not being exported
  proc_element_handler_inode_operations = proc_dir_inode_operations;
  proc_element_handler_inode_operations.default_file_ops = &proc_element_handler_operations;
}

void
cleanup_proc_click_elements()
{
  cleanup_router_element_procs();
  delete[] handler_strings;
  delete[] handler_strings_next;
}
