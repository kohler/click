// -*- c-basic-offset: 4 -*-
/*
 * kerneltun.{cc,hh} -- element accesses network via /dev/tun device
 * Robert Morris, Douglas S. J. De Couto, Eddie Kohler
 *
 * Copyright (c) 1999-2000 Massachusetts Institute of Technology
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
#endif

#if HAVE_NET_IF_TUN_H
# include <net/if.h>
# include <net/if_tun.h>
#elif HAVE_LINUX_IF_TUN_H
# include <linux/if.h>
# include <linux/if_tun.h>
#endif

CLICK_DECLS

KernelTun::KernelTun()
    : Element(1, 1), _fd(-1), _task(this),
      _ignore_q_errs(false), _printed_write_err(false), _printed_read_err(false)
{
    MOD_INC_USE_COUNT;
}

KernelTun::~KernelTun()
{
    MOD_DEC_USE_COUNT;
}

KernelTun *
KernelTun::clone() const
{
    return new KernelTun();
}

void
KernelTun::notify_ninputs(int n)
{
    set_ninputs(n < 1 ? 0 : 1);
}

void
KernelTun::notify_noutputs(int n)
{
    set_noutputs(n < 2 ? 1 : 2);
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
		    "HEADROOM", cpUnsigned, "default headroom for generated packets", &_headroom,
		    "IGNORE_QUEUE_OVERFLOWS", cpBool, "ignore queue overflow errors?", &_ignore_q_errs,
		    "MTU", cpInteger, "MTU", &_mtu_out,
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
    ifr.ifr_flags = IFF_TUN;
    int err = ioctl(fd, TUNSETIFF, (void *)&ifr);
    if (err < 0) {
	errh->warning("Linux universal tun failed: %s", strerror(errno));
	close(fd);
	return -errno;
    }

    _dev_name = ifr.ifr_name;
    _fd = fd;
    return 0;
}
#endif

int
KernelTun::try_tun(const String &dev_name, ErrorHandler *)
{
    String filename = "/dev/" + dev_name;
    int fd = open(filename.cc(), O_RDWR | O_NONBLOCK);
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
#if !KERNELTUN_LINUX && !KERNELTUN_NET
    return errh->error("KernelTun is not yet supported on this system.\n(Please report this message to click@pdos.lcs.mit.edu.)");
#endif

    int error, saved_error = 0;
    String saved_device, saved_message;
    StringAccum tried;
    
#if KERNELTUN_LINUX
    _type = LINUX_UNIVERSAL;
    if ((error = try_linux_universal(errh)) >= 0)
	return error;
    else if (!saved_error || error != -ENOENT) {
	saved_error = error, saved_device = "net/tun";
	if (error == -ENODEV)
	    saved_message = "\n(Perhaps you need to enable tun in your kernel or load the `tun' module.)";
    }
    tried << "/dev/net/tun, ";
#endif

#ifdef __linux__
    _type = LINUX_ETHERTAP;
    String dev_prefix = "tap";
#else
    _type = BSD_TUN;
    String dev_prefix = "tun";
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
	return errh->error("could not find a tap device\n(checked %s)\nYou may need to enable tap support in your kernel.", tried.cc());
    } else
	return errh->error("could not allocate device /dev/%s: %s%s", saved_device.cc(), strerror(-saved_error), saved_message.cc());
}

int
KernelTun::setup_tun(struct in_addr near, struct in_addr mask, ErrorHandler *errh)
{
    char tmp[512], tmp0[64], tmp1[64];

// #if defined(__OpenBSD__)  && !defined(TUNSIFMODE)
//     /* see OpenBSD bug: http://cvs.openbsd.org/cgi-bin/wwwgnats.pl/full/782 */
// #define       TUNSIFMODE      _IOW('t', 88, int)
// #endif
#if defined(TUNSIFMODE) || defined(__FreeBSD__)
    {
	int mode = IFF_BROADCAST;
	if (ioctl(_fd, TUNSIFMODE, &mode) != 0)
	    return errh->error("TUNSIFMODE failed: %s", strerror(errno));
    }
#endif
#if defined(__OpenBSD__)
    {
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
    int yes = 1;
    if (ioctl(_fd, TUNSIFHEAD, &yes) != 0)
	return errh->error("TUNSIFHEAD failed: %s", strerror(errno));
#endif        

    strcpy(tmp0, inet_ntoa(near));
    strcpy(tmp1, inet_ntoa(mask));
    sprintf(tmp, "/sbin/ifconfig %s %s netmask %s up 2>/dev/null", _dev_name.cc(), tmp0, tmp1);
    if (system(tmp) != 0) {
# if defined(__linux__)
	// Is Ethertap available? If it is moduleified, then it might not be.
	// beside the ethertap module, you may also need the netlink_dev
	// module to be loaded.
	return errh->error("%s: `%s' failed\n(Perhaps Ethertap is in a kernel module that you haven't loaded yet?)", _dev_name.cc(), tmp);
# else
	return errh->error("%s: `%s' failed", _dev_name.cc(), tmp);
# endif
    }
    
    if (_gw) {
#if defined(__linux__)
	sprintf(tmp, "/sbin/route -n add default gw %s", _gw.s().cc());
#elif defined(__FreeBSD__) || defined(__OpenBSD__)
	sprintf(tmp, "/sbin/route -n add default %s", _gw.s().cc());
#endif
	if (system(tmp) != 0)
	    return errh->error("%s: %s", tmp, strerror(errno));
    }

    // XXXXXXX should set MTU
    if (_type == LINUX_UNIVERSAL)
	_mtu_in = _mtu_out + 4;
    else if (_type == BSD_TUN)
	_mtu_in = _mtu_out + 4;
    else /* _type == LINUX_ETHERTAP */
	_mtu_in = _mtu_out + 16;
    
    return 0;
}

void
KernelTun::dealloc_tun()
{
    String cmd = "/sbin/ifconfig " + _dev_name + " down";
    if (system(cmd.cc()) != 0) 
	click_chatter("%s: failed: %s", id().cc(), cmd.cc());
}

int
KernelTun::initialize(ErrorHandler *errh)
{
    if (alloc_tun(errh) < 0)
	return -1;
    if (setup_tun(_near, _mask, errh) < 0)
	return -1;
  if (input_is_pull(0))
    ScheduleInfo::join_scheduler(this, &_task, errh);
  add_select(_fd, SELECT_READ);
  return 0;
}

void
KernelTun::cleanup(CleanupStage)
{
    if (_fd >= 0) {
	close(_fd);
	remove_select(_fd, SELECT_READ);
	if (_type != LINUX_UNIVERSAL)
	    dealloc_tun();
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
	
	if (_type == LINUX_UNIVERSAL) {
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
		click_chatter("KernelTun(%s): don't know AF %d", _dev_name.cc(), af);
		checked_output_push(1, p->clone());
	    } else
		ok = fake_pcap_force_ip(p, FAKE_DLT_RAW);
	} else { /* _type == LINUX_ETHERTAP */
	    // 2-byte padding followed by a mostly-useless Ethernet header
	    p->pull(2);
	    ok = fake_pcap_force_ip(p, FAKE_DLT_EN10MB);
	}

	if (ok) {
	    (void) click_gettimeofday(&p->timestamp_anno());
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
    _task.fast_reschedule();
    return p != 0;
}

void
KernelTun::push(int, Packet *p)
{
    // Every packet has a 14-byte Ethernet header.
    // Extract the packet type, then ignore the Ether header.
    const click_ip *iph = p->ip_header();
    if (!iph) {
	click_chatter("KernelTun(%s): no network header", _dev_name.cc());
	p->kill();
    } else if (p->network_length() > _mtu_out) {
	click_chatter("KernelTun(%s): packet larger than MTU (%d)", _dev_name.cc(), _mtu_out);
	p->kill();
    } else if (iph->ip_v != 4 && iph->ip_v != 6) {
	click_chatter("KernelTun(%s): unknown IP version %d", _dev_name.cc(), iph->ip_v);
	p->kill();
    } else {
	p->change_headroom_and_length(p->headroom() + p->network_header_offset(), p->network_length());

	WritablePacket *q;
	if (_type == LINUX_UNIVERSAL) {
	    // 2-byte padding followed by an Ethernet type
	    uint32_t ethertype = (iph->ip_v == 4 ? htonl(ETHERTYPE_IP) : htonl(ETHERTYPE_IP6));
	    if ((q = p->push(4)))
		*(uint32_t *)(q->data()) = ethertype;
	} else if (_type == BSD_TUN) {
	    uint32_t af = (iph->ip_v == 4 ? htonl(AF_INET) : htonl(AF_INET6));
	    if ((q = p->push(4)))
		*(uint32_t *)(q->data()) = af;
	} else { /* _type == LINUX_ETHERTAP */
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

	if (q) {
	    int w = write(_fd, q->data(), q->length());
	    if (w != (int) q->length() && (errno != ENOBUFS || !_ignore_q_errs || !_printed_write_err)) {
		_printed_write_err = true;
		click_chatter("KernelTun(%s): write failed: %s", _dev_name.cc(), strerror(errno));
	    }
	    q->kill();
	} else
	    click_chatter("KernelTun(%s): out of memory", _dev_name.cc());
    }
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
