/*
 * proc_element.cc -- support /proc/click/ELEMENT directories
 * Eddie Kohler
 *
 * Copyright (c) 1999-2000 Massachusetts Institute of Technology.
 *
 * This software is being provided by the copyright holders under the GNU
 * General Public License, either version 2 or, at your discretion, any later
 * version. For more information, see the `COPYRIGHT' file in the source
 * distribution.
 */

#ifdef HAVE_CONFIG_H
# include <config.h>
#endif
#include "modulepriv.hh"
#include "router.hh"
#include "error.hh"

static proc_dir_entry **element_pdes = 0;
static char *numbers = 0;
static int numbers_ndigits = 0;

static struct proc_dir_entry proc_element_elemdir_prototype = {
  0,				// dynamic inode
  0, "",			// name
  S_IFDIR | S_IRUGO | S_IXUGO,
  2, 0, 0,			// nlink, uid, gid
  0, &proc_dir_inode_operations,
  NULL, NULL,
  NULL,
  NULL, NULL
};

//
// ELEMENT NAME SYMLINK OPERATIONS
//

static int
proc_elementlink_readlink_proc(proc_dir_entry *pde, char *page)
{
  // must add 1 to (int)pde->data as directories are numbered from 1
  int pos = 0;
  proc_dir_entry *parent = pde->parent;
  while (parent != &proc_click_entry) {
    sprintf(page + pos, "../");
    pos += 3;
    parent = parent->parent;
  }
  return pos + sprintf(page + pos, "%d", (int)pde->data + 1);
}

static struct proc_dir_entry proc_element_elemdir_link_prototype = {
  0,
  0, "",
  S_IFLNK | S_IRUGO | S_IWUGO | S_IXUGO,
  1, 0, 0,
  64, NULL,			// size, inode operations
  NULL, NULL,			// get_info, fill_inode
  NULL, NULL, NULL,		// next, parent, subdir
  NULL,				// data
  NULL, NULL,			// read_proc, write_proc
  proc_elementlink_readlink_proc // readlink_proc
};


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
    NULL,			// select
    proc_element_handler_ioctl,	// ioctl
    NULL,			// mmap
    proc_element_handler_open,	// open
    NULL,			// flush
    proc_element_handler_release, // release
    NULL			// fsync
};

static struct inode_operations proc_element_handler_inode_operations;

static struct proc_dir_entry proc_element_read_handler_prototype = {
  0,				// dynamic inode
  0, "",			// name
  S_IFREG | S_IRUGO,
  1, 0, 0,			// nlink, uid, gid
  0, &proc_element_handler_inode_operations,
  NULL, NULL,
  NULL, NULL, NULL,
};

static struct proc_dir_entry proc_element_write_handler_prototype = {
  0,				// dynamic inode
  0, "",			// name
  S_IFREG | S_IWUSR | S_IWGRP,
  1, 0, 0,			// nlink, uid, gid
  0, &proc_element_handler_inode_operations,
  NULL, NULL,
  NULL, NULL, NULL,
};

static struct proc_dir_entry proc_element_read_write_handler_prototype = {
  0,				// dynamic inode
  0, "",			// name
  S_IFREG | S_IRUGO | S_IWUSR | S_IWGRP,
  1, 0, 0,			// nlink, uid, gid
  0, &proc_element_handler_inode_operations,
  NULL, NULL,
  NULL, NULL, NULL,
};

// OPERATIONS

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
prepare_handler_read(int eindex, int handlerno, String **store)
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
  
  *store = new String(s);
  if (!*store)
    return -ENOMEM;
  
  return 0;
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
finish_handler_write(int eindex, int handlerno, String *s)
{
  Element *e = (eindex >= 0 ? current_router->element(eindex) : 0);
  const Router::Handler *h = find_handler(eindex, handlerno);
  if (!h)
    return -ENOENT;
  else if (!h->write)
    return -EPERM;
  
  String context_string = "In write handler `" + h->name + "'";
  if (e) context_string += String(" for `") + e->declaration() + "'";
  ContextErrorHandler cerrh(kernel_errh, context_string + ":");
  
  // prevent interrupts
  unsigned cli_flags;
  save_flags(cli_flags);
  cli();

  int result = h->write(*s, e, h->write_thunk, &cerrh);

  // restore interrupts
  restore_flags(cli_flags);
  
  return result;
}

static int
parent_proc_dir_eindex(proc_dir_entry *pde)
{
  if (pde->parent == &proc_click_entry)
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
  if (eindex < -1) return eindex;
  
  String *s = 0;
  int retval;
  if (writing) {
    retval = prepare_handler_write(eindex, (int)pde->data);
    if (retval >= 0) {
      s = new String;
      if (!s) return -ENOMEM;
    }
  } else
    retval = prepare_handler_read(eindex, (int)pde->data, &s);
  
  if (retval >= 0) {
    filp->private_data = (void *)s;
    MOD_INC_USE_COUNT;
    return 0;
  } else {
    filp->private_data = 0;
    return retval;
  }
}

static ssize_t
proc_element_handler_read(struct file *filp, char *buffer, size_t count, loff_t *store_f_pos)
{
  loff_t f_pos = *store_f_pos;
  String *s = (String *)filp->private_data;
  if (!s)
    return -ENOMEM;
  if (f_pos + count > s->length())
    count = s->length() - f_pos;
  if (copy_to_user(buffer, s->data() + f_pos, count) > 0)
    return -EFAULT;
  *store_f_pos += count;
  return count;
}

static ssize_t
proc_element_handler_write(struct file *filp, const char *buffer, size_t count, loff_t *store_f_pos)
{
  loff_t f_pos = *store_f_pos;
  String *s = (String *)filp->private_data;
  if (!s)
    return -ENOMEM;
  int old_length = s->length();
  
  if (f_pos + count > LARGEST_HANDLER_WRITE)
    return -EFBIG;
  
  if (f_pos + count > old_length) {
    int out_of_memory = String::out_of_memory_count();
    s->append(0, f_pos + count - old_length);
    if (String::out_of_memory_count() != out_of_memory)
      return -ENOMEM;
  }
  
  int length = s->length();
  if (f_pos > length)
    return -EFBIG;
  else if (f_pos + count > length)
    count = length - f_pos;
  
  char *data = s->mutable_data();
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
  int retval = 0;
  
  if (writing) {
    proc_dir_entry *pde = (proc_dir_entry *)filp->f_dentry->d_inode->u.generic_ip;
    int eindex = parent_proc_dir_eindex(pde);
    if (eindex < -1)
      retval = eindex;
    else
      retval = finish_handler_write(eindex, (int)(pde->data),
				    (String *)filp->private_data);
  }
  
  delete ((String *)filp->private_data);
  MOD_DEC_USE_COUNT;
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
  if (h->read && h->write)
    pattern = &proc_element_read_write_handler_prototype;
  else if (h->write)
    pattern = &proc_element_write_handler_prototype;
  else if (h->read)
    pattern = &proc_element_read_handler_prototype;
  else
    return;
  click_register_new_dynamic_pde
    (directory, pattern, h->name.length(), h->name.cc(), (void *)handlerno);
}

//
// ROUTER ELEMENT PROCS
//

void
cleanup_router_element_procs()
{
  if (!current_router) return;
  int nelements = current_router->nelements();
  for (int i = 0; i < 2*nelements; i++)
    if (element_pdes[i])
      click_kill_dynamic_pde(element_pdes[i]);
  kfree(element_pdes);
  element_pdes = 0;
}

static void
make_compound_element_symlink(int fi)
{
  // must use substrings of `id' -- otherwise not guaranteed the char data
  // will stick around
  const String &id = current_router->element(fi)->id();
  const char *data = id.data();
  int len = id.length();
  if (len == 0)
    // empty name; do nothing
    return;

  // looking in `parent_dir'. if we create any proc_dir_entry at top level --
  // under `proc_click_entry' -- we must save it in
  // element_pdes[fi+nelements]. otherwise it will not be removed when the
  // router is destroyed.
  proc_dir_entry *parent_dir = &proc_click_entry;
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
      subdir = click_register_new_dynamic_pde
	(parent_dir, &proc_element_elemdir_prototype,
	 last_pos - first_pos, data + first_pos, (void *)0);
      if (parent_dir == &proc_click_entry)
	element_pdes[fi + nelements] = subdir;
    } else if (subdir->ops != &proc_dir_inode_operations)
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
  proc_dir_entry *link = click_register_new_dynamic_pde
    (parent_dir, &proc_element_elemdir_link_prototype,
     last_pos - first_pos, data + first_pos, (void *)fi);
  if (parent_dir == &proc_click_entry)
    element_pdes[fi + nelements] = link;
}

void
init_router_element_procs()
{
  int nelements = current_router->nelements();
  
  int ndigits = 2;
  int top;
  for (top = 10; top <= nelements; top *= 10)
    ndigits++;
  if (ndigits != numbers_ndigits) {
    kfree(numbers);
    numbers = (char *)kmalloc(ndigits * top, GFP_ATOMIC);
    if (numbers) {
      for (int i = 1; i < top; i++)
	sprintf(numbers + (i-1)*ndigits, "%d", i);
      numbers_ndigits = ndigits;
    } else
      numbers_ndigits = 0;
  }
  
  if (!numbers) return;
  
  element_pdes = (proc_dir_entry **)
    kmalloc(sizeof(proc_dir_entry *) * 2 * nelements, GFP_ATOMIC);
  if (!element_pdes) return;
  for (int i = 0; i < 2*nelements; i++)
    element_pdes[i] = 0;

  // make EINDEX directories
  int namelen = 1, next_namegap = 10;
  for (int i = 0; i < nelements; i++) {
    if (i+1 >= next_namegap) namelen++, next_namegap *= 10;
    element_pdes[i] = click_register_new_dynamic_pde
      (&proc_click_entry, &proc_element_elemdir_prototype,
       namelen, numbers + numbers_ndigits * i, (void *)i);
  }
  
  // add symlinks for ELEMENTNAME -> EINDEX
  for (int i = 0; i < nelements; i++)
    if (proc_dir_entry *pde = element_pdes[i]) {
      const String &id = current_router->element(i)->id();
      if (memchr(id.data(), '/', id.length()) != 0)
	make_compound_element_symlink(i);
      else {
	if (click_find_pde(&proc_click_entry, id)) continue;
	element_pdes[i + nelements] = click_register_new_dynamic_pde
	  (&proc_click_entry, &proc_element_elemdir_link_prototype,
	   id.length(), id.data(), (void *)i);
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
  kfree(numbers);
}
