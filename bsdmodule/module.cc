/*
 * module.cc -- FreeBSD kernel module main program
 * Eddie Kohler, Robert Morris, Nickolai Zeldovich, Marko Zec
 *
 * Copyright (c) 1999-2001 Massachusetts Institute of Technology
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
#include "clickfs_tree.hh"

#include <click/lexer.hh>
#include <click/router.hh>
#include <click/straccum.hh>
#include <click/confparse.hh>
#include <click/bighashmap_arena.hh>
#include <click/notifier.hh>
#include <click/nameinfo.hh>
#include <sys/stat.h>


int click_mode_r, click_mode_w, click_mode_x, click_mode_dir;
static int click_accessible = 1;

extern "C" int click_cleanup_packages();

KernelErrorHandler *click_logged_errh = 0;
static KernelErrorHandler *syslog_errh = 0;
Router *click_router = 0;
Master *click_master = 0;


/***************************** Global handlers *******************************/

static String
read_cycles(Element *, void *)
{
    StringAccum sa;
    sa << click_get_cycles() << " cycles\n";
    return sa.take_string();
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
	    click_router->set_runcount(-10000);
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
    if (!cp_bool(cp_uncomment(s), &stop))
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
	printf("%.*s\n", newline - begin, begin);
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

extern "C" void click_ether_input(struct ifnet *, struct mbuf **, struct ether_header *);
extern "C" void (*ng_ether_input_p)(struct ifnet *, struct mbuf **, struct ether_header *);

extern "C" void click_ether_output(struct ifnet *, struct mbuf **);
extern "C" void (*ng_ether_output_p)(struct ifnet *, struct mbuf **);


extern "C" int
init_module()
{
    // C++ static initializers
    String::static_initialize();
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

    // thread manager, config manager
    click_init_sched(ErrorHandler::default_handler());
    click_init_config();

    // global handlers
    Router::add_read_handler(0, "packages", read_packages, 0);
    Router::add_read_handler(0, "meminfo", read_meminfo, 0);
    Router::add_read_handler(0, "cycles", read_cycles, 0);
    Router::add_read_handler(0, "errors", read_errors, 0);
    Router::change_handler_flags(0, "errors", 0, HANDLER_REREAD);
    Router::add_read_handler(0, "messages", read_errors, (void *)1);
    Router::change_handler_flags(0, "messages", 0, HANDLER_REREAD);
#if HAVE_KERNEL_ASSERT
    Router::add_read_handler(0, "assert_stop", read_assert_stop, 0);
    Router::add_write_handler(0, "assert_stop", write_assert_stop, 0);
#endif

    // filesystem interface
    clickfs_tree_init();
    // set modes based on 'accessible'
    if (click_accessible) {
	click_mode_r = S_IRUSR | S_IRGRP | S_IROTH;
	click_mode_x = S_IXUSR | S_IXGRP | S_IXOTH;
    } else {
	click_mode_r = S_IRUSR | S_IRGRP;
	click_mode_x = S_IXUSR | S_IXGRP;
    }
    click_mode_w = S_IWUSR | S_IWGRP;
    click_mode_dir = S_IFDIR | click_mode_r | click_mode_x;

    // netgraph hooks
    ng_ether_input_p = click_ether_input;
    ng_ether_output_p = click_ether_output;

    return 0;
}

void click_dmalloc_cleanup();

extern "C" void
cleanup_module()
{
    extern int click_new_count; /* glue.cc */
    extern int click_outstanding_news; /* glue.cc */

    // netgraph hooks
    ng_ether_input_p = 0;
    ng_ether_output_p = 0;

    // extra packages, global handlers
    click_cleanup_packages();
    Router::static_cleanup();

    // config manager, thread manager
    click_cleanup_config();
    click_cleanup_sched();
  
    // filesystem interface
    clickfs_tree_cleanup();

    cp_va_static_cleanup();

    // error handlers
    ErrorHandler::static_cleanup();
    delete click_logged_errh;
    delete syslog_errh;
    click_logged_errh = syslog_errh = 0;
  
    printf("Click module exiting\n");
  
    // HashMap
    HashMap_ArenaFactory::static_cleanup();
  
    // String (after any operations that might destroy Strings)
    NameInfo::static_cleanup();
    String::static_cleanup();

    // report memory leaks
    if (Element::nelements_allocated)
	printf("click error: %d elements still allocated\n", Element::nelements_allocated);
    if (click_outstanding_news) {
	printf("click error: %d outstanding news\n", click_outstanding_news);
	click_dmalloc_cleanup();
    }
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
