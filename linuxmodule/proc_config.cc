/*
 * proc_config.cc -- support /proc/click/config
 * Eddie Kohler
 *
 * Copyright (c) 1999-2000 Massachusetts Institute of Technology
 * Copyright (c) 2000-2002 Mazu Networks, Inc.
 * Copyright (c) 2001-2002 International Computer Science Institute
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

#include <click/straccum.hh>
#include <click/router.hh>
#include <click/error.hh>

static atomic_t config_write_lock;
static String *current_config = 0;
static StringAccum *build_config = 0;
extern atomic_t click_config_generation;


//
// DECLARE FILE OPERATIONS
//

extern "C" {

static struct file_operations proc_click_config_operations;
#ifdef LINUX_2_2
static struct inode_operations proc_click_config_inode_operations;
static struct wait_queue *proc_click_config_wait_queue = 0;
#else
static wait_queue_head_t proc_click_config_wait_queue;
#endif

static struct proc_dir_entry *proc_click_config_entry;
static struct proc_dir_entry *proc_click_hotconfig_entry;


//
// CONFIG
//

static int
click_config_open(struct inode *, struct file *filp)
{
  // XXX race conditions
  bool writing = (filp->f_flags & O_ACCMODE) == O_WRONLY;
  if ((filp->f_flags & O_ACCMODE) == O_RDWR
      || (filp->f_flags & O_APPEND)
      || (writing && !(filp->f_flags & O_TRUNC))
      || (!writing && (filp->f_dentry->d_inode->i_ino & 0xFFFF) ==
	  proc_click_hotconfig_entry->low_ino))
    return -EACCES;
  
  if (writing) {
    if (atomic_read(&config_write_lock) > 0)
      return -EBUSY;
    if (!build_config)
      build_config = new StringAccum;
    if (!build_config)
      return -ENOMEM;
    build_config->clear();
    if (!build_config->reserve(1024))
      return -ENOMEM;
    atomic_set(&config_write_lock, 1);
  }
  
  return 0;
}

static ssize_t
click_config_read(struct file *filp, char *buffer, size_t count, loff_t *store_f_pos)
{
  loff_t f_pos = *store_f_pos;
  if (!current_config)
    return 0;
  loff_t len = current_config->length();
  if (f_pos > len)
    return 0;
  if (f_pos + count > len)
    count = len - f_pos;
  if (copy_to_user(buffer, current_config->data() + f_pos, count) > 0)
    return -EFAULT;
  *store_f_pos += count;
  return count;
}

static unsigned
click_config_poll(struct file *filp, struct poll_table_struct *pollt)
{
  loff_t f_pos = filp->f_pos;
  unsigned mask = 0;
  if ((filp->f_flags & O_ACCMODE) == O_WRONLY)
    mask |= POLLOUT | POLLWRNORM;
  else {
    if (current_config && f_pos < current_config->length())
      mask |= POLLIN | POLLRDNORM;
    poll_wait(filp, &proc_click_config_wait_queue, pollt);
  }
  return mask;
}

static ssize_t
click_config_write(struct file *filp, const char *buffer, size_t count, loff_t *store_f_pos)
{
  loff_t f_pos = *store_f_pos;
  
  if (!build_config)
    return -ENOMEM;

  loff_t last_len = build_config->length();
  loff_t end_pos = f_pos + count;
  if (end_pos > last_len && !build_config->extend(end_pos - last_len))
    return -ENOMEM;

  char *x = build_config->data() + f_pos;
  if (f_pos > last_len)
    memset(x, 0, f_pos - last_len);
  if (copy_from_user(x, buffer, count) > 0)
    return -EFAULT;

  *store_f_pos += count;
  return count;
}

static void
set_current_config(const String &s)
{
  *current_config = s;
  atomic_inc(&click_config_generation);

  // wake up anyone waiting for errors
  wake_up_interruptible(&proc_click_config_wait_queue);
}

static int
hotswap_config()
{
  String s = build_config->take_string();
  int before_errors = kernel_errh->nerrors();
  Router *r = parse_router(s);
  if (!r)
    return -EINVAL;

  // XXX should we lock the kernel?
  
  if (kernel_errh->nerrors() == before_errors
      && r->initialize(kernel_errh) >= 0) {
    // perform hotswap
    if (current_router && current_router->initialized()) {
      // turn off all threads on current router before you take_state
      if (kill_current_router_threads() >= 0) {
	printk("<1>click: performing hotswap\n");
	r->take_state(current_router, kernel_errh);
      }
    }
    // install
    kill_current_router();
    install_current_router(r);
    set_current_config(s);
  } else
    delete r;
  
  return 0;
}

static int
swap_config()
{
  set_current_config(build_config->take_string());
  kill_current_router();
  int before_errors = kernel_errh->nerrors();
  Router *router = parse_router(*current_config);
  if (router) {
    router->preinitialize();
    router->initialize(kernel_errh);
    install_current_router(router);
    return router->initialized() ? 0 : -EINVAL;
  } else
    return -EINVAL;
}

static int
click_config_flush(struct file *filp)
{
  bool writing = (filp->f_flags & O_ACCMODE) == O_WRONLY;
#ifdef LINUX_2_2
  if (!writing || filp->f_count > 1)
    return 0;
#else
  if (!writing || atomic_read(&filp->f_count) > 1)
    return 0;
#endif

  if (atomic_read(&config_write_lock) == 0)
    return -EIO;
  int success = -EINVAL;
  bool out_of_memory = (!build_config || build_config->out_of_memory());
  
  if (build_config && current_config && !out_of_memory) {
    reset_proc_click_errors();
    unsigned my_ino = filp->f_dentry->d_inode->i_ino;

    if ((my_ino & 0xFFFF) == proc_click_hotconfig_entry->low_ino)
      success = hotswap_config();
    else
      success = swap_config();
    
    proc_click_config_entry->size = current_config->length();
  }
  
  atomic_set(&config_write_lock, 0);
  return (out_of_memory ? -ENOMEM : success);
}

}


void
init_proc_click_config()
{
  atomic_set(&config_write_lock, 0);

#ifdef LINUX_2_4
  proc_click_config_operations.owner = THIS_MODULE;
#endif
  proc_click_config_operations.read = click_config_read;
  proc_click_config_operations.write = click_config_write;
  proc_click_config_operations.poll = click_config_poll;
  proc_click_config_operations.open = click_config_open;
  proc_click_config_operations.flush = click_config_flush;
  
  proc_click_config_entry = create_proc_entry("config", S_IFREG | proc_click_mode_r | proc_click_mode_w, proc_click_entry);
  proc_click_hotconfig_entry = create_proc_entry("hotconfig", S_IFREG | proc_click_mode_w, proc_click_entry);
  // XXX memory exhaustion?

#ifdef LINUX_2_2
  // work around proc_lookup not being exported
  proc_click_config_inode_operations = proc_dir_inode_operations;
  proc_click_config_inode_operations.default_file_ops = &proc_click_config_operations;
  proc_click_config_entry->ops = &proc_click_config_inode_operations;
  proc_click_hotconfig_entry->ops = &proc_click_config_inode_operations;
#else
  proc_click_config_entry->proc_fops = &proc_click_config_operations;
  proc_click_hotconfig_entry->proc_fops = &proc_click_config_operations;
#endif
  
#ifdef LINUX_2_4
  init_waitqueue_head(&proc_click_config_wait_queue);
#endif
  
  current_config = new String;
}

void
cleanup_proc_click_config()
{
  remove_proc_entry("config", proc_click_entry);
  remove_proc_entry("hotconfig", proc_click_entry);
  delete current_config;
  delete build_config;
}
