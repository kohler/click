/*
 * kernelerror.{cc,hh} -- ErrorHandler subclass that saves errors for
 * /proc/click/errors and reports them with printk()
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
#include "kernelerror.hh"
#include "modulepriv.hh"
#include "straccum.hh"

static StringAccum *all_errors = 0;

static ssize_t click_errors_read(struct file *, char *, size_t, loff_t *);

static struct file_operations proc_click_errors_operations = {
    NULL,			// lseek
    click_errors_read,	// read
    NULL,			// write
    NULL,			// readdir
    NULL,			// select
    NULL,			// ioctl
    NULL,			// mmap
    NULL,			// open
    NULL,			// flush
    NULL,			// release
    NULL			// fsync
};

static struct inode_operations proc_click_errors_inode_operations;

static struct proc_dir_entry proc_click_errors_entry = {
  0,				// dynamic inode
  6, "errors",
  S_IFREG | S_IRUGO,
  1, 0, 0,			// nlink, uid, gid
  0, &proc_click_errors_inode_operations, // inode size, operations
};


static void
syslog_message(const String &message)
{
  int pos = 0, nl;
  while ((nl = message.find_left('\n', pos)) >= 0) {
    String x = message.substring(pos, nl - pos);
    printk("<1>%s\n", x.cc());
    pos = nl + 1;
  }
  if (pos < message.length()) {
    String x = message.substring(pos);
    printk("<1>%s\n", x.cc());
  }
}

void
KernelErrorHandler::vmessage(Seriousness seriousness, const String &message)
{
  if (seriousness == Message) /* do nothing */;
  else if (seriousness == Warning) _nwarnings++;
  else _nerrors++;

  syslog_message(message);
  *all_errors << message << "\n";
  proc_click_errors_entry.size = all_errors->length();
  
  if (seriousness == Fatal)
    panic("KernelErrorHandler");
}

void
SyslogErrorHandler::vmessage(Seriousness seriousness, const String &message)
{
  syslog_message(message);
}


static ssize_t
click_errors_read(struct file *filp, char *buffer, size_t count, loff_t *store_f_pos)
{
  loff_t f_pos = *store_f_pos;
  if (!all_errors) return 0;
  if (f_pos > all_errors->length())
    return 0;
  if (f_pos + count > all_errors->length())
    count = all_errors->length() - f_pos;
  if (copy_to_user(buffer, all_errors->data() + f_pos, count) > 0)
    return -EFAULT;
  *store_f_pos += count;
  return count;
}


void
reset_proc_click_errors()
{
  all_errors->clear();
  proc_click_errors_entry.size = 0;
}

void
init_proc_click_errors()
{
  // work around proc_lookup not being exported
  proc_click_errors_inode_operations = proc_dir_inode_operations;
  proc_click_errors_inode_operations.default_file_ops = &proc_click_errors_operations;
  click_register_pde(&proc_click_entry, &proc_click_errors_entry);
  
  all_errors = new StringAccum;
}

void
cleanup_proc_click_errors()
{
  reset_proc_click_errors();
  delete all_errors;
}
