/*
 * module.cc -- Linux kernel module main program
 * Eddie Kohler, Robert Morris
 *
 * Copyright (c) 1999-2000 Massachusetts Institute of Technology
 * Copyright (c) 2000 Mazu Networks, Inc.
 * Copyright (c) 2008 Meraki, Inc.
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

#if HAVE_KERNEL_ASSERT
static bool assert_stops_router = false;
#endif
static int chatterlevel_ceiling = ErrorHandler::el_error;


/***************************** Global handlers *******************************/

enum {
    h_cycles, h_meminfo, h_packages, h_assert_stop
};

#ifdef HAVE_LINUX_READ_NET_SKBCOUNT
extern "C" int read_net_skbcount(void);
#endif

static String
read_global(Element *, void *user_data)
{
    StringAccum sa;
    switch ((intptr_t) user_data) {
#if HAVE_INT64_TYPES
    case h_cycles:
	sa << click_get_cycles();
	break;
#endif
    case h_meminfo: {
	extern size_t click_dmalloc_curnew, click_dmalloc_totalnew;
	sa << "outstanding news " << click_dmalloc_curnew << "\n"
	   << "news " << click_dmalloc_totalnew << "\n";
#if CLICK_DMALLOC
	extern size_t click_dmalloc_curmem, click_dmalloc_maxmem;
	sa << "current allocated mem " << click_dmalloc_curmem << '\n'
	   << "max allocated mem " << click_dmalloc_maxmem << '\n';
#endif
#ifdef HAVE_LINUX_READ_NET_SKBCOUNT
	sa << "net_skbcount " << read_net_skbcount() << "\n";
#endif
	break;
    }
    case h_packages: {
	Vector<String> v;
	click_public_packages(v);
	for (int i = 0; i < v.size(); i++)
	    sa << v[i] << "\n";
	break;
    }
#if HAVE_KERNEL_ASSERT
    case h_assert_stop:
	sa << assert_stops_router;
	break;
#endif
    }
    return sa.take_string();
}


/******************************* Assertions **********************************/

extern "C" void
click_assert_failed(const char *file, int line, const char *problem_text)
{
    click_chatter(KERN_ALERT "%s:%d: assertion failed: %s", file, line, problem_text);
#if HAVE_KERNEL_ASSERT
    if (assert_stops_router) {
	if (click_router) {
	    click_chatter(KERN_ALERT "%s:%d: assertion failed: Asking router to stop", file, line);
	    click_router->set_runcount(Router::STOP_RUNCOUNT);
	} else
	    click_chatter(KERN_ALERT "%s:%d: assertion failed: No router to stop", file, line);
    }
#endif
}

#if HAVE_KERNEL_ASSERT
static int
write_assert_stop(const String &s, Element *, void *, ErrorHandler *errh)
{
    if (!BoolArg().parse(s, assert_stops_router))
	return errh->error("assert_stop must be a boolean");
    else
	return 0;
}
#endif


/****************************** Error handlers *******************************/

void
KernelErrorHandler::buffer_store(uint32_t head, const char *begin, const char *end)
{
    if (begin != end) {
	uint32_t head_pos = head & (logbuf_siz - 1);
	uint32_t tail_pos = ((head + end - begin - 1) & (logbuf_siz - 1)) + 1;
	if (head_pos < tail_pos)
	    memcpy(_logbuf + head_pos, begin, end - begin);
	else {
	    uint32_t first = logbuf_siz - head_pos;
	    memcpy(_logbuf + head_pos, begin, first);
	    memcpy(_logbuf, begin + first, (end - begin) - first);
	}
    }
}

void
KernelErrorHandler::log_line(String landmark, const char *begin, const char *end)
{
    static_assert((logbuf_siz & (logbuf_siz - 1)) == 0, "logbuf_siz must be a power of two.");

    // ensure begin <= end
    if (begin > end)
	begin = end;

    // skip "chatter: " for message log
    if (begin + 9 <= end && memcmp(begin, "chatter: ", 9) == 0)
	begin += 9;

    // truncate a long line
    if (end - begin + landmark.length() > logbuf_siz - 1) {
	if (landmark.length() > logbuf_siz / 2)
	    landmark = landmark.substring(0, logbuf_siz / 2);
	end = begin + logbuf_siz - 1 - landmark.length();
    }

    // allocate space in the buffer
    uint32_t line_head, line_tail;
    do {
	line_head = _tail;
	line_tail = line_head + (end - begin) + 1 + landmark.length();
    } while (atomic_uint32_t::compare_swap(_tail, line_head, line_tail) != line_head);
    while (line_tail - _head > logbuf_siz)
	/* spin */;

    // copy the line into the buffer
    if (landmark)
	buffer_store(line_head, landmark.begin(), landmark.end());
    buffer_store(line_head + landmark.length(), begin, end);
    _logbuf[(line_tail - 1) & (logbuf_siz - 1)] = '\n';

    // mark the line as stored
    while (atomic_uint32_t::compare_swap(_head, line_head, line_tail) != line_head)
	/* spin */;
    if (line_tail > logbuf_siz)
	_wrapped = true;
}

void *
KernelErrorHandler::emit(const String &str, void *, bool)
{
    String landmark;
    int level = 3;
    const char *s = parse_anno(str, str.begin(), str.end(), "l", &landmark,
			       "#<>", &level, (const char *) 0);
    // normalize level
    level = (level < 0 ? 0 : (level > 7 ? 7 : level));
    if (level > chatterlevel_ceiling)
	level = chatterlevel_ceiling;

    landmark = clean_landmark(landmark, true);
    printk("<%d>%.*s%.*s\n", level, landmark.length(), landmark.begin(),
	   (int) (str.end() - s), s);
    log_line(landmark, s, str.end());
    return 0;
}

void
KernelErrorHandler::account(int level)
{
    ErrorHandler::account(level);
    if (level <= el_fatal)
	panic("kclick");
}

String
KernelErrorHandler::read(click_handler_direct_info *hdi) const
{
    uint32_t initial = 0;
    if (!*hdi->string) {
	initial = (_wrapped ? _tail - logbuf_siz : 0);
	*hdi->string = String(initial);
    } else
	IntArg().parse(*hdi->string, initial);

    uint32_t tail = _tail;
    uint32_t len = tail - initial;
    loff_t f_pos = *hdi->store_f_pos;
    if (f_pos > len)
	f_pos = len;
    if (f_pos + hdi->count > len)
	hdi->count = len - f_pos;

    loff_t last_f_pos = f_pos + hdi->count;
    while (f_pos < last_f_pos) {
	size_t pos = (initial + f_pos) & (logbuf_siz - 1);
	size_t amount = logbuf_siz - pos;
	if (amount > last_f_pos - f_pos)
	    amount = last_f_pos - f_pos;
	if (copy_to_user(hdi->buffer, _logbuf + pos, amount) > 0) {
	    hdi->retval = -EFAULT;
	    return String();
	}
	hdi->buffer += amount;
	f_pos += amount;
    }
    hdi->count = f_pos - *hdi->store_f_pos;
    return String();
}

static String
read_errors(Element *, void *user_data)
{
    if (click_logged_errh)
	return click_logged_errh->read((click_handler_direct_info *) user_data);
    else
	return String::make_out_of_memory();
}

static String
read_messages(Element *, void *user_data)
{
    if (syslog_errh)
	return syslog_errh->read((click_handler_direct_info *) user_data);
    else
	return String::make_out_of_memory();
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
    Router::add_read_handler(0, "packages", read_global, (void *) (intptr_t) h_packages);
    Router::add_read_handler(0, "meminfo", read_global, (void *) (intptr_t) h_meminfo);
#if HAVE_INT64_TYPES
    Router::add_read_handler(0, "cycles", read_global, (void *) (intptr_t) h_cycles);
#endif
    Router::add_read_handler(0, "errors", read_errors, 0, HANDLER_DIRECT);
    Router::add_read_handler(0, "messages", read_messages, 0, HANDLER_DIRECT);
#if HAVE_KERNEL_ASSERT
    Router::add_read_handler(0, "assert_stop", read_global, (void *) (intptr_t) h_assert_stop);
    Router::add_write_handler(0, "assert_stop", write_assert_stop, 0, Handler::NONEXCLUSIVE);
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

    // extra packages, global handlers, packets
    click_cleanup_packages();
    Router::static_cleanup();
    Packet::static_cleanup();

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
	printk("<1>click error: %d outstanding news\n", (int) click_dmalloc_curnew);
	click_dmalloc_cleanup();
    }
#ifdef HAVE_LINUX_READ_NET_SKBCOUNT
    printk("<1>net_skbcount: %d\n", read_net_skbcount());
#endif
}

#ifdef MODULE_LICENSE
MODULE_LICENSE("Dual BSD/GPL");
#endif
