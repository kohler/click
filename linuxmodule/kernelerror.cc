/*
 * kernelerror.{cc,hh} -- ErrorHandler subclass that saves errors for
 * /proc/click/errors and reports them with printk()
 * Eddie Kohler
 *
 * Copyright (c) 1999-2000 Massachusetts Institute of Technology
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
#define WANT_MOD_USE_COUNT 1	/* glue.hh should define MOD_USE_COUNTs */
#include "modulepriv.hh"

#include "kernelerror.hh"
#include <click/straccum.hh>

static StringAccum *all_errors = 0;

static struct file_operations proc_click_errors_operations;
static struct proc_dir_entry *proc_click_errors_entry;
#ifdef LINUX_2_2
static struct inode_operations proc_click_errors_inode_operations;
static struct wait_queue *proc_click_errors_wait_queue = 0;
#else
static wait_queue_head_t proc_click_errors_wait_queue;
#endif


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
  // set up proc_click_errors_operations
#ifdef LINUX_2_4
  proc_click_errors_operations.owner = THIS_MODULE;
#endif
  proc_click_errors_operations.read = click_errors_read;
  proc_click_errors_operations.poll = click_errors_poll;

  proc_click_errors_entry = create_proc_entry("errors", S_IFREG | proc_click_mode_r, proc_click_entry); // XXX error checking
#ifdef LINUX_2_2
  proc_click_errors_inode_operations = proc_dir_inode_operations;
  proc_click_errors_inode_operations.default_file_ops = &proc_click_errors_operations;
  proc_click_errors_entry->ops = &proc_click_errors_inode_operations;
#else
  proc_click_errors_entry->proc_fops = &proc_click_errors_operations;
#endif

#ifdef LINUX_2_4
  init_waitqueue_head(&proc_click_errors_wait_queue);
#endif
  
  all_errors = new StringAccum;
}

void
cleanup_proc_click_errors()
{
  remove_proc_entry("errors", proc_click_entry);
  reset_proc_click_errors();
  delete all_errors;
}
