/*
 * kernelerror.{cc,hh} -- ErrorHandler subclass that saves errors for
 * /proc/click/errors and reports them with printk()
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
#include "kernelerror.hh"
#include "modulepriv.hh"
#include <click/straccum.hh>

static StringAccum *all_errors = 0;

static ssize_t click_errors_read(struct file *, char *, size_t, loff_t *);
static unsigned click_errors_poll(struct file *, struct poll_table_struct *);

static struct file_operations proc_click_errors_operations = {
    NULL,			// lseek
    click_errors_read,		// read
    NULL,			// write
    NULL,			// readdir
    click_errors_poll,		// poll
    NULL,			// ioctl
    NULL,			// mmap
    NULL,			// open
    NULL,			// flush
    NULL,			// release
    NULL			// fsync
};

static struct inode_operations proc_click_errors_inode_operations;
static struct proc_dir_entry *proc_click_errors_entry;
static struct wait_queue *proc_click_errors_wait_queue = 0;


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

static void
update_proc_click_errors()
{
  unsigned len = (all_errors ? all_errors->length() : 0);
  proc_click_errors_entry->size = len;
  
  // wake up anyone waiting for errors
  wake_up_interruptible(&proc_click_errors_wait_queue);
}


void
KernelErrorHandler::handle_text(Seriousness seriousness, const String &message)
{
  if (seriousness <= ERR_MESSAGE)
    /* do nothing */;
  else if (seriousness == ERR_WARNING)
    _nwarnings++;
  else
    _nerrors++;

  syslog_message(message);
  *all_errors << message << "\n";
  update_proc_click_errors();
  
  if (seriousness == ERR_FATAL)
    panic("KernelErrorHandler");
}

void
SyslogErrorHandler::handle_text(Seriousness seriousness, const String &message)
{
  syslog_message(message);
}


static ssize_t
click_errors_read(struct file *filp, char *buffer, size_t count, loff_t *store_f_pos)
{
  // exit if no errors
  if (!all_errors) return 0;
  
  loff_t f_pos = *store_f_pos;
  if (f_pos > all_errors->length())
    return 0;
  if (f_pos + count > all_errors->length())
    count = all_errors->length() - f_pos;
  if (copy_to_user(buffer, all_errors->data() + f_pos, count) > 0)
    return -EFAULT;
  *store_f_pos += count;
  return count;
}

static unsigned
click_errors_poll(struct file *filp, struct poll_table_struct *pollt)
{
  loff_t f_pos = filp->f_pos;
  unsigned mask = 0;
  if (all_errors && f_pos < all_errors->length())
    mask |= POLLIN | POLLRDNORM;
  poll_wait(filp, &proc_click_errors_wait_queue, pollt);
  return mask;
}


void
reset_proc_click_errors()
{
  all_errors->clear();
  update_proc_click_errors();
}

void
init_proc_click_errors()
{
  // work around proc_lookup not being exported
  proc_click_errors_inode_operations = proc_dir_inode_operations;
  proc_click_errors_inode_operations.default_file_ops = &proc_click_errors_operations;

  proc_click_errors_entry = create_proc_entry("errors", S_IFREG | proc_click_mode_r, proc_click_entry);
  // XXX error checking
  proc_click_errors_entry->ops = &proc_click_errors_inode_operations;
  
  all_errors = new StringAccum;
}

void
cleanup_proc_click_errors()
{
  remove_proc_entry("errors", proc_click_entry);
  reset_proc_click_errors();
  delete all_errors;
}
