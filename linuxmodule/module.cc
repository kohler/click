/*
 * module.cc -- Linux kernel module main program
 * Eddie Kohler, Robert Morris
 *
 * Copyright (c) 1999-2000 Massachusetts Institute of Technology
 * Copyright (c) 2000 Mazu Networks, Inc.
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

#include "lexer.hh"
#include "router.hh"
#include "kernelerror.hh"
#include "straccum.hh"
#include "confparse.hh"

static struct click_x_proc_dir_entry proc_click_x_entry = {
  0,				// dynamic inode
  5, "click",			// name
  S_IFDIR | S_IRUGO | S_IXUGO,
  2, 0, 0,			// nlink, uid, gid
  0, &proc_dir_inode_operations,
};
click_proc_dir_entry *proc_click_entry = reinterpret_cast<click_proc_dir_entry *>(&proc_click_x_entry);

ErrorHandler *kernel_errh = 0;
static Lexer *lexer = 0;
Router *current_router = 0;

Router::Handler *root_handlers;
int nroot_handlers = 0;

static Vector<String> *packages;


class LinuxModuleLexerExtra : public LexerExtra { public:
  LinuxModuleLexerExtra() { }
  void require(const String &, ErrorHandler *);
};

void
LinuxModuleLexerExtra::require(const String &r, ErrorHandler *errh)
{
  for (int i = 0; i < packages->size(); i++)
    if (packages->at(i) == r)
      return;
  errh->error("unsatisfied requirement `%s'", String(r).cc());
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
    kill_click_sched(current_router);
    //printk("<1>  killed\n");
    cleanup_router_element_procs();
    //printk("<1>  cleaned\n");
    current_router->unuse();
    //printk("<1>  deleted\n");
    current_router = 0;
  }
}

void
install_current_router(Router *r)
{
  current_router = r;
  r->use();
  init_router_element_procs();
  if (r->initialized())
    start_click_sched(r, kernel_errh);
}

/*
 * Count cycles for all of IPB code.
 * FromDevice and ToDevice call entering_ipb() and leaving_ipb().
 */
unsigned long long click_cycles = 0;
unsigned int click_calls = 0;
unsigned long long click_enter_time;
int click_entered;

static unsigned long cli_flags;

#if CLICK_STATS > 0
void
_entering_ipb()
{
#if 1
  save_flags(cli_flags);
  cli();
#endif

  assert(click_entered == 0);
  click_entered++;
  click_enter_time = click_get_cycles();
}

void
_leaving_ipb()
{
  unsigned long long t;
  
  t = click_get_cycles();
  click_cycles += t - click_enter_time;
  click_calls += 1;
  assert(click_entered == 1);
  click_entered -= 1;
  
#if 1
  restore_flags(cli_flags);
#endif
}
#endif /* CLICK_STATS */

extern "C" void
click_cycle_counter(int which, unsigned int *fnp, unsigned long long *valp);

static String
read_cycles(Element *, void *)
{
  StringAccum sa;
  sa << click_get_cycles() << " cycles\n";
  sa << click_calls << " ipb calls\n";
  sa << click_cycles << " ipb cycles\n";
  click_calls = 0;
  click_cycles = 0;
  return sa.take_string();
}

static String
read_version(Element *, void *)
{
  return String(VERSION) + "\n";
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
  int x = atomic_read(&num_click_threads);
  return String(x) + "\n";
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
  for (int i = 0; i < packages->size(); i++)
    sa << packages->at(i) << "\n";
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


extern "C" void
click_provide(const char *name)
{
  MOD_INC_USE_COUNT;
  packages->push_back(String(name));
}

extern "C" void
click_unprovide(const char *name)
{
  String n = name;
  for (int i = 0; i < packages->size(); i++)
    if (packages->at(i) == n) {
      MOD_DEC_USE_COUNT;
      packages->at(i) = packages->back();
      packages->pop_back();
      break;
    }
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
  if (nroot_handlers >= ROOT_HANDLERS_CAP)
    return;
  int i = nroot_handlers;
  nroot_handlers++;
  root_handlers[i].name = name;
  root_handlers[i].read = read;
  root_handlers[i].read_thunk = read_thunk;
  root_handlers[i].write = write;
  root_handlers[i].write_thunk = write_thunk;
  register_handler(proc_click_entry, -1, i);
}

extern "C" int
init_module()
{
  // first call C++ static initializers
  String::static_initialize();
  Element::static_initialize();
  ErrorHandler::static_initialize(new KernelErrorHandler);
  kernel_errh = ErrorHandler::default_handler();
  extern ErrorHandler *click_chatter_errh;
  click_chatter_errh = new SyslogErrorHandler;
  packages = new Vector<String>;
  lexer = new Lexer(kernel_errh);
  export_elements(lexer);
  
  current_router = 0;
  
  click_register_pde(&proc_root, proc_click_entry);
  init_click_proc();  
  init_proc_click_config();
  init_proc_click_elements();
  init_proc_click_errors();
  init_click_sched();

  // add handlers to the root directory. warning: this only works if there
  // is no current_router while the handlers are being added.
  root_handlers = new Router::Handler[ROOT_HANDLERS_CAP];
  next_root_handler("version", read_version, 0, 0, 0);
  next_root_handler("list", read_list, 0, 0, 0);
  next_root_handler("classes", read_classes, 0, 0, 0);
  next_root_handler("packages", read_packages, 0, 0, 0);
  next_root_handler("requirements", read_requirements, 0, 0, 0);
  next_root_handler("flatconfig", read_flatconfig, 0, 0, 0);
  next_root_handler("meminfo", read_meminfo, 0, 0, 0);
  next_root_handler("cycles", read_cycles, 0, 0, 0);
  next_root_handler("threads", read_threads, 0, 0, 0);

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
  click_unregister_pde(proc_click_entry);
  cleanup_click_proc();
  cleanup_click_sched();
  delete lexer;
  
  extern ErrorHandler *click_chatter_errh;
  delete click_chatter_errh;
  delete packages;
  click_chatter_errh = 0;
  
  delete[] root_handlers;
  ErrorHandler::static_cleanup();
  
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
