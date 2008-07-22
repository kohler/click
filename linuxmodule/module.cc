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
#include <click/bighashmap_arena.hh>
#include <click/notifier.hh>
#include <click/nameinfo.hh>

extern "C" int click_cleanup_packages();

click_fsmode_t click_fsmode;
KernelErrorHandler *click_logged_errh = 0;
static KernelErrorHandler *syslog_errh = 0;
Router *click_router = 0;
Master *click_master = 0;


/***************************** Global handlers *******************************/

#if HAVE_INT64_TYPES
static String
read_cycles(Element *, void *)
{
  StringAccum sa;
  sa << click_get_cycles() << " cycles\n";
  return sa.take_string();
}
#endif

#ifdef HAVE_LINUX_READ_NET_SKBCOUNT
extern "C" int read_net_skbcount(void);
#endif

static String
read_meminfo(Element *, void *)
{
    extern size_t click_dmalloc_curnew, click_dmalloc_totalnew;
#if CLICK_DMALLOC
    extern size_t click_dmalloc_curmem, click_dmalloc_maxmem;
#endif
    StringAccum sa;
    sa << "outstanding news " << click_dmalloc_curnew << "\n"
       << "news " << click_dmalloc_totalnew << "\n";
#if CLICK_DMALLOC
    sa << "current allocated mem " << click_dmalloc_curmem << '\n'
       << "max allocated mem " << click_dmalloc_maxmem << '\n';
#endif
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


/******************************* Assertions **********************************/

#if HAVE_KERNEL_ASSERT
static bool assert_stops_router = false;
#endif

extern "C" void
click_assert_failed(const char *file, int line, const char *problem_text)
{
  click_chatter("%s:%d: assertion failed: %s", file, line, problem_text);
#if HAVE_KERNEL_ASSERT
  if (assert_stops_router) {
    if (click_router) {
      click_chatter("%s:%d: assertion failed: Asking router to stop", file, line);
      click_router->set_runcount(Router::STOP_RUNCOUNT);
    } else
      click_chatter("%s:%d: assertion failed: No router to stop", file, line);
  }
#endif
}

#if HAVE_KERNEL_ASSERT
static String
read_assert_stop(Element *, void *)
{
  return (assert_stops_router ? "true\n" : "false\n");
}

static int
write_assert_stop(const String &s, Element *, void *, ErrorHandler *errh)
{
  bool stop;
  if (!cp_bool(s, &stop))
    return errh->error("assert_stop must be a boolean");
  else {
    assert_stops_router = stop;
    return 0;
  }
}
#endif


/****************************** Error handlers *******************************/

void
KernelErrorHandler::log_line(const char *begin, const char *end)
{
  static_assert(LOGBUF_SIZ == LOGBUF_SAVESIZ * 2);

  // ensure begin <= end
  if (begin > end)
    begin = end;
  
  // skip "chatter: " for message log
  if (begin + 9 <= end && memcmp(begin, "chatter: ", 9) == 0)
    begin += 9;

  // manipulate log buffer to prevent memory overflow
  if (_pos + end - begin > LOGBUF_SIZ - 1 && _pos >= LOGBUF_SAVESIZ) {
    memcpy(&_logbuf[0], &_logbuf[LOGBUF_SAVESIZ], _pos - LOGBUF_SAVESIZ);
    _pos -= LOGBUF_SAVESIZ;
    _generation++;
  }
  if (_pos + end - begin > LOGBUF_SIZ - 1) {
    _pos = 0;
    _generation += 2;
  }
  if (_pos + end - begin > LOGBUF_SIZ - 1)
    begin = end - (LOGBUF_SIZ - 1);

  // log line
  memcpy(&_logbuf[_pos], begin, end - begin);
  _pos += end - begin;
  _logbuf[_pos++] = '\n';
}

void
KernelErrorHandler::handle_text(Seriousness seriousness, const String &message)
{
  // print message to syslog
  const char *begin = message.begin();
  const char *end = message.end();
  while (begin < end) {
    const char *newline = find(begin, end, '\n');
    printk("<1>%.*s\n", newline - begin, begin);
    log_line(begin, newline);
    begin = newline + 1;
  }

  // panic on fatal errors
  if (seriousness >= ERR_MIN_FATAL)
    panic("click");
}

inline String
KernelErrorHandler::stable_string() const
{
  return String::stable_string(&_logbuf[0], &_logbuf[_pos]);
}

static String
read_errors(Element *, void *thunk)
{
  KernelErrorHandler *errh = (thunk ? syslog_errh : click_logged_errh);
  if (errh)
    // OK to return a stable_string, even though the data is not really
    // stable, because we use it for a very short time (HANDLER_REREAD).
    // Problems are possible, of course.
    return errh->stable_string();
  else
    return String::out_of_memory_string();
}

void
click_clear_error_log()
{
  if (click_logged_errh)
    click_logged_errh->clear_log();
  if (syslog_errh)
    syslog_errh->clear_log();
}



/******************** Module initialization and cleanup **********************/

extern "C" int
init_module()
{
  // C++ static initializers
  NameInfo::static_initialize();
  cp_va_static_initialize();

  // error initialization
  syslog_errh = new KernelErrorHandler;
  click_logged_errh = new KernelErrorHandler;
  ErrorHandler::static_initialize(new LandmarkErrorHandler(syslog_errh, "chatter"));

  // default provisions
  Router::static_initialize();
  NotifierSignal::static_initialize();
  CLICK_DEFAULT_PROVIDES;

  // thread manager, sk_buff manager, config manager
  click_init_sched(ErrorHandler::default_handler());
  skbmgr_init();
  click_init_config();
  
  // global handlers
  Router::add_read_handler(0, "packages", read_packages, 0);
  Router::add_read_handler(0, "meminfo", read_meminfo, 0);
#if HAVE_INT64_TYPES
  Router::add_read_handler(0, "cycles", read_cycles, 0);
#endif
  Router::add_read_handler(0, "errors", read_errors, 0);
  Router::set_handler_flags(0, "errors", HANDLER_REREAD);
  Router::add_read_handler(0, "messages", read_errors, (void *)1);
  Router::set_handler_flags(0, "messages", HANDLER_REREAD);
#if HAVE_KERNEL_ASSERT
  Router::add_read_handler(0, "assert_stop", read_assert_stop, 0);
  Router::add_write_handler(0, "assert_stop", write_assert_stop, 0);
#endif

  // filesystem interface
  // set modes based on 'accessible'
  if (click_parm(CLICKPARM_ACCESSIBLE)) {
    click_fsmode.read = S_IRUSR | S_IRGRP | S_IROTH;
    click_fsmode.exec = S_IXUSR | S_IXGRP | S_IXOTH;
  } else {
    click_fsmode.read = S_IRUSR | S_IRGRP;
    click_fsmode.exec = S_IXUSR | S_IXGRP;
  }
  click_fsmode.write = S_IWUSR | S_IWGRP;
  click_fsmode.dir = S_IFDIR | click_fsmode.read | click_fsmode.exec;
  click_fsmode.uid = click_parm(CLICKPARM_UID);
  click_fsmode.gid = click_parm(CLICKPARM_GID);

  init_clickfs();

  return 0;
}

void click_dmalloc_cleanup();

extern "C" void
cleanup_module()
{
  extern size_t click_dmalloc_curnew; /* glue.cc */
  
  // filesystem interface
  cleanup_clickfs();

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
  click_logged_errh = syslog_errh = 0;
  
  printk("<1>click module exiting\n");

  // HashMap
  HashMap_ArenaFactory::static_cleanup();
  
  // String (after any operations that might destroy Strings)
  NameInfo::static_cleanup();

  // report memory leaks
  if (Element::nelements_allocated)
    printk("<1>click error: %d elements still allocated\n", Element::nelements_allocated);
  if (click_dmalloc_curnew) {
    printk("<1>click error: %d outstanding news\n", click_dmalloc_curnew);
    click_dmalloc_cleanup();
  }
#ifdef HAVE_LINUX_READ_NET_SKBCOUNT
  printk("<1>net_skbcount: %d\n", read_net_skbcount());
#endif
}

#ifdef MODULE_LICENSE
MODULE_LICENSE("Dual BSD/GPL");
#endif
