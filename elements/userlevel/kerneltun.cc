// -*- c-basic-offset: 4 -*-
/*
 * kerneltun.{cc,hh} -- element accesses network via /dev/tun device
 * Robert Morris, Douglas S. J. De Couto, Eddie Kohler
 *
 * Copyright (c) 1999-2000 Massachusetts Institute of Technology
 * Copyright (c) 2006 Regents of the University of California
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
#include "kerneltun.hh"
#include "fakepcap.hh"
#include <click/error.hh>
#include <click/bitvector.hh>
#include <click/confparse.hh>
#include <click/straccum.hh>
#include <click/glue.hh>
#include <clicknet/ether.h>
#include <click/standard/scheduleinfo.hh>
#include <unistd.h>
#include <fcntl.h>
#include <sys/ioctl.h>
#include <arpa/inet.h>

#if defined(__linux__) && defined(HAVE_LINUX_IF_TUN_H)
# define KERNELTUN_LINUX 1
#elif defined(HAVE_NET_IF_TUN_H)
# define KERNELTUN_NET 1
#elif defined(__APPLE__)
# define KERNELTUN_OSX 1 
// assume tun driver installed from http://chrisp.de/en/projects/tunnel.html
// this driver doesn't produce or expect packets with an address family prepended
#endif
#if defined(HAVE_NET_IF_TAP_H)
# define KERNELTAP_NET
#endif

#include <net/if.h>
#if HAVE_NET_IF_TUN_H
# include <net/if_tun.h>
#elif HAVE_LINUX_IF_TUN_H
# include <linux/if_tun.h>
#endif
#if HAVE_NET_IF_TAP_H
# include <net/if_tap.h>
#endif

CLICK_DECLS

KernelTun::KernelTun()
    : _fd(-1), _tap(false), _task(this), _ignore_q_errs(false),
      _printed_write_err(false), _printed_read_err(false)
{
}

KernelTun::~KernelTun()
{
}

void *
KernelTun::cast(const char *n)
{
    if (strcmp(n, "KernelTun") == 0)
	return this;
    else
	return Element::cast(n);
}

int
KernelTun::configure(Vector<String> &conf, ErrorHandler *errh)
{
    _gw = IPAddress();
    _headroom = Packet::DEFAULT_HEADROOM;
    _mtu_out = DEFAULT_MTU;
    if (cp_va_parse(conf, this, errh,
		    cpIPPrefix, "network address", &_near, &_mask,
		    cpOptional,
		    cpIPAddress, "default gateway", &_gw,
		    cpKeywords,
		    "TAP", cpBool, "supply Ethernet headers?", &_tap,
		    "HEADROOM", cpUnsigned, "default headroom for generated packets", &_headroom,
		    "ETHER", cpEthernetAddress, "fake device Ethernet address", &_macaddr,
		    "IGNORE_QUEUE_OVERFLOWS", cpBool, "ignore queue overflow errors?", &_ignore_q_errs,
		    "MTU", cpInteger, "MTU", &_mtu_out,
#if KERNELTUN_LINUX
		    "DEV_NAME", cpString, "device name", &_dev_name,
#endif
		    cpEnd) < 0)
	return -1;

    if (_gw) { // then it was set to non-zero by arg
	// check net part matches 
	unsigned int g = _gw.in_addr().s_addr;
	unsigned int m = _mask.in_addr().s_addr;
	unsigned int n = _near.in_addr().s_addr;
	if ((g & m) != (n & m)) {
	    _gw = 0;
	    errh->warning("not setting up default route\n(network address and gateway are on different networks)");
	}
    }

    if (_mtu_out < (int) sizeof(click_ip))
	return errh->error("MTU must be greater than %d", sizeof(click_ip));
    
    return 0;
}

#if KERNELTUN_LINUX
int
KernelTun::try_linux_universal(ErrorHandler *errh)
{
    int fd = open("/dev/net/tun", O_RDWR | O_NONBLOCK);
    if (fd < 0)
	return -errno;

    struct ifreq ifr;
    memset(&ifr, 0, sizeof(ifr));
    ifr.ifr_flags = (_tap ? IFF_TAP : IFF_TUN);
    if (_dev_name)
	// Setting ifr_name allows us to select an arbitrary interface name.
	strncpy(ifr.ifr_name, _dev_name.c_str(), sizeof(ifr.ifr_name));
    int err = ioctl(fd, TUNSETIFF, (void *)&ifr);
    if (err < 0) {
	errh->warning("Linux universal tun failed: %s", strerror(errno));
	close(fd);
	return -errno;
    }

    _dev_name = ifr.ifr_name;
    _fd = fd;
    _type = LINUX_UNIVERSAL;
    return 0;
}
#endif

int
KernelTun::try_tun(const String &dev_name, ErrorHandler *)
{
    String filename = "/dev/" + dev_name;
    int fd = open(filename.c_str(), O_RDWR | O_NONBLOCK);
    if (fd < 0)
	return -errno;

    _dev_name = dev_name;
    _fd = fd;
    return 0;
}

/*
 * Find an available kernel tap, or report error if none are available.
 * Does not set up the tap.
 * On success, _dev_name, _type, and _fd are valid.
 */
int
KernelTun::alloc_tun(ErrorHandler *errh)
{
#if !KERNELTUN_LINUX && !KERNELTUN_NET && !KERNELTUN_OSX
    return errh->error("%s is not yet supported on this system.\n(Please report this message to click@pdos.lcs.mit.edu.)", class_name());
#endif

    int error, saved_error = 0;
    String saved_device, saved_message;
    StringAccum tried;
    
#if KERNELTUN_LINUX
    if ((error = try_linux_universal(errh)) >= 0)
	return error;
    else if (!saved_error || error != -ENOENT) {
	saved_error = error, saved_device = "net/tun";
	if (error == -ENODEV)
	    saved_message = "\n(Perhaps you need to enable tun in your kernel or load the `tun' module.)";
    }
    tried << "/dev/net/tun, ";
#endif

    String dev_prefix;
#ifdef __linux__
    _type = LINUX_ETHERTAP;
    dev_prefix = "tap";
#elif defined(KERNELTUN_OSX)
    _type = OSX_TUN;
    dev_prefix = "tun";
#else
    _type = (_tap ? BSD_TAP : BSD_TUN);
    dev_prefix = (_tap ? "tap" : "tun");
#endif

    for (int i = 0; i < 6; i++) {
	if ((error = try_tun(dev_prefix + String(i), errh)) >= 0)
	    return error;
	else if (!saved_error || error != -ENOENT)
	    saved_error = error, saved_device = dev_prefix + String(i), saved_message = String();
	tried << "/dev/" << dev_prefix << i << ", ";
    }
    
    if (saved_error == -ENOENT) {
	tried.pop_back(2);
	return errh->error("could not find a tap device\n(checked %s)\nYou may need to load a kernel module to support tap.", tried.c_str());
    } else
	return errh->error("could not allocate device /dev/%s: %s%s", saved_device.c_str(), strerror(-saved_error), saved_message.c_str());
}

int
KernelTun::updown(IPAddress addr, IPAddress mask, ErrorHandler *errh)
{
    int before = errh->nerrors();
    int s = socket(PF_INET, SOCK_DGRAM, IPPROTO_IP);
    if (s < 0)
	return errh->error("socket() failed: %s", strerror(errno));
    struct ifreq ifr;
    memset(&ifr, 0, sizeof(ifr));
    strncpy(ifr.ifr_name, _dev_name.c_str(), sizeof(ifr.ifr_name));
#if defined(SIOCSIFADDR) && defined(SIOCSIFNETMASK) 
    {
	struct sockaddr_in *sin = (struct sockaddr_in *) &ifr.ifr_addr;
	sin->sin_family = AF_INET;
	sin->sin_len = sizeof(struct sockaddr_in);
	sin->sin_port = 0;
	sin->sin_addr = mask;
	if (ioctl(s, SIOCSIFNETMASK, &ifr) != 0) {
	    errh->error("SIOCSIFNETMASK failed: %s", strerror(errno));
	    goto out;
	}
	sin->sin_addr = addr;
	if (ioctl(s, SIOCSIFADDR, &ifr) != 0) {
	    errh->error("SIOCSIFADDR failed: %s", strerror(errno));
	    goto out;
	}
    }
#else
# error "Lacking SIOCSIFADDR and/or SIOCSIFNETMASK"
#endif
#if defined(SIOCSIFHWADDR)
    if (_macaddr) {
	ifr.ifr_hwaddr.sa_family = ARPHRD_ETHER;
	memcpy(ifr.ifr_hwaddr.sa_data, _macaddr.data(), sizeof(_macaddr));
	if (ioctl(s, SIOCSIFHWADDR, &ifr) != 0)
	    errh->warning("could not set interface Ethernet address: %s", strerror(errno));
    }
#elif !defined(__FreeBSD__)
    if (_macaddr)
	errh->warning("could not set interface Ethernet address: no support");
#else
    /* safe to ignore _macaddr on FreeBSD */
#endif
#if defined(SIOCSIFMTU)
    if (_mtu_out != DEFAULT_MTU) {
	ifr.ifr_mtu = _mtu_out;
	if (ioctl(s, SIOCSIFMTU, &ifr) != 0)
	    errh->warning("could not set interface MTU: %s", strerror(errno));
    }
#endif
#if defined(SIOCGIFFLAGS) && defined(SIOCSIFFLAGS)
    if (ioctl(s, SIOCGIFFLAGS, &ifr) != 0) {
	errh->error("SIOCGIFFLAGS failed: %s", strerror(errno));
	goto out;
    }
    if (_tap)
	ifr.ifr_flags = (addr ? ifr.ifr_flags & ~IFF_NOARP : ifr.ifr_flags | IFF_NOARP);
    ifr.ifr_flags = (addr ? ifr.ifr_flags | IFF_UP | IFF_PROMISC : ifr.ifr_flags & ~IFF_UP & ~IFF_PROMISC);
    if (ioctl(s, SIOCSIFFLAGS, &ifr) != 0) {
	errh->error("SIOCSIFFLAGS failed: %s", strerror(errno));
	goto out;
    }
#else
# error "Lacking SIOCGIFFLAGS and/or SIOCSIFFLAGS"
#endif
 out:
    close(s);
    return (errh->nerrors() == before ? 0 : -1);
}

int
KernelTun::setup_tun(ErrorHandler *errh)
{
// #if defined(__OpenBSD__)  && !defined(TUNSIFMODE)
//     /* see OpenBSD bug: http://cvs.openbsd.org/cgi-bin/wwwgnats.pl/full/782 */
// #define       TUNSIFMODE      _IOW('t', 88, int)
// #endif
#if defined(TUNSIFMODE) || defined(__FreeBSD__)
    if (!_tap) {
	int mode = IFF_BROADCAST;
	if (ioctl(_fd, TUNSIFMODE, &mode) != 0)
	    return errh->error("TUNSIFMODE failed: %s", strerror(errno));
    }
#endif
#if defined(__OpenBSD__)
    if (!_tap) {
	struct tuninfo ti;
	memset(&ti, 0, sizeof(struct tuninfo));
	if (ioctl(_fd, TUNGIFINFO, &ti) != 0)
	    return errh->error("TUNGIFINFO failed: %s", strerror(errno));
	ti.flags &= IFF_BROADCAST;
	if (ioctl(_fd, TUNSIFINFO, &ti) != 0)
	    return errh->error("TUNSIFINFO failed: %s", strerror(errno));
    }
#endif
    
#if defined(TUNSIFHEAD) || defined(__FreeBSD__)
    // Each read/write prefixed with a 32-bit address family,
    // just as in OpenBSD.
    if (!_tap && _type == BSD_TUN) {
	int yes = 1;
	if (ioctl(_fd, TUNSIFHEAD, &yes) != 0)
	    return errh->error("TUNSIFHEAD failed: %s", strerror(errno));
    }
#endif        

    // set addresses and MTU
    if (updown(_near, _mask, errh) < 0)
	return -1;

    if (_gw) {
	String cmd = "/sbin/route -n add default ";
#if defined(__linux__)
	cmd += "gw " + _gw.unparse();
#else
	cmd += _gw.unparse();
#endif
	if (system(cmd.c_str()) != 0)
	    return errh->error("%s: %s", cmd.c_str(), strerror(errno));
    }

    // calculate maximum packet size needed to receive data from
    // tun/tap.
    if (_type == LINUX_UNIVERSAL)
	_mtu_in = _mtu_out + 4;
    else if (_type == BSD_TUN)
	_mtu_in = _mtu_out + 4;
    else if (_type == BSD_TAP)
	_mtu_in = _mtu_out;
    else if (_type == OSX_TUN)
	_mtu_in = _mtu_out + 4; // + 0?
    else /* _type == LINUX_ETHERTAP */
	_mtu_in = _mtu_out + 16;
    
    return 0;
}

int
KernelTun::initialize(ErrorHandler *errh)
{
    if (alloc_tun(errh) < 0)
	return -1;
    if (setup_tun(errh) < 0)
	return -1;
    if (input_is_pull(0)) {
	ScheduleInfo::join_scheduler(this, &_task, errh);
	_signal = Notifier::upstream_empty_signal(this, 0, &_task);
    }
    add_select(_fd, SELECT_READ);
    return 0;
}

void
KernelTun::cleanup(CleanupStage)
{
    if (_fd >= 0) {
	if (_type != LINUX_UNIVERSAL)
	    updown(0, ~0, ErrorHandler::default_handler());
	close(_fd);
	remove_select(_fd, SELECT_READ);
    }
}

void
KernelTun::selected(int fd)
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
	bool ok = false;

	if (_tap) {
	    if (_type == LINUX_UNIVERSAL)
		// 2-byte padding, 2-byte Ethernet type, then Ethernet header
		p->pull(4);
	    else if (_type == LINUX_ETHERTAP)
		// 2-byte padding, then Ethernet header
		p->pull(2);
	    ok = true;
	} else if (_type == LINUX_UNIVERSAL) {
	    // 2-byte padding followed by an Ethernet type
	    uint16_t etype = *(uint16_t *)(p->data() + 2);
	    p->pull(4);
	    if (etype != htons(ETHERTYPE_IP) && etype != htons(ETHERTYPE_IP6))
		checked_output_push(1, p->clone());
	    else
		ok = fake_pcap_force_ip(p, FAKE_DLT_RAW);
	} else if (_type == BSD_TUN) {
	    // 4-byte address family followed by IP header
	    int af = ntohl(*(unsigned *)p->data());
	    p->pull(4);
	    if (af != AF_INET && af != AF_INET6) {
		click_chatter("KernelTun(%s): don't know AF %d", _dev_name.c_str(), af);
		checked_output_push(1, p->clone());
	    } else
		ok = fake_pcap_force_ip(p, FAKE_DLT_RAW);
	} else if (_type == OSX_TUN) {
	    ok = fake_pcap_force_ip(p, FAKE_DLT_RAW);
	} else { /* _type == LINUX_ETHERTAP */
	    // 2-byte padding followed by a mostly-useless Ethernet header
	    uint16_t etype = *(uint16_t *)(p->data() + 14);
	    p->pull(16);
	    if (etype != htons(ETHERTYPE_IP) && etype != htons(ETHERTYPE_IP6))
		checked_output_push(1, p->clone());
	    else
		ok = fake_pcap_force_ip(p, FAKE_DLT_RAW);
	}

	if (ok) {
	    p->timestamp_anno().set_now();
	    output(0).push(p);
	} else
	    checked_output_push(1, p);

    } else {
	if (!_ignore_q_errs || !_printed_read_err || (errno != ENOBUFS)) {
	    _printed_read_err = true;
	    perror("KernelTun read");
	}
    }
}

bool
KernelTun::run_task()
{
    Packet *p = input(0).pull();
    if (p)
	push(0, p);
    else if (!_signal)
	return false;
    _task.fast_reschedule();
    return p != 0;
}

void
KernelTun::push(int, Packet *p)
{
    const click_ip *iph = 0;
    
    // sanity checks
    if (!_tap) {
	iph = p->ip_header();
	if (!iph || p->network_length() < (int) sizeof(click_ip))
	    click_chatter("KernelTun(%s): no network header", _dev_name.c_str());
	else if (iph->ip_v != 4 && iph->ip_v != 6)
	    click_chatter("KernelTun(%s): unknown IP version %d", _dev_name.c_str(), iph->ip_v);
	else {
	    p->change_headroom_and_length(p->headroom() + p->network_header_offset(), p->network_length());
	    goto check_length;
	}
    kill:
	p->kill();
	return;
    } else if (p->length() < sizeof(click_ether)) {
	click_chatter("KernelTap(%s): packet too small", _dev_name.c_str());
	goto kill;
    }

    // check MTU
 check_length:
    if ((int) p->length() > _mtu_out) {
	click_chatter("%s(%s): packet larger than MTU (%d)", class_name(), _dev_name.c_str(), _mtu_out);
	goto kill;
    }

    WritablePacket *q;
    if (_tap) {
	if (_type == LINUX_UNIVERSAL) {
	    // 2-byte padding, 2-byte Ethernet type, then Ethernet header
	    uint16_t ethertype = ((const click_ether *) p->data())->ether_type;
	    if ((q = p->push(4)))
		((uint16_t *) q->data())[1] = ethertype;
	    p = q;
	} else if (_type == LINUX_ETHERTAP) {
	    // 2-byte padding, then Ethernet header
	    p = p->push(2);
	} else
	    /* existing packet is OK */;
    } else if (_type == LINUX_UNIVERSAL) {
	// 2-byte padding followed by an Ethernet type
	uint32_t ethertype = (iph->ip_v == 4 ? htonl(ETHERTYPE_IP) : htonl(ETHERTYPE_IP6));
	if ((q = p->push(4)))
	    *(uint32_t *)(q->data()) = ethertype;
    } else if (_type == BSD_TUN) { 
	uint32_t af = (iph->ip_v == 4 ? htonl(AF_INET) : htonl(AF_INET6));
	if ((q = p->push(4)))
	    *(uint32_t *)(q->data()) = af;
    } else if (_type == LINUX_ETHERTAP) {
	uint16_t ethertype = (iph->ip_v == 4 ? htons(ETHERTYPE_IP) : htons(ETHERTYPE_IP6));
	if ((q = p->push(16))) {
	    /* ethertap driver is very picky about what address we use
	     * here. e.g. if we have the wrong address, linux might ignore
	     * all the packets, or accept udp or icmp, but ignore tcp.
	     * aaarrrgh, well this works. -ddc */
	    memcpy(q->data(), "\x00\x00\xFE\xFD\x00\x00\x00\x00\xFE\xFD\x00\x00\x00\x00", 14);
	    *(uint16_t *)(q->data() + 14) = ethertype;
	}
    }

    if (p) {
	int w = write(_fd, p->data(), p->length());
	if (w != (int) p->length() && (errno != ENOBUFS || !_ignore_q_errs || !_printed_write_err)) {
	    _printed_write_err = true;
	    click_chatter("%s(%s): write failed: %s", class_name(), _dev_name.c_str(), strerror(errno));
	}
	p->kill();
    } else
	click_chatter("%s(%s): out of memory", class_name(), _dev_name.c_str());
}

String
KernelTun::print_dev_name(Element *e, void *) 
{
    KernelTun *kt = (KernelTun *) e;
    return kt->_dev_name;
}

void
KernelTun::add_handlers()
{
    if (input_is_pull(0))
	add_task_handlers(&_task);
    add_read_handler("dev_name", print_dev_name, 0);
}

CLICK_ENDDECLS
ELEMENT_REQUIRES(userlevel FakePcap)
EXPORT_ELEMENT(KernelTun)
