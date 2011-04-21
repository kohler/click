// -*- c-basic-offset: 4 -*-
/*
 * fromhost.{cc,hh} -- receives packets to Linux through the
 * TUN Universal TUN/TAP module
 * John Bicket
 *
 * Copyright (c) 2004 Massachusetts Institute of Technology
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
#include "fromhost.hh"
#include <click/error.hh>
#include <click/bitvector.hh>
#include <click/args.hh>
#include <click/straccum.hh>
#include <click/glue.hh>
#include <clicknet/ether.h>
#include <click/standard/scheduleinfo.hh>
#include <unistd.h>
#include <fcntl.h>
#include <sys/ioctl.h>
#include <arpa/inet.h>

#include <net/if.h>
#include <linux/if_tun.h>

#ifdef HAVE_PROPER
#include <proper/prop.h>
#endif
CLICK_DECLS

FromHost::FromHost()
    : _fd(-1), _task(this)
{
#if HAVE_IP6
    _prefix6 = 0;
#endif
}

FromHost::~FromHost()
{
}

int
FromHost::configure(Vector<String> &conf, ErrorHandler *errh)
{
    _headroom = Packet::default_headroom;
    _headroom += (4 - (_headroom + 2) % 4) % 4; // default 4/2 alignment
    _mtu_out = DEFAULT_MTU;

    if (Args(conf, this, errh)
	.read_mp("DEVNAME", _dev_name)
	.read_p("DST", IPPrefixArg(), _near, _mask)
	.read("GATEWAY", _gw)
#if HAVE_IP6 && 0
	// XXX
	"DST6", 0, cpIP6PrefixLen, &_near6, &_prefix6,
#endif
	.read("ETHER", _macaddr)
	.read("HEADROOM", _headroom)
	.read("MTU", _mtu_out)
	.complete() < 0)
	return -1;

    if (_near && _gw && !_gw.matches_prefix(_near, _mask))
	return errh->error("bad GATEWAY");
    if (!_dev_name)
	return errh->error("must specify device name");
    if (_headroom > 8192)
	return errh->error("HEADROOM too large");
    return 0;
}

int
FromHost::try_linux_universal(ErrorHandler *errh)
{
    int fd;
#ifdef HAVE_PROPER
    fd = prop_open("/dev/net/tun", O_RDWR);
    if (fd >= 0) {
	if (fcntl(fd, F_SETFL, O_NONBLOCK) < 0) {
	    int e = errno;
	    errh->error("fcntl /dev/net/tun: %s", strerror(e));
	    close(fd);
	    return -e;
	}
    } else
	errh->warning("prop_open /dev/net/tun: %s", strerror(errno));
    if (fd < 0)
#endif
    fd = open("/dev/net/tun", O_RDWR | O_NONBLOCK);
    if (fd < 0) {
	int e = errno;
	errh->error("open /dev/net/tun: %s", strerror(e));
	return -e;
    }

    struct ifreq ifr;
    memset(&ifr, 0, sizeof(ifr));
    /* we want an ethertap-like interface */
    ifr.ifr_flags = IFF_TAP;

    /*
     * setting ifr_name this allows us to select an aribitrary
     * interface name.
     */
    strcpy(ifr.ifr_name, _dev_name.c_str());

    int err = ioctl(fd, TUNSETIFF, (void *)&ifr);
    if (err < 0) {
	errh->warning("Linux universal tun failed for %s: %s",
		      _dev_name.c_str(),
		      strerror(errno));
	close(fd);
	return -errno;
    }

    _dev_name = ifr.ifr_name;
    _fd = fd;
    return 0;
}

int
FromHost::setup_tun(ErrorHandler *errh)
{
    StringAccum sa;
    const char *up = " up";

    if (_macaddr) {
	sa.clear();
	sa << "/sbin/ifconfig " << _dev_name << " hw ether " << _macaddr.unparse_colon();
	if (system(sa.c_str()) != 0)
	    errh->error("%s: %s", sa.c_str(), strerror(errno));

	sa.clear();
	sa << "/sbin/ifconfig " << _dev_name << " arp";
	if (system(sa.c_str()) != 0)
	    return errh->error("%s: couldn't set arp flags: %s", sa.c_str(), strerror(errno));
    }

    if (_near) {
	sa.clear();
	sa << "/sbin/ifconfig " << _dev_name << " " << _near << " netmask " << _mask << up << " 2>/dev/null";
	if (system(sa.c_str()) != 0)
	    return errh->error("%s: %<%s%> failed\n(Perhaps Ethertap is in a kernel module that you haven't loaded yet?)", _dev_name.c_str(), sa.c_str());
	up = "";
    }

    if (_gw) {
	sa.clear();
	sa << "/sbin/route -n add default ";
#if defined(__linux__)
	sa << "gw ";
#endif
	sa << _gw;
	if (system(sa.c_str()) != 0)
	    return errh->error("%s: %<%s%> failed", _dev_name.c_str(), sa.c_str());
    }

#if HAVE_IP6
    if (_near6) {
	sa.clear();
	sa << "/sbin/ifconfig " << _dev_name << " inet6 add " << _near6 << "/" << _prefix6 << up << " 2>/dev/null";
	if (system(sa.c_str()) != 0)
	    return errh->error("%s: %<%s%> failed", _dev_name.c_str(), sa.c_str());
	up = "";
    }
#endif

    // calculate maximum packet size needed to receive data from
    // tun/tap.
    _mtu_in = _mtu_out + 4;
    return 0;
}

void
FromHost::dealloc_tun()
{
  if (_near) {
      String cmd = "/sbin/ifconfig " + _dev_name + " down";
      if (system(cmd.c_str()) != 0)
	  click_chatter("%s: failed: %s", name().c_str(), cmd.c_str());
  }
}

int
FromHost::initialize(ErrorHandler *errh)
{
    if (try_linux_universal(errh) < 0)
	return -1;
    if (setup_tun(errh) < 0)
	return -1;

    ScheduleInfo::join_scheduler(this, &_task, errh);
    _nonfull_signal = Notifier::downstream_full_signal(this, 0, &_task);

    add_select(_fd, SELECT_READ);
    return 0;
}

void
FromHost::cleanup(CleanupStage)
{
    if (_fd >= 0) {
	close(_fd);
	remove_select(_fd, SELECT_READ);
    }
}

void
FromHost::selected(int fd, int)
{
    if (fd != _fd)
	return;

    WritablePacket *p = Packet::make(_headroom, 0, _mtu_in, 0);
    if (!p) {
	click_chatter("out of memory!");
	return;
    }

    int cc = read(_fd, p->data(), _mtu_in);
    if (cc > 0) {
	p->take(_mtu_in - cc);
	// 2-byte padding followed by an Ethernet type
	p->pull(4);
	p->set_mac_header(p->data());
	const click_ip *ip = reinterpret_cast<const click_ip *>(p->data() + sizeof(click_ether));
	p->set_dst_ip_anno(IPAddress(ip->ip_dst));
	p->set_ip_header(ip, ip->ip_hl << 2);
	p->timestamp_anno().assign_now();
	output(0).push(p);
    } else {
	p->kill();
	perror("FromHost read");
    }

    if (!_nonfull_signal) {
	remove_select(_fd, SELECT_READ);
	return;
    }


}

bool
FromHost::run_task(Task *)
{
    if (!_nonfull_signal)
	return false;

    add_select(_fd, SELECT_READ);
    return true;

}

String
FromHost::read_param(Element *e, void *)
{
    FromHost *fh = static_cast<FromHost *>(e);
    return String(fh->_nonfull_signal.active());
}

void
FromHost::add_handlers()
{
    add_data_handlers("dev_name", Handler::OP_READ, &_dev_name);
    add_read_handler("signal", read_param, 0);
}

CLICK_ENDDECLS
ELEMENT_REQUIRES(userlevel linux)
EXPORT_ELEMENT(FromHost)
