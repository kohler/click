/*
 * proc_config.cc -- support /proc/click/config
 * Eddie Kohler
 *
 * Copyright (c) 1999 Massachusetts Institute of Technology.
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

static int config_write_lock = 0;
static char *current_config = 0;
static int current_config_len = 0;

static char *build_config = 0;
static int build_config_len = 0;
static int build_config_cap = 0;

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
      || (writing && !(filp->f_flags & O_TRUNC)))
    return -EACCES;
  
  if (writing) {
    if (config_write_lock)
      return -EBUSY;
    build_config = kmalloc(1024, GFP_KERNEL);
    if (!build_config)
      return -ENOMEM;
    build_config_cap = 1024;
    build_config_len = 0;
    config_write_lock = 1;
  }
  
  MOD_INC_USE_COUNT;
  return 0;
}

static
DECLARE_RELEASE_FILEOP(click_config_release)
{
  bool writing = (filp->f_flags & O_ACCMODE) == O_WRONLY;
  
  if (writing) {
    if (!config_write_lock)
      return -EIO;
    if (build_config) {
      kfree(current_config);
      current_config = build_config;
      current_config_len = build_config_len;
      initialize_router(current_config, current_config_len);
      proc_click_config_entry.size = current_config_len;
    }
    config_write_lock = 0;
    MOD_DEC_USE_COUNT;
    return (current_router && current_router->initialized() ? 0 : -EINVAL);
    
  } else {
    MOD_DEC_USE_COUNT;
    return 0;
  }
}

static
DECLARE_READ_FILEOP(click_config_read)
{
  loff_t f_pos = FILEOP_F_POS;
  if (!current_config || f_pos > current_config_len)
    return 0;
  if (f_pos + count > current_config_len)
    count = current_config_len - f_pos;
  if (copy_to_user(buffer, current_config + f_pos, count) > 0)
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
  
  // XXX very long files -- integer overflow?
  
  if (f_pos + count > build_config_cap) {
    int newcap = build_config_cap + 1024;
    while (newcap < f_pos + count)
      newcap += 1024;
    char *new_build_config = kmalloc(newcap, GFP_KERNEL);
    if (!new_build_config) {
      kfree(build_config);
      build_config = 0;
      return -ENOMEM;
    }
    memcpy(new_build_config, build_config, build_config_len);
    kfree(build_config);
    build_config = new_build_config;
    build_config_cap = newcap;
  }
  
  if (f_pos > build_config_len)
    memset(build_config + build_config_len, 0, f_pos - build_config_len);
  
  if (copy_from_user(build_config + f_pos, buffer, count) > 0)
    return -EFAULT;
  
  if (f_pos + count > build_config_len)
    build_config_len = f_pos + count;
  FILEOP_F_POS += count;
  return count;
}

}


void
init_proc_click_config()
{
  // work around proc_lookup not being exported
  proc_click_config_inode_operations = proc_dir_inode_operations;
  proc_click_config_inode_operations.default_file_ops = &proc_click_config_operations;
  click_register_pde(&proc_click_entry, &proc_click_config_entry);
}

void
cleanup_proc_click_config()
{
  kfree(current_config);
  if (current_config != build_config)
    kfree(build_config);
}
