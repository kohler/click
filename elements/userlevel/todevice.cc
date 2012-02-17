/*
 * todevice.{cc,hh} -- element writes packets to network via pcap library
 * Douglas S. J. De Couto, Eddie Kohler, John Jannotti
 *
 * Copyright (c) 1999-2000 Massachusetts Institute of Technology
 * Copyright (c) 2005-2008 Regents of the University of California
 * Copyright (c) 2011 Meraki, Inc.
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
#if HAVE_NET_BPF_H
# include <sys/types.h>
# include <sys/time.h>
# include <net/bpf.h>
# define PCAP_DONT_INCLUDE_PCAP_BPF_H 1
#endif
#include "todevice.hh"
#include <click/error.hh>
#include <click/etheraddress.hh>
#include <click/args.hh>
#include <click/router.hh>
#include <click/standard/scheduleinfo.hh>
#include <click/packet_anno.hh>
#include <click/straccum.hh>
#include <stdio.h>
#include <unistd.h>

#if TODEVICE_ALLOW_DEVBPF
# include <fcntl.h>
# include <sys/types.h>
# include <sys/socket.h>
# include <sys/ioctl.h>
# include <net/if.h>
#endif
#if TODEVICE_ALLOW_LINUX
# include <sys/socket.h>
# include <sys/ioctl.h>
# include <net/if.h>
# include <net/if_packet.h>
# include <features.h>
# if __GLIBC__ >= 2 && __GLIBC_MINOR__ >= 1
#  include <netpacket/packet.h>
# else
#  include <linux/if_packet.h>
# endif
#endif

CLICK_DECLS

ToDevice::ToDevice()
    : _task(this), _timer(&_task), _q(0), _pulls(0)
{
#if TODEVICE_ALLOW_PCAP
    _pcap = 0;
    _my_pcap = false;
#endif
#if TODEVICE_ALLOW_LINUX || TODEVICE_ALLOW_DEVBPF || TODEVICE_ALLOW_PCAPFD
    _fd = -1;
    _my_fd = false;
#endif
}

ToDevice::~ToDevice()
{
}

int
ToDevice::configure(Vector<String> &conf, ErrorHandler *errh)
{
    String method;
    _burst = 1;
    if (Args(conf, this, errh)
	.read_mp("DEVNAME", _ifname)
	.read("DEBUG", _debug)
	.read("METHOD", WordArg(), method)
	.read("BURST", _burst)
	.complete() < 0)
	return -1;
    if (!_ifname)
	return errh->error("interface not set");
    if (_burst <= 0)
	return errh->error("bad BURST");

    if (method == "") {
#if TODEVICE_ALLOW_PCAP && TODEVICE_ALLOW_LINUX
	_method = method_pcap;
	if (FromDevice *fd = find_fromdevice())
	    if (fd->linux_fd())
		_method = method_linux;
#elif TODEVICE_ALLOW_PCAP
	_method = method_pcap;
#elif TODEVICE_ALLOW_LINUX
	_method = method_linux;
#elif TODEVICE_ALLOW_DEVBPF
	_method = method_devbpf;
#elif TODEVICE_ALLOW_PCAPFD
	_method = method_pcapfd;
#else
	return errh->error("cannot send packets on this platform");
#endif
    }
#if TODEVICE_ALLOW_PCAP
    else if (method == "PCAP")
	_method = method_pcap;
#endif
#if TODEVICE_ALLOW_LINUX
    else if (method == "LINUX")
	_method = method_linux;
#endif
#if TODEVICE_ALLOW_DEVBPF
    else if (method == "DEVBPF")
	_method = method_devbpf;
#endif
#if TODEVICE_ALLOW_PCAPFD
    else if (method == "PCAPFD")
	_method = method_pcapfd;
#endif
    else
	return errh->error("bad METHOD");

    return 0;
}

FromDevice *
ToDevice::find_fromdevice() const
{
    Router *r = router();
    for (int ei = 0; ei < r->nelements(); ++ei) {
	FromDevice *fd = (FromDevice *) r->element(ei)->cast("FromDevice");
	if (fd && fd->ifname() == _ifname && fd->fd() >= 0)
	    return fd;
    }
    return 0;
}

int
ToDevice::initialize(ErrorHandler *errh)
{
    _timer.initialize(this);

#if TODEVICE_ALLOW_PCAP
    if (_method == method_pcap) {
	FromDevice *fd = find_fromdevice();
	if (fd && fd->pcap())
	    _pcap = fd->pcap();
	else {
	    _pcap = FromDevice::open_pcap(_ifname, FromDevice::default_snaplen, false, errh);
	    if (!_pcap)
		return -1;
	    _my_pcap = true;
	}
	_fd = pcap_fileno(_pcap);
	/* _my_fd = false by default */
    }
#endif

#if TODEVICE_ALLOW_DEVBPF
    if (_method == method_devbpf) {
	/* pcap_open_live() doesn't open for writing. */
	for (int i = 0; i < 16 && _fd < 0; i++) {
	    char tmp[64];
	    sprintf(tmp, "/dev/bpf%d", i);
	    _fd = open(tmp, 1);
	}
	if (_fd < 0)
	    return(errh->error("open /dev/bpf* for write: %s", strerror(errno)));
	_my_fd = true;

	struct ifreq ifr;
	strncpy(ifr.ifr_name, _ifname.c_str(), sizeof(ifr.ifr_name));
	ifr.ifr_name[sizeof(ifr.ifr_name) - 1] = 0;
	if (ioctl(_fd, BIOCSETIF, (caddr_t)&ifr) < 0)
	    return errh->error("BIOCSETIF %s failed", ifr.ifr_name);
# ifdef BIOCSHDRCMPLT
	int yes = 1;
	if (ioctl(_fd, BIOCSHDRCMPLT, (caddr_t)&yes) < 0)
	    errh->warning("BIOCSHDRCMPLT %s failed", ifr.ifr_name);
# endif
    }
#endif

#if TODEVICE_ALLOW_LINUX
    if (_method == method_linux) {
	FromDevice *fd = find_fromdevice();
	if (fd && fd->linux_fd() >= 0)
	    _fd = fd->linux_fd();
	else {
	    _fd = FromDevice::open_packet_socket(_ifname, errh);
	    if (_fd < 0)
		return -1;
	    _my_fd = true;
	}
    }
#endif

#if TODEVICE_ALLOW_PCAPFD
    if (_method == method_pcapfd) {
	FromDevice *fd = find_fromdevice();
	if (fd && fd->pcap())
	    _fd = fd->fd();
	else
	    return errh->error("initialized FromDevice required on this platform");
    }
#endif

    // check for duplicate writers
    void *&used = router()->force_attachment("device_writer_" + _ifname);
    if (used)
	return errh->error("duplicate writer for device %<%s%>", _ifname.c_str());
    used = this;

    ScheduleInfo::join_scheduler(this, &_task, errh);
    _signal = Notifier::upstream_empty_signal(this, 0, &_task);
    return 0;
}

void
ToDevice::cleanup(CleanupStage)
{
#if TODEVICE_ALLOW_PCAP
    if (_pcap && _my_pcap)
	pcap_close(_pcap);
    _pcap = 0;
#endif
#if TODEVICE_ALLOW_LINUX || TODEVICE_ALLOW_DEVBPF || TODEVICE_ALLOW_PCAPFD
    if (_fd >= 0 && _my_fd)
	close(_fd);
    _fd = -1;
#endif
}


/*
 * Linux select marks datagram fd's as writeable when the socket
 * buffer has enough space to do a send (sock_writeable() in
 * sock.h). BSD select always marks datagram fd's as writeable
 * (bpf_poll() in sys/net/bpf.c) This function should behave
 * appropriately under both.  It makes use of select if it correctly
 * tells us when buffers are available, and it schedules a backoff
 * timer if buffers are not available.
 * --jbicket
 */
int
ToDevice::send_packet(Packet *p)
{
    int r = 0;
    errno = 0;

#if TODEVICE_ALLOW_PCAP
    if (_method == method_pcap) {
# if HAVE_PCAP_INJECT
	r = pcap_inject(_pcap, p->data(), p->length());
# else
	r = pcap_sendpacket(_pcap, p->data(), p->length());
# endif
    }
#endif

#if TODEVICE_ALLOW_LINUX
    if (_method == method_linux)
	r = send(_fd, p->data(), p->length(), 0);
#endif

#if TODEVICE_ALLOW_DEVBPF
    if (_method == method_devbpf)
	if (write(_fd, p->data(), p->length()) != (ssize_t) p->length())
	    r = -1;
#endif

#if TODEVICE_ALLOW_PCAPFD
    if (_method == method_pcapfd)
	if (write(_fd, p->data(), p->length()) != (ssize_t) p->length())
	    r = -1;
#endif

    if (r >= 0)
	return 0;
    else
	return errno ? -errno : -EINVAL;
}

bool
ToDevice::run_task(Task *)
{
    Packet *p = _q;
    _q = 0;
    int count = 0, r = 0;

    do {
	if (!p) {
	    ++_pulls;
	    if (!(p = input(0).pull()))
		break;
	}
	if ((r = send_packet(p)) >= 0) {
	    _backoff = 0;
	    checked_output_push(0, p);
	    ++count;
	} else
	    break;
    } while (count < _burst);

    if (r == -ENOBUFS || r == -EAGAIN) {
	assert(!_q);
	_q = p;

	if (!_backoff) {
	    _backoff = 1;
	    add_select(_fd, SELECT_WRITE);
	} else {
	    _timer.schedule_after(Timestamp::make_usec(_backoff));
	    if (_backoff < 256)
		_backoff *= 2;
	    if (_debug) {
		Timestamp now = Timestamp::now();
		click_chatter("%p{element} backing off for %d at %p{timestamp}\n", this, _backoff, &now);
	    }
	}
	return count > 0;
    } else if (r < 0) {
	click_chatter("ToDevice(%s): %s", _ifname.c_str(), strerror(-r));
	checked_output_push(1, p);
    }

    if (p || _signal)
	_task.fast_reschedule();
    return count > 0;
}

void
ToDevice::selected(int, int)
{
    _task.reschedule();
    remove_select(_fd, SELECT_WRITE);
}


String
ToDevice::read_param(Element *e, void *thunk)
{
    ToDevice *td = (ToDevice *)e;
    switch((uintptr_t) thunk) {
    case h_debug:
	return String(td->_debug);
    case h_signal:
	return String(td->_signal);
    case h_pulls:
	return String(td->_pulls);
    case h_q:
	return String((bool) td->_q);
    default:
	return String();
    }
}

int
ToDevice::write_param(const String &in_s, Element *e, void *vparam,
		     ErrorHandler *errh)
{
    ToDevice *td = (ToDevice *)e;
    String s = cp_uncomment(in_s);
    switch ((intptr_t)vparam) {
    case h_debug: {
	bool debug;
	if (!BoolArg().parse(s, debug))
	    return errh->error("type mismatch");
	td->_debug = debug;
	break;
    }
    }
    return 0;
}

void
ToDevice::add_handlers()
{
    add_task_handlers(&_task);
    add_read_handler("debug", read_param, h_debug, Handler::CHECKBOX);
    add_read_handler("pulls", read_param, h_pulls);
    add_read_handler("signal", read_param, h_signal);
    add_read_handler("q", read_param, h_q);
    add_write_handler("debug", write_param, h_debug);
}

CLICK_ENDDECLS
ELEMENT_REQUIRES(FromDevice userlevel)
EXPORT_ELEMENT(ToDevice)
