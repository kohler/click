/*
 * proc_config.cc -- support /proc/click/config
 * Eddie Kohler
 *
 * Copyright (c) 1999-2000 Massachusetts Institute of Technology
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
#include <click/straccum.hh>
#include <click/router.hh>
#include <click/error.hh>

static atomic_t config_write_lock;
static String *current_config = 0;
static StringAccum *build_config = 0;

//
// DECLARE FILE OPERATIONS
//

extern "C" {

static int click_config_open(struct inode *, struct file *);
static int click_config_flush(struct file *);
static ssize_t click_config_read(struct file *, char *, size_t, loff_t *);
static ssize_t click_config_write(struct file *, const char *, size_t, loff_t *);
static unsigned click_config_poll(struct file *, struct poll_table_struct *);

static struct file_operations proc_click_config_operations = {
    NULL,			// lseek
    click_config_read,		// read
    click_config_write,		// write
    NULL,			// readdir
    click_config_poll,		// poll
    NULL,			// ioctl
    NULL,			// mmap
    click_config_open,		// open
    click_config_flush,		// flush
    NULL,			// release
    NULL			// fsync
};

static struct proc_dir_entry *proc_click_config_entry;
static struct proc_dir_entry *proc_click_hotconfig_entry;
  
static struct inode_operations proc_click_config_inode_operations;

static struct wait_queue *proc_click_config_wait_queue = 0;


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
  
  /* prevent interrupts */
  unsigned cli_flags;
  save_flags(cli_flags);
  cli();

  if (kernel_errh->nerrors() == before_errors
      && r->initialize(kernel_errh) >= 0) {
    // perform hotswap
    if (current_router && current_router->initialized())
      r->take_state(current_router, kernel_errh);
    // install
    kill_current_router();
    install_current_router(r);
    set_current_config(s);
  } else
    delete r;
  
  /* allow interrupts */
  restore_flags(cli_flags);
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
    if (kernel_errh->nerrors() == before_errors)
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
  if (!writing || filp->f_count > 1)
    return 0;
  
  if (atomic_read(&config_write_lock) == 0)
    return -EIO;
  int success = -EINVAL;
  
  if (build_config && current_config) {
    reset_proc_click_errors();
    unsigned my_ino = filp->f_dentry->d_inode->i_ino;

    if ((my_ino & 0xFFFF) == proc_click_hotconfig_entry->low_ino)
      success = hotswap_config();
    else
      success = swap_config();
    
    proc_click_config_entry->size = current_config->length();
  }
  
  atomic_set(&config_write_lock, 0);
  return success;
}

}


void
init_proc_click_config()
{
  atomic_set(&config_write_lock, 0);
  
  // work around proc_lookup not being exported
  proc_click_config_inode_operations = proc_dir_inode_operations;
  proc_click_config_inode_operations.default_file_ops = &proc_click_config_operations;
  
  proc_click_config_entry = create_proc_entry("config", S_IFREG | proc_click_mode_r | proc_click_mode_w, proc_click_entry);
  proc_click_hotconfig_entry = create_proc_entry("hotconfig", S_IFREG | proc_click_mode_w, proc_click_entry);
  // XXX memory exhaustion?
  proc_click_config_entry->ops = &proc_click_config_inode_operations;
  proc_click_hotconfig_entry->ops = &proc_click_config_inode_operations;
  
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
