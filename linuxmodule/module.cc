/*
 * module.cc -- Linux kernel module main program
 * Eddie Kohler, Robert Morris
 *
 * Copyright (c) 1999-2000 Massachusetts Institute of Technology
 * Copyright (c) 2000 Mazu Networks, Inc.
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

#include <click/skbmgr.hh>
#include <click/lexer.hh>
#include <click/router.hh>
#include <click/straccum.hh>
#include <click/confparse.hh>
#include <click/error.hh>
#include <click/bighashmap_arena.hh>

int click_mode_r, click_mode_w, click_mode_x, click_mode_dir;

extern "C" int click_accessible();
extern "C" int click_cleanup_packages();

ErrorHandler *click_logged_errh = 0;
Router *click_router = 0;


/***************************** Global handlers *******************************/

static String
read_cycles(Element *, void *)
{
  StringAccum sa;
  sa << click_get_cycles() << " cycles\n";
  return sa.take_string();
}

#ifdef HAVE_LINUX_READ_NET_SKBCOUNT
extern "C" int read_net_skbcount(void);
#endif

static String
read_meminfo(Element *, void *)
{
  extern int click_new_count; /* glue.cc */
  extern int click_outstanding_news; /* glue.cc */
  StringAccum sa;
  sa << "outstanding news " << click_outstanding_news << "\n";
  sa << "news " << click_new_count << "\n";
#ifdef HAVE_LINUX_READ_NET_SKBCOUNT
  sa << "net_skbcount " << read_net_skbcount() << "\n";
#endif
  return sa.take_string();
}

static String
read_packages(Element *, void *)
{
  StringAccum sa;
  Vector<String> v;
  click_public_packages(v);
  for (int i = 0; i < v.size(); i++)
    sa << v[i] << "\n";
  return sa.take_string();
}


/****************************** Error handlers *******************************/

class KernelErrorHandler : public BaseErrorHandler { public:
  KernelErrorHandler(bool log)		: _log(log) { }
  void handle_text(Seriousness, const String &);
 private:
  bool _log;
};

static StringAccum *error_log;

void
KernelErrorHandler::handle_text(Seriousness seriousness, const String &message)
{
  // print message to syslog
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

  // log message if required
  if (_log && error_log)
    *error_log << message << "\n";
  
  if (seriousness >= ERR_MIN_FATAL)
    panic("click");
}

static String
read_errors(Element *, void *)
{
  if (error_log)
    // OK to return a stable_string, even though the data is not really
    // stable, because we use it for a very short time (HANDLER_REREAD).
    // Problems are possible, of course.
    return String::stable_string(error_log->data(), error_log->length());
  else
    return String::out_of_memory_string();
}

void
click_clear_error_log()
{
  if (error_log)
    error_log->clear();
}



/******************** Module initialization and cleanup **********************/

static ErrorHandler *syslog_errh;

extern "C" int
init_module()
{
#ifdef HAVE_PROC_CLICK
  // check for another click module already existing
  for (proc_dir_entry *de = proc_root.subdir; de; de = de->next)
    if (de->low_ino && de->namelen == 5 && memcmp(de->name, "click", 5) == 0) {
      printk("<1>Some Click module is already installed! You must remove it.\n");
      return -EBUSY;
    }
#endif
  
  // C++ static initializers
  String::static_initialize();
  cp_va_static_initialize();

  // error initialization
  syslog_errh = new KernelErrorHandler(false);
  click_logged_errh = new KernelErrorHandler(true);
  ErrorHandler::static_initialize(new LandmarkErrorHandler(syslog_errh, "chatter"));
  error_log = new StringAccum;

  // default provisions
  CLICK_DEFAULT_PROVIDES;
  Router::static_initialize();

  // thread manager, sk_buff manager, config manager
  click_init_sched();
  skbmgr_init();
  click_init_config();
  
  // global handlers
  Router::add_read_handler(0, "packages", read_packages, 0);
  Router::add_read_handler(0, "meminfo", read_meminfo, 0);
  Router::add_read_handler(0, "cycles", read_cycles, 0);
  Router::add_read_handler(0, "errors", read_errors, 0);
  Router::change_handler_flags(0, "errors", 0, HANDLER_REREAD);

  // filesystem interface
  // set modes based on 'accessible'
  if (click_accessible()) {
    click_mode_r = S_IRUSR | S_IRGRP | S_IROTH;
    click_mode_x = S_IXUSR | S_IXGRP | S_IXOTH;
  } else {
    click_mode_r = S_IRUSR | S_IRGRP;
    click_mode_x = S_IXUSR | S_IXGRP;
  }
  click_mode_w = S_IWUSR | S_IWGRP;
  click_mode_dir = S_IFDIR | click_mode_r | click_mode_x;

#ifdef HAVE_CLICKFS
  init_clickfs();
#endif
#ifdef HAVE_PROC_CLICK
  init_proc_click();
#endif

  return 0;
}

void click_dmalloc_cleanup();

extern "C" void
cleanup_module()
{
  extern int click_new_count; /* glue.cc */
  extern int click_outstanding_news; /* glue.cc */
  
  // filesystem interface
#ifdef HAVE_CLICKFS
  cleanup_clickfs();
#endif
#ifdef HAVE_PROC_CLICK
  cleanup_proc_click();
#endif

  // extra packages, global handlers
  click_cleanup_packages();
  Router::static_cleanup();
  
  // config manager, thread manager, sk_buff manager
  click_cleanup_config();
  click_cleanup_sched();
  skbmgr_cleanup();

  cp_va_static_cleanup();

  // error handlers
  ErrorHandler::static_cleanup();
  delete click_logged_errh;
  delete syslog_errh;
  delete error_log;
  click_logged_errh = syslog_errh = 0;
  
  printk("<1>click module exiting\n");

  // HashMap
  BigHashMap_ArenaFactory::static_cleanup();
  
  // String (after any operations that might create Strings)
  String::static_cleanup();

  // report memory leaks
  if (Element::nelements_allocated)
    printk("<1>click error: %d elements still allocated\n", Element::nelements_allocated);
  if (click_outstanding_news) {
    printk("<1>click error: %d outstanding news\n", click_outstanding_news);
    click_dmalloc_cleanup();
  }
#ifdef HAVE_LINUX_READ_NET_SKBCOUNT
  printk("<1>net_skbcount: %d\n", read_net_skbcount());
#endif
}

#ifdef MODULE_LICENSE
MODULE_LICENSE("Dual BSD/GPL");
#endif
