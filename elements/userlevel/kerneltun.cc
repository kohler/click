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
#include <click/args.hh>
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
# include <net/route.h>
#endif
#if defined(HAVE_NET_IF_TAP_H)
# define KERNELTAP_NET 1
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

#if defined(__NetBSD__)
# include <sys/param.h>
# include <sys/sysctl.h>
#endif

#if defined(__FreeBSD__)
# include <net/ethernet.h>
#endif

CLICK_DECLS

KernelTun::KernelTun()
    : _fd(-1), _tap(false), _task(this), _ignore_q_errs(false),
      _printed_write_err(false), _printed_read_err(false),
      _selected_calls(0), _packets(0)
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
    _headroom = Packet::default_headroom;
    _adjust_headroom = false;
    _headroom += (4 - _headroom % 4) % 4; // default 4/0 alignment
    _mtu_out = DEFAULT_MTU;
    _burst = 1;
    if (Args(conf, this, errh)
	.read_mp("ADDR", IPPrefixArg(), _near, _mask)
	.read_p("GATEWAY", _gw)
	.read("TAP", _tap)
	.read("HEADROOM", _headroom).read_status(_adjust_headroom)
	.read("BURST", _burst)
	.read("ETHER", _macaddr)
	.read("IGNORE_QUEUE_OVERFLOWS", _ignore_q_errs)
	.read("MTU", _mtu_out)
#if KERNELTUN_LINUX
	.read("DEV_NAME", Args::deprecated, _dev_name)
	.read("DEVNAME", _dev_name)
#endif
	.complete() < 0)
	return -1;

    if (_gw && !_gw.matches_prefix(_near, _mask))
	return errh->error("bad GATEWAY");
    if (_burst < 1)
	return errh->error("BURST must be >= 1");
    if (_mtu_out < (int) sizeof(click_ip))
	return errh->error("MTU must be greater than %d", sizeof(click_ip));
    if (_headroom > 8192)
	return errh->error("HEADROOM too big");
    _adjust_headroom = !_adjust_headroom;
    return 0;
}

#if KERNELTUN_LINUX
int
KernelTun::try_linux_universal()
{
    int fd = open("/dev/net/tun", O_RDWR | O_NONBLOCK);
    if (fd < 0)
	return -errno;

    struct ifreq ifr;
    memset(&ifr, 0, sizeof(ifr));
    ifr.ifr_flags = (_tap ? IFF_TAP : IFF_TUN) | IFF_NO_PI;
    if (_dev_name)
	// Setting ifr_name allows us to select an arbitrary interface name.
	strncpy(ifr.ifr_name, _dev_name.c_str(), sizeof(ifr.ifr_name));
    int err = ioctl(fd, TUNSETIFF, (void *)&ifr);
    if (err < 0) {
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
#if !KERNELTUN_LINUX && !KERNELTUN_NET && !KERNELTUN_OSX && !KERNELTAP_NET
    return errh->error("%s is not yet supported on this system.\n(Please report this message to click@librelist.com.)", class_name());
#endif

    int error, saved_error = 0;
    String saved_device, saved_message;
    StringAccum tried;

#if KERNELTUN_LINUX
    if ((error = try_linux_universal()) >= 0)
	return error;
    else if (!saved_error || error != -ENOENT) {
	saved_error = error, saved_device = "net/tun";
	if (error == -ENODEV)
	    saved_message = "\n(Perhaps you need to enable tun in your kernel or load the 'tun' module.)";
    }
    tried << "/dev/net/tun, ";
#else
    String dev_prefix;
# if defined(KERNELTUN_OSX)
    _type = OSX_TUN;
    dev_prefix = "tun";
# elif defined(__NetBSD__) && !defined(TUNSIFHEAD)
    _type = (_tap ? NETBSD_TAP : NETBSD_TUN);
    dev_prefix = (_tap ? "tap" : "tun");
# else
    _type = (_tap ? BSD_TAP : BSD_TUN);
    dev_prefix = (_tap ? "tap" : "tun");
# endif

# if defined(__NetBSD__) && !defined(TUNSIFHEAD)
    if (_type == NETBSD_TAP) {
	// In NetBSD, two ways to create a tap:
	// 1. open /dev/tap cloning interface.
	// 2. do ifconfig tapN create (SIOCIFCREATE), and then open(/dev/tapN).
	// We use the cloning interface.
	if ((error = try_tun(dev_prefix, errh)) >= 0) {
	    struct ifreq ifr;
	    memset(&ifr, 0, sizeof(ifr));
	    if (ioctl(_fd, TAPGIFNAME, &ifr) != 0)
		return errh->error("TAPGIFNAME failed: %s", strerror(errno));
	    _dev_name = ifr.ifr_name;
	    return error;
	} else if (!saved_error || error != -ENOENT)
	    saved_error = error, saved_device = dev_prefix, saved_message = String();
	tried << "/dev/" << dev_prefix;
	goto error_out;
    }
# endif

    for (int i = 0; i < 6; i++) {
	if ((error = try_tun(dev_prefix + String(i), errh)) >= 0)
	    return error;
	else if (!saved_error || error != -ENOENT)
	    saved_error = error, saved_device = dev_prefix + String(i), saved_message = String();
	tried << "/dev/" << dev_prefix << i << ", ";
    }

# if defined(__NetBSD__) && !defined(TUNSIFHEAD)
 error_out:
# endif

#endif
    if (saved_error == -ENOENT) {
	tried.pop_back(2);
	return errh->error("could not find a tap device\n(checked %s)\nYou may need to load a kernel module to support tap.", tried.c_str());
    } else
	return errh->error("/dev/%s: %s%s", saved_device.c_str(), strerror(-saved_error), saved_message.c_str());
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
    struct sockaddr_in ifr_sin;
    ifr_sin.sin_family = AF_INET;
#if HAVE_SOCKADDR_IN_SIN_LEN
    ifr_sin.sin_len = sizeof(struct sockaddr_in);
#endif
    ifr_sin.sin_port = 0;
#if defined(SIOCSIFADDR) && defined(SIOCSIFNETMASK)
    for (int trynum = 0; trynum < 2; trynum++) {
	// Try setting the netmask twice.  On FreeBSD, we need to set the mask
	// *before* we set the address, or there's nasty behavior where the
	// tunnel cannot be assigned a different address.  (Or something like
	// that, I forget now.)  But on Linux, you must set the mask *after*
	// the address.
	ifr_sin.sin_addr = mask;
	memcpy(&ifr.ifr_addr, &ifr_sin, sizeof(ifr_sin));
	if (ioctl(s, SIOCSIFNETMASK, &ifr) == 0)
	    trynum++;
	else if (trynum == 1) {
	    errh->error("SIOCSIFNETMASK failed: %s", strerror(errno));
	    goto out;
	}
	ifr_sin.sin_addr = addr;
	memcpy(&ifr.ifr_addr, &ifr_sin, sizeof(ifr_sin));
	if (trynum < 2 && ioctl(s, SIOCSIFADDR, &ifr) != 0) {
	    errh->error("SIOCSIFADDR failed: %s", strerror(errno));
	    goto out;
	}
    }
#else
# error "Lacking SIOCSIFADDR and/or SIOCSIFNETMASK"
#endif
#if defined(KERNELTUN_OSX)
    // On OSX, we have to explicitly add a route, too
    {
       static int seq = 0;
       struct {
           struct rt_msghdr msghdr;
           struct sockaddr_in sin[3]; // Destination, gateway, netmask
       } msg;

       memset(&msg, 0, sizeof(msg));
       msg.msghdr.rtm_msglen  = sizeof(msg);
       msg.msghdr.rtm_version = RTM_VERSION;
       msg.msghdr.rtm_type    = RTM_ADD;
       msg.msghdr.rtm_index   = 0;
       msg.msghdr.rtm_pid     = 0;
       msg.msghdr.rtm_addrs   = RTA_DST | RTA_GATEWAY | RTA_NETMASK;
       msg.msghdr.rtm_seq     = ++seq;
       msg.msghdr.rtm_errno   = 0;
       msg.msghdr.rtm_flags   = RTF_UP | RTF_GATEWAY;

       for (unsigned int i = 0; i < sizeof(msg.sin) / sizeof(msg.sin[0]); i++) {
           msg.sin[i].sin_len    = sizeof(msg.sin[0]);
           msg.sin[i].sin_family = AF_INET;
       }

       msg.sin[0].sin_addr = addr & mask; // Destination
       msg.sin[1].sin_addr = addr;        // Gateway
       msg.sin[2].sin_addr = mask;        // Netmask

       int s = socket(PF_ROUTE, SOCK_RAW, AF_INET);
       if (s < 0) {
           errh->warning("Opening a PF_ROUTE socket failed: %s", strerror(errno));
           goto out;
       }
       int r = write(s, (char *)&msg, sizeof(msg));
       if (r < 0) {
           errh->warning("Writing to the PF_ROUTE socket failed: %s", strerror(errno));
       }
       r = close(s);
       if (r < 0) {
           errh->warning("Closing the PF_ROUTE socket failed: %s", strerror(errno));
       }
    }
#endif
#if defined(SIOCSIFHWADDR)
    if (_macaddr) {
	ifr.ifr_hwaddr.sa_family = ARPHRD_ETHER;
	memcpy(ifr.ifr_hwaddr.sa_data, _macaddr.data(), sizeof(_macaddr));
	if (ioctl(s, SIOCSIFHWADDR, &ifr) != 0)
	    errh->warning("could not set interface Ethernet address: %s", strerror(errno));
    }
#elif defined(__NetBSD__)
    if (_macaddr && _tap) {
	String tap = "net.link.tap." + _dev_name, mac = _macaddr.unparse_colon();
	int r = sysctlbyname(tap.c_str(), (void *) 0, (size_t *) 0, (void *) mac.c_str(), mac.length());
	if (r < 0)
	    errh->warning("could not set interface Ethernet address: %s", strerror(errno));
    } else if (_macaddr)
	errh->warning("could not set interface Ethernet address: no support for /dev/tun");
#elif defined(__FreeBSD__)
    if (_macaddr && _tap) {
	ifr.ifr_addr.sa_len = ETHER_ADDR_LEN;
	ifr.ifr_addr.sa_family = AF_LINK;
	memcpy(ifr.ifr_addr.sa_data, _macaddr.data(), ETHER_ADDR_LEN);
	if (ioctl(s, SIOCSIFLLADDR, &ifr) != 0)
	    errh->warning("could not set interface Ethernet address: %s", strerror(errno));
    } else if (_macaddr)
	errh->warning("could not set interface Ethernet address: no support for /dev/tun");
#else
    if (_macaddr)
	errh->warning("could not set interface Ethernet address: no support");
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
    if (_tap) {
	_mtu_in = _mtu_out + 14;
    } else if (_type == BSD_TUN)
	_mtu_in = _mtu_out + 4;
    else
	_mtu_in = _mtu_out;

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
    if (_adjust_headroom) {
	_headroom += (4 - _headroom % 4) % 4; // default 4/0 alignment
    }
    add_select(_fd, SELECT_READ);
    return 0;
}

void
KernelTun::cleanup(CleanupStage)
{
    if (_fd >= 0) {
	if (_type != LINUX_UNIVERSAL && _type != NETBSD_TAP)
	    updown(0, ~0, ErrorHandler::default_handler());
	close(_fd);
	remove_select(_fd, SELECT_READ);
    }
}

void
KernelTun::selected(int fd, int)
{
    Timestamp now = Timestamp::now();
    if (fd != _fd)
	return;
    ++_selected_calls;
    unsigned n = _burst;
    while (n > 0 && one_selected(now))
	--n;
}

bool
KernelTun::one_selected(const Timestamp &now)
{
    WritablePacket *p = Packet::make(_headroom, 0, _mtu_in, 0);
    if (!p) {
	click_chatter("out of memory!");
	return false;
    }

    int cc = read(_fd, p->data(), _mtu_in);
    if (cc > 0) {
	++_packets;
	p->take(_mtu_in - cc);
	bool ok = false;

	if (_tap) {
	    ok = true;
	} else if (_type == BSD_TUN) {
	    // 4-byte address family followed by IP header
	    int af = ntohl(*(unsigned *)p->data());
	    p->pull(4);
	    if (af != AF_INET && af != AF_INET6) {
		click_chatter("KernelTun(%s): don't know AF %d", _dev_name.c_str(), af);
		checked_output_push(1, p->clone());
	    } else
		ok = fake_pcap_force_ip(p, FAKE_DLT_RAW);
	} else {
	    ok = fake_pcap_force_ip(p, FAKE_DLT_RAW);
	}

	if (ok) {
	    p->set_timestamp_anno(now);
	    output(0).push(p);
	} else
	    checked_output_push(1, p);
	return true;
    } else {
	p->kill();
	if (errno != EAGAIN && errno != EWOULDBLOCK
	    && (!_ignore_q_errs || !_printed_read_err || errno != ENOBUFS)) {
	    _printed_read_err = true;
	    perror("KernelTun read");
	}
	return false;
    }
}

bool
KernelTun::run_task(Task *)
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
    int check_length;

    // sanity checks
    if (_tap) {
	if (p->length() < sizeof(click_ether)) {
	    click_chatter("%s(%s): packet too small", class_name(), _dev_name.c_str());
	    goto kill;
	}
	// use network length for MTU
	check_length = p->length() - sizeof(click_ether);

    } else {
	iph = p->ip_header();
	// check IP header
	if (!p->has_network_header()
	    || p->network_length() < (int) sizeof(click_ip)) {
	    click_chatter("%s(%s): no network header", class_name(), _dev_name.c_str());
	kill:
	    p->kill();
	    return;
	} else if (iph->ip_v != 4 && iph->ip_v != 6) {
	    click_chatter("%s(%s): unknown IP version %d", class_name(), _dev_name.c_str(), iph->ip_v);
	    goto kill;
	}
	// strip link headers
	p->change_headroom_and_length(p->headroom() + p->network_header_offset(), p->network_length());
	check_length = p->length();
    }

    // check MTU
    if (check_length > _mtu_out) {
	click_chatter("%s(%s): packet larger than MTU (%d)", class_name(), _dev_name.c_str(), _mtu_out);
	goto kill;
    }

    WritablePacket *q;
    if (_tap) {
	/* existing packet is OK */;
    } else if (_type == BSD_TUN) {
	uint32_t af = (iph->ip_v == 4 ? htonl(AF_INET) : htonl(AF_INET6));
	if ((q = p->push(4)))
	    *(uint32_t *)(q->data()) = af;
	p = q;
    } else {
	/* existing packet is OK */;
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

void
KernelTun::add_handlers()
{
    if (input_is_pull(0))
	add_task_handlers(&_task);
    add_data_handlers("dev_name", Handler::OP_READ, &_dev_name);
    add_data_handlers("selected_calls", Handler::OP_READ, &_selected_calls);
    add_data_handlers("packets", Handler::OP_READ, &_packets);
}

CLICK_ENDDECLS
ELEMENT_REQUIRES(userlevel FakePcap)
EXPORT_ELEMENT(KernelTun)
