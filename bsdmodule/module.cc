/*
 * module.cc -- FreeBSD kernel module main program
 * Nickolai Zeldovich
 *
 * Copyright (c) 2001 Massachusetts Institute of Technology
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
#include "clickfs_tree.hh"

#include "kernelerror.hh"
#include <click/lexer.hh>
#include <click/router.hh>
#include <click/straccum.hh>
#include <click/confparse.hh>

ErrorHandler *kernel_errh = 0;
static Lexer *lexer = 0;
Router *current_router = 0;


class BsdModuleLexerExtra : public LexerExtra { public:
  BsdModuleLexerExtra() { }
  void require(String, ErrorHandler *);
};

void
BsdModuleLexerExtra::require(String r, ErrorHandler *errh)
{
  if (!click_has_provision(r))
    errh->error("unsatisfied requirement `%s'", r.cc());
}

Router *
parse_router(String s)
{
  BsdModuleLexerExtra lextra;

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
    // printf("  killed\n");
    cleanup_router_element_procs();
    // printf("  cleaned\n");
    current_router->unuse();
    // printf("  deleted\n");
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

#if 0		/* XXX   the BSD clickfs isn't this flexible yet.. */
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
  StringAccum sa;
  spin_lock(&click_thread_spinlock);
  if (click_thread_pids)
    for (int i = 0; i < click_thread_pids->size(); i++)
      sa << (*click_thread_pids)[i] << '\n';
  spin_unlock(&click_thread_spinlock);
  return sa.take_string();
  //int x = atomic_read(&num_click_threads);
  //return String(x) + "\n";
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

  spin_lock(&click_thread_spinlock);
  click_thread_priority = priority;
  for (int i = 0; i < click_thread_pids->size(); i++) {
    struct proc *p = find_task_by_pid((*click_thread_pids)[i]);
    if (p)
      TASK_PRIO(task) = priority;
  }
  spin_unlock(&click_thread_spinlock);
  
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
#endif

extern void export_elements(Lexer *);

static ErrorHandler *syslog_errh;

extern "C" int
init_module()
{
  // first call C++ static initializers
  String::static_initialize();
  cp_va_static_initialize();

  syslog_errh = new SyslogErrorHandler;
  kernel_errh = new KernelErrorHandler;
  ErrorHandler *default_errh = new LandmarkErrorHandler(syslog_errh, "chatter");
  ErrorHandler::static_initialize(default_errh);

  init_click_sched();
  clickfs_tree_init();
  
  CLICK_DEFAULT_PROVIDES;
  lexer = new Lexer(kernel_errh);
  export_elements(lexer);
  
  current_router = 0;

  return 0;
}

void print_and_free_chunks();

extern "C" int
cleanup_module()
{
  int ret;
  extern int click_new_count; /* glue.cc */
  extern int click_outstanding_news; /* glue.cc */

  kill_current_router();

  cleanup_click_sched();
  delete lexer;
  
  Router::cleanup_global_handlers();
  cp_va_static_cleanup();

  /* Clean up the click filesystem tree */
  clickfs_tree_cleanup();

#if 0 /* XXX ?? */ 
  click_cleanup_packages();
#endif
  
  delete kernel_errh;
  delete syslog_errh;
  ErrorHandler::static_cleanup();
  kernel_errh = syslog_errh = 0;
 
  printf("click module exiting\n");
    
  // must call after all operations that might create strings are done
  String::static_cleanup();
  if (Element::nelements_allocated)
    printf("click error: %d elements still allocated\n",
	   Element::nelements_allocated);
  if (click_outstanding_news) {
    printf("click error: %d outstanding news\n", click_outstanding_news);
    print_and_free_chunks();
  }

  return 0;
}

static int
click_load(struct module *mod, int cmd, void *arg)
{
    int ret = ENOTSUP;

    /* Load and unload the VFS part first */
    ret = vfs_modevent(mod, cmd, arg);
    if (ret)
	return ret;
 
    switch (cmd) {
	case MOD_LOAD:
	    printf("Click module loading\n");
	    if (init_module()) {
		ret = EINVAL;
		break;
	    }

	    ret = 0;
	    break;

	case MOD_UNLOAD:
	    printf("Click module unloading\n");
	    cleanup_module();
	    ret = 0;
	    break;

	case MOD_SHUTDOWN:
	    /*
	     * MOD_SHUTDOWN is usually called when the machine is
	     * about to shut down and the module is loaded at the
	     * moment. Perhaps we should call cleanup_module() at
	     * this point, but since we're shutting down anyway,
	     * it doesn't really matter..
	     */
	    printf("Click module shutdown\n");
	    ret = 0;
	    break;

	default:
	    printf("Click: unknown module command %d\n", cmd);
	    ret = EINVAL;
	    break;
    }

    return ret;
}

static struct vfsconf click_vfsconf = {
	&clickfs_vfsops,
	"click",
	-1,
	0,
	VFCF_SYNTHETIC
};

static moduledata_t mod_data = {
	"click",
	click_load,
	&click_vfsconf
};

DECLARE_MODULE(click, mod_data, SI_SUB_VFS, SI_ORDER_MIDDLE);
VNODEOP_SET(clickfs_vnodeop_opv_desc);
