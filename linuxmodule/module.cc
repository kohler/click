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

#include "kernelerror.hh"
#include <click/skbmgr.hh>
#include <click/lexer.hh>
#include <click/router.hh>
#include <click/straccum.hh>
#include <click/confparse.hh>

proc_dir_entry *proc_click_entry = 0;
int proc_click_mode_r, proc_click_mode_w, proc_click_mode_x;
int proc_click_mode_dir;
extern "C" int click_accessible();
#if __MTCLICK__
extern "C" int click_threads();
#endif
extern "C" int click_cleanup_packages();

ErrorHandler *kernel_errh = 0;
static Lexer *lexer = 0;
Router *current_router = 0;


class LinuxModuleLexerExtra : public LexerExtra { public:
  LinuxModuleLexerExtra() { }
  void require(String, ErrorHandler *);
};

void
LinuxModuleLexerExtra::require(String r, ErrorHandler *errh)
{
  if (!click_has_provision(r))
    errh->error("unsatisfied requirement `%s'", r.cc());
}

Router *
parse_router(String s)
{
  LinuxModuleLexerExtra lextra;

  int cookie = lexer->begin_parse(s, "line ", &lextra);
  while (lexer->ystatement())
    /* do nothing */;
  
  Router *r = lexer->create_router();
  
  lexer->end_parse(cookie);
  return r;
}

void
kill_current_router()
{
  if (current_router) {
    current_router->please_stop_driver();
    // printk("<1>  killed\n");
    cleanup_router_element_procs();
    // printk("<1>  cleaned\n");
    current_router->unuse();
    // printk("<1>  deleted\n");
    current_router = 0;
  }
}

void
install_current_router(Router *r)
{
  current_router = r;
  r->use();
  init_router_element_procs();
#if __MTCLICK__
  if (r->initialized())
    start_click_sched(r, click_threads(), kernel_errh);
#else
  if (r->initialized())
    start_click_sched(r, 1, kernel_errh);
#endif
}

extern "C" void
click_cycle_counter(int which, unsigned int *fnp, unsigned long long *valp);

static String
read_cycles(Element *, void *)
{
  StringAccum sa;
  sa << click_get_cycles() << " cycles\n";
  return sa.take_string();
}

static String
read_version(Element *, void *)
{
  return String(CLICK_VERSION) + "\n";
}

#ifdef HAVE_READ_NET_SKBCOUNT
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
#ifdef HAVE_READ_NET_SKBCOUNT
  sa << "net_skbcount " << read_net_skbcount() << "\n";
#endif
  return sa.take_string();
}

static String
read_threads(Element *, void *)
{
  Vector<int> threads;
  get_click_thread_pids(threads);
  
  StringAccum sa;
  for (int i = 0; i < threads.size(); i++)
    sa << threads[i] << '\n';
  return sa.take_string();
}

static String
read_flatconfig(Element *, void *)
{
  if (current_router)
    return current_router->flat_configuration_string();
  else
    return "";
}

static String
read_list(Element *, void *)
{
  if (!current_router)
    return "0\n";
  else
    return current_router->element_list_string();
}

static String
read_classes(Element *, void *)
{
  Vector<String> v;
  lexer->element_type_names(v);
  StringAccum sa;
  for (int i = 0; i < v.size(); i++)
    sa << v[i] << "\n";
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

static String
read_requirements(Element *, void *)
{
  if (current_router) {
    const Vector<String> &v = current_router->requirements();
    StringAccum sa;
    for (int i = 0; i < v.size(); i++)
      sa << v[i] << "\n";
    return sa.take_string();
  } else
    return "";
}

#ifdef LINUX_2_2
# define MIN_PRIO	1
# define MAX_PRIO	(2 * DEF_PRIORITY)
# define PRIO2NICE(p)	(DEF_PRIORITY - (p))
# define NICE2PRIO(n)	(DEF_PRIORITY - (n))
# define TASK_PRIO(t)	((t)->priority)
#else
# define MIN_PRIO	(-20)
# define MAX_PRIO	19
# define PRIO2NICE(p)	(p)
# define NICE2PRIO(n)	(n)
# define TASK_PRIO(t)	((t)->nice)
#endif

static String
read_priority(Element *, void *)
{
  return String(PRIO2NICE(click_thread_priority)) + "\n";
}

static int
write_priority(const String &conf, Element *, void *, ErrorHandler *errh)
{
  int priority;
  if (!cp_integer(cp_uncomment(conf), &priority))
    return errh->error("priority must be integer");

  priority = NICE2PRIO(priority);
  if (priority < MIN_PRIO) {
    priority = MIN_PRIO;
    errh->warning("priority pinned at %d", PRIO2NICE(priority));
  } else if (priority > MAX_PRIO) {
    priority = MAX_PRIO;
    errh->warning("priority pinned at %d", PRIO2NICE(priority));
  }

  change_click_thread_priority(priority);
  
  return 0;
}


extern "C" int
click_add_element_type(const char *name, Element *e)
{
  return lexer->add_element_type(name, e);
}

extern "C" void
click_remove_element_type(int i)
{
  lexer->remove_element_type(i);
}


extern void export_elements(Lexer *);

static void
next_root_handler(const char *name, ReadHandler read, void *read_thunk,
		  WriteHandler write, void *write_thunk)
{
  if (read)
    Router::add_global_read_handler(name, read, read_thunk);
  if (write)
    Router::add_global_write_handler(name, write, write_thunk);
  register_handler(proc_click_entry, Router::find_global_handler(name));
}

static ErrorHandler *syslog_errh;

extern "C" int
init_module()
{
  // check for another click module already existing
  for (proc_dir_entry *de = proc_root.subdir; de; de = de->next)
    if (de->low_ino && de->namelen == 5 && memcmp(de->name, "click", 5) == 0) {
      printk("<1>Some Click module is already installed! You must remove it.\n");
      return -EBUSY;
    }
  
  // first call C++ static initializers
  String::static_initialize();
  cp_va_static_initialize();

  syslog_errh = new SyslogErrorHandler;
  kernel_errh = new KernelErrorHandler;
  ErrorHandler::static_initialize(new LandmarkErrorHandler(syslog_errh, "chatter"));
  
  init_click_sched();
  skbmgr_init();
  
  CLICK_DEFAULT_PROVIDES;
  lexer = new Lexer(kernel_errh);
  export_elements(lexer);
  
  current_router = 0;

  // set modes based on 'accessible'
  if (click_accessible()) {
    proc_click_mode_r = S_IRUSR | S_IRGRP | S_IROTH;
    proc_click_mode_x = S_IXUSR | S_IXGRP | S_IXOTH;
  } else {
    proc_click_mode_r = S_IRUSR | S_IRGRP;
    proc_click_mode_x = S_IXUSR | S_IXGRP;
  }
  proc_click_mode_w = S_IWUSR | S_IWGRP;
  proc_click_mode_dir = S_IFDIR | proc_click_mode_r | proc_click_mode_x;

  proc_click_entry = create_proc_entry("click", proc_click_mode_dir, 0);
  init_proc_click_config();
  init_proc_click_elements();
  init_proc_click_errors();

  // add handlers to the root directory. warning: this only works if there
  // is no current_router while the handlers are being added.
  next_root_handler("version", read_version, 0, 0, 0);
  next_root_handler("list", read_list, 0, 0, 0);
  next_root_handler("classes", read_classes, 0, 0, 0);
  next_root_handler("flatconfig", read_flatconfig, 0, 0, 0);
  next_root_handler("packages", read_packages, 0, 0, 0);
  next_root_handler("requirements", read_requirements, 0, 0, 0);
  next_root_handler("meminfo", read_meminfo, 0, 0, 0);
  next_root_handler("cycles", read_cycles, 0, 0, 0);
  next_root_handler("threads", read_threads, 0, 0, 0);
  next_root_handler("priority", read_priority, 0, write_priority, 0);

  return 0;
}

void print_and_free_chunks();

extern "C" void
cleanup_module()
{
  extern int click_new_count; /* glue.cc */
  extern int click_outstanding_news; /* glue.cc */
  
  kill_current_router();

  cleanup_proc_click_errors();
  cleanup_proc_click_elements();
  cleanup_proc_click_config();
  
  // remove root handlers
  for (int i = 0; i < Router::nglobal_handlers(); i++) {
    const Router::Handler &h = Router::global_handler(Router::FIRST_GLOBAL_HANDLER + i);
    remove_proc_entry(String(h.name()).cc(), proc_click_entry);
  }

  // remove the `/proc/click' directory first
  remove_proc_entry("click", 0);

  // invalidate any remaining `/proc/click' dentry, which would be hanging
  // around because someone has a handler open
#ifdef LINUX_2_2
  struct dentry *click_de = lookup_dentry("/proc/click", 0, LOOKUP_DIRECTORY);
#else
  struct dentry *click_de = lookup_one("/proc/click", 0); // XXX?
#endif
  if (!IS_ERR(click_de)) {
    d_drop(click_de);
    dput(click_de);
  }
  
  cleanup_click_sched();
  delete lexer;
  
  Router::cleanup_global_handlers();
  cp_va_static_cleanup();

  delete kernel_errh;
  delete syslog_errh;
  ErrorHandler::static_cleanup();
  kernel_errh = syslog_errh = 0;
  
  skbmgr_cleanup();
  click_cleanup_packages();
  
  printk("<1>click module exiting\n");
    
  // printk("<1>cycles: %s", read_cycles(0, 0).cc());
  
  // must call after all operations that might create strings are done
  String::static_cleanup();
  if (Element::nelements_allocated)
    printk("<1>click error: %d elements still allocated\n", Element::nelements_allocated);
  if (click_outstanding_news) {
    printk("<1>click error: %d outstanding news\n", click_outstanding_news);
    print_and_free_chunks();
  }
#ifdef HAVE_READ_NET_SKBCOUNT
  printk("<1>net_skbcount: %d\n", read_net_skbcount());
#endif
}
