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

#include <click/lexer.hh>
#include <click/router.hh>
#include <click/straccum.hh>
#include <click/confparse.hh>
#include <click/error.hh>

extern "C" int click_cleanup_packages();

ErrorHandler *click_logged_errh = 0;
Router *click_router = 0;


/***************************** Global handlers *******************************/

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

static String
read_meminfo(Element *, void *)
{
  extern int click_new_count; /* glue.cc */
  extern int click_outstanding_news; /* glue.cc */
  StringAccum sa;
  sa << "outstanding news " << click_outstanding_news << "\n";
  sa << "news " << click_new_count << "\n";
  return sa.take_string();
}

static int
write_stop(const String &s, Element *, void *, ErrorHandler *errh)
{
  if (click_router) {
    int n = 1;
    (void) cp_integer(cp_uncomment(s), &n);
    click_router->adjust_driver_reservations(-n);
  } else
    errh->message("no router installed");
  return 0;
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
  if (click_router) {
    const Vector<String> &v = click_router->requirements();
    StringAccum sa;
    for (int i = 0; i < v.size(); i++)
      sa << v[i] << "\n";
    return sa.take_string();
  } else
    return "";
}


/****************************** Error handlers *******************************/

class KernelErrorHandler : public ErrorHandler { public:

  KernelErrorHandler(bool log)		: _log(log) { reset_counts(); }
  
  int nwarnings() const			{ return _nwarnings; }
  int nerrors() const			{ return _nerrors; }
  void reset_counts()			{ _nwarnings = _nerrors = 0; }
  
  void handle_text(Seriousness, const String &);

 private:

  bool _log;
  int _nwarnings;
  int _nerrors;
  
};

static StringAccum *error_log;

void
KernelErrorHandler::handle_text(Seriousness seriousness, const String &message)
{
  if (seriousness <= ERR_MESSAGE)
    /* do nothing */;
  else if (seriousness == ERR_WARNING)
    _nwarnings++;
  else
    _nerrors++;

  // print message to syslog
  int pos = 0, nl;
  while ((nl = message.find_left('\n', pos)) >= 0) {
    String x = message.substring(pos, nl - pos);
    printf("%s\n", x.cc());
    pos = nl + 1;
  }
  if (pos < message.length()) {
    String x = message.substring(pos);
    printf("%s\n", x.cc());
  }

  // log message if required
  if (_log && error_log)
    *error_log << message << "\n";
  
  if (seriousness == ERR_FATAL)
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

extern void export_elements(Lexer *);

static ErrorHandler *syslog_errh;

extern "C" void click_ether_input(struct ifnet *, struct mbuf **, struct ether_header *);
extern "C" void (*ng_ether_input_p)(struct ifnet *, struct mbuf **, struct ether_header *);

extern "C" void click_ether_output(struct ifnet *, struct mbuf **);
extern "C" void (*ng_ether_output_p)(struct ifnet *, struct mbuf **);

extern "C" int
init_module()
{
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

  // thread manager, sk_buff manager, config manager
  click_init_sched();
  click_init_config();

  // global handlers
  Router::add_global_read_handler("version", read_version, 0);
  Router::add_global_read_handler("packages", read_packages, 0);
  Router::add_global_read_handler("requirements", read_requirements, 0);
  Router::add_global_read_handler("meminfo", read_meminfo, 0);
  Router::add_global_read_handler("cycles", read_cycles, 0);
  Router::add_global_write_handler("stop", write_stop, 0);
  Router::add_global_read_handler("errors", read_errors, 0);
  Router::change_handler_flags(0, -1, "errors", 0, HANDLER_REREAD);

  // filesystem interface
#if 0
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
#endif

  init_router_element_procs();

  // netgraph hooks
  ng_ether_input_p = click_ether_input;
  ng_ether_output_p = click_ether_output;

  return 0;
}

void print_and_free_chunks();

extern "C" int
cleanup_module()
{
  extern int click_new_count; /* glue.cc */
  extern int click_outstanding_news; /* glue.cc */

  // netgraph hooks
  ng_ether_input_p = 0;
  ng_ether_output_p = 0;

  // filesystem interface
  clickfs_tree_cleanup();

  // extra packages, global handlers
  click_cleanup_packages();
  Router::cleanup_global_handlers();

  // config manager, thread manager, sk_buff manager
  click_cleanup_config();
  click_cleanup_sched();
  
  cp_va_static_cleanup();

  // clean up error handlers
  ErrorHandler::static_cleanup();
  delete click_logged_errh;
  delete syslog_errh;
  delete error_log;
  click_logged_errh = syslog_errh = 0;
  
  printf("click module exiting\n");
  
  // String (after any operations that might create Strings)
  String::static_cleanup();

  // report memory leaks
  if (Element::nelements_allocated)
    printf("click error: %d elements still allocated\n", Element::nelements_allocated);
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
