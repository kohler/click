/*
 * proc_config.cc -- support /proc/click/config
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
#include "straccum.hh"
#include "router.hh"

static int config_write_lock = 0;
static String *current_config = 0;
static StringAccum *build_config = 0;

//
// DECLARE FILE OPERATIONS
//

extern "C" {

static int click_config_open(struct inode *, struct file *);
static DECLARE_RELEASE_FILEOP(click_config_release);
static DECLARE_READ_FILEOP(click_config_read);
static DECLARE_WRITE_FILEOP(click_config_write);

static struct file_operations proc_click_config_operations = {
    NULL,			// lseek
    click_config_read,		// read
    click_config_write,		// write
    NULL,			// readdir
    NULL,			// select
    NULL,			// ioctl
    NULL,			// mmap
    click_config_open,		// open
    NULL,			// flush
    click_config_release,	// release
    NULL			// fsync
};

static struct inode_operations proc_click_config_inode_operations;

static struct proc_dir_entry proc_click_config_entry = {
  0,				// dynamic inode
  6, "config",
  S_IFREG | S_IRUGO | S_IWUSR | S_IWGRP,
  1, 0, 0,			// nlink, uid, gid
  0, &proc_click_config_inode_operations, // inode size, operations
};

static struct proc_dir_entry proc_click_hotconfig_entry = {
  0,				// dynamic inode
  9, "hotconfig",
  S_IFREG | S_IWUSR | S_IWGRP,
  1, 0, 0,			// nlink, uid, gid
  0, &proc_click_config_inode_operations, // inode size, operations
};


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
      || (!writing && filp->f_dentry->d_inode->i_ino ==
	  proc_click_hotconfig_entry.low_ino))
    return -EACCES;
  
  if (writing) {
    if (config_write_lock)
      return -EBUSY;
    if (!build_config)
      build_config = new StringAccum;
    if (!build_config)
      return -ENOMEM;
    build_config->clear();
    if (!build_config->reserve(1024))
      return -ENOMEM;
    config_write_lock = 1;
  }
  
  MOD_INC_USE_COUNT;
  return 0;
}

static
DECLARE_READ_FILEOP(click_config_read)
{
  loff_t f_pos = FILEOP_F_POS;
  loff_t len = current_config->length();
  if (!current_config || f_pos > len)
    return 0;
  if (f_pos + count > len)
    count = len - f_pos;
  if (copy_to_user(buffer, current_config->data() + f_pos, count) > 0)
    return -EFAULT;
  FILEOP_F_POS += count;
  return count;
}

static
DECLARE_WRITE_FILEOP(click_config_write)
{
  loff_t f_pos = FILEOP_F_POS;
  
  if (!build_config)
    return -ENOMEM;

  loff_t last_len = build_config->length();
  loff_t end_pos = f_pos + count;
  char *x;
  if (end_pos > last_len)
    x = build_config->extend(end_pos - last_len);
  else
    x = build_config->data() + f_pos;
  if (!x)
    return -ENOMEM;
  
  if (f_pos > last_len)
    memset(x, 0, f_pos - last_len);
  if (copy_from_user(x + (f_pos - last_len), buffer, count) > 0)
    return -EFAULT;

  FILEOP_F_POS += count;
  return count;
}

static int
hotswap_config()
{
  String s = build_config->take_string();
  Router *r = parse_router(s);
  if (!r)
    return -EINVAL;
  
  /* prevent interrupts */
  unsigned cli_flags;
  save_flags(cli_flags);
  cli();

  if (r->initialize(kernel_errh) >= 0) {
    // perform hotswap
    if (current_router && current_router->initialized())
      r->take_state(current_router, kernel_errh);
    // install
    kill_current_router();
    install_current_router(r);
    *current_config = s;
  } else
    delete r;
  
  /* allow interrupts */
  restore_flags(cli_flags);
  return 0;
}

static int
swap_config()
{
  *current_config = build_config->take_string();
  kill_current_router();
  Router *router = parse_router(*current_config);
  if (router) {
    router->initialize(kernel_errh);
    install_current_router(router);
    return router->initialized() ? 0 : -EINVAL;
  } else
    return -EINVAL;
}

static
DECLARE_RELEASE_FILEOP(click_config_release)
{
  bool writing = (filp->f_flags & O_ACCMODE) == O_WRONLY;
  if (!writing) {
    MOD_DEC_USE_COUNT;
    return 0;
  }
  if (!config_write_lock)
    return -EIO;

  int success = -EINVAL;
  if (build_config) {
    if (!current_config)
      current_config = new String;
    if (current_config) {
      reset_proc_click_errors();
      unsigned inum = filp->f_dentry->d_inode->i_ino;
      if (inum == proc_click_hotconfig_entry.low_ino)
	success = hotswap_config();
      else
	success = swap_config();
      proc_click_config_entry.size = current_config->length();
    }
  }
  config_write_lock = 0;
  MOD_DEC_USE_COUNT;
  return success;
}

}


void
init_proc_click_config()
{
  // work around proc_lookup not being exported
  proc_click_config_inode_operations = proc_dir_inode_operations;
  proc_click_config_inode_operations.default_file_ops = &proc_click_config_operations;
  click_register_pde(&proc_click_entry, &proc_click_config_entry);
  click_register_pde(&proc_click_entry, &proc_click_hotconfig_entry);
}

void
cleanup_proc_click_config()
{
  delete current_config;
  delete build_config;
}
