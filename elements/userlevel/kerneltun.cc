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
#if defined(__FreeBSD__) || defined(__OpenBSD__) || defined(__APPLE__)
#include <net/if.h>
#include <net/if_tun.h>
#endif
#if defined(__linux__) && defined(HAVE_LINUX_IF_TUN_H)
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

int
KernelTun::configure(Vector<String> &conf, ErrorHandler *errh)
{
  _gw = IPAddress();
  _headroom = 0;
  if (cp_va_parse(conf, this, errh,
		  cpIPPrefix, "network address", &_near, &_mask,
		  cpOptional,
		  cpIPAddress, "default gateway", &_gw,
		  cpUnsigned, "packet data headroom", &_headroom,
		  cpKeywords,
		  "IGNORE_QUEUE_OVERFLOWS", cpBool, "ignore queue overflow errors?", &_ignore_q_errs,
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
  return 0;
}

#if defined(__linux__) && defined(HAVE_LINUX_IF_TUN_H)
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
    _type = LINUX_UNIVERSAL;
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
    _type = LINUXBSD_TUN;
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
#if !(defined(__linux__) || defined(__FreeBSD__) || defined(__OpenBSD__))
    return errh->error("KernelTun is not yet supported on this system.\n(Please report this message to click@pdos.lcs.mit.edu.)");
#endif

    int error, saved_error = 0;
    String saved_device, saved_message;
    StringAccum tried;
    
#if defined(__linux__) && defined(HAVE_LINUX_IF_TUN_H)
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
    String dev_prefix = "tap";
#else
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
	if (ioctl(fd, TUNSIFMODE, &mode) != 0)
	    return errh->error("TUNSIFMODE failed: %s", strerror(errno));
    }
#endif
#if defined(__OpenBSD__)
    {
	struct tuninfo ti;
	memset(&ti, 0, sizeof(struct tuninfo));
	if (ioctl(fd, TUNGIFINFO, &ti) != 0)
	    return errh->error("TUNGIFINFO failed: %s", strerror(errno));
	ti.flags &= IFF_BROADCAST;
	if (ioctl(fd, TUNSIFINFO, &ti) != 0)
	    return errh->error("TUNSIFINFO failed: %s", strerror(errno));
    }
#endif
    
#if defined(TUNSIFHEAD) || defined(__FreeBSD__)
    // Each read/write prefixed with a 32-bit address family,
    // just as in OpenBSD.
    int yes = 1;
    if (ioctl(fd, TUNSIFHEAD, &yes) != 0)
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
#if defined(__OpenBSD__) || defined(__FreeBSD__) || defined(__linux__) || defined(__APPLE__)
    int cc;
    unsigned char b[2048];

    if (fd != _fd)
	return;
  
    cc = read(_fd, b, sizeof(b));
    if (cc > 0) {
# if defined (__OpenBSD__) || defined(__FreeBSD__) || defined(__APPLE__)
	// BSDs prefix packet with 32-bit address family.
	int af = ntohl(*(unsigned *)b);
	struct click_ether *e;
	WritablePacket *p = Packet::make(_headroom + cc - 4 + sizeof(click_ether));
	p->pull(_headroom);
	e = (struct click_ether *) p->data();
	memset(e, '\0', sizeof(*e));
	if(af == AF_INET){
	    e->ether_type = htons(ETHERTYPE_IP);
	} else if(af == AF_INET6){
	    e->ether_type = htons(ETHERTYPE_IP6);
	} else {
	    click_chatter("KernelTun: don't know af %d", af);
	    p->kill();
	    return;
	}
	memcpy(p->data() + sizeof(click_ether), b + 4, cc - 4);
# elif defined (__linux__)
	Packet *p = Packet::make(_headroom, b, cc, 0);
	if (_type == LINUX_UNIVERSAL)
	    /* Two zero bytes of padding, then the Ethernet type, then the
	       Ethernet header including the type */
	    /* XXX alignment */
	    p->pull(4);
	else
	    /* Two zero bytes of padding, then the Ethernet header */
	    p->pull(2);
# endif

	struct timeval tv;
	int res = gettimeofday(&tv, 0);
	if (res == 0) 
	    p->set_timestamp_anno(tv);
	output(0).push(p);
    } else {
	if (!_ignore_q_errs || !_printed_read_err || (errno != ENOBUFS)) {
	    _printed_read_err = true;
	    perror("KernelTun read");
	}
    }
#endif
}

void
KernelTun::run_scheduled()
{
  if (Packet *p = input(0).pull()) {
    push(0, p); 
  }
  _task.fast_reschedule();
}

void
KernelTun::push(int, Packet *p)
{
    // Every packet has a 14-byte Ethernet header.
    // Extract the packet type, then ignore the Ether header.

    click_ether *e = (click_ether *) p->data();
    if(p->length() < sizeof(*e)){
	click_chatter("KernelTun: packet to small");
	p->kill();
	return;
    }
    int type = ntohs(e->ether_type);
    const unsigned char *data = p->data() + sizeof(*e);
    unsigned length = p->length() - sizeof(*e);

    int num_written;
    int num_expected_written;
#if defined (__OpenBSD__) || defined(__FreeBSD__) || defined(__APPLE__)
    char big[2048];
    int af;

    if(type == ETHERTYPE_IP){
	af = AF_INET;
    } else if(type == ETHERTYPE_IP6){
	af = AF_INET6;
    } else {
	click_chatter("KernelTun: unknown ether type %04x", type);
	p->kill();
	return;
    }

    if(length+4 >= sizeof(big)){
	click_chatter("KernelTun: packet too big (%d bytes)", length);
	p->kill();
	return;
    }
    af = htonl(af);
    memcpy(big, &af, 4);
    memcpy(big+4, data, length);
  
    num_written = write(_fd, big, length + 4);
    num_expected_written = (int) length + 4;
#elif defined(__linux__)
    /*
     * Ethertap is linux equivalent of/dev/tun; wants ethernet header plus 2
     * alignment bytes */
    char big[2048];
    /*
     * ethertap driver is very picky about what address we use here.
     * e.g. if we have the wrong address, linux might ignore all the
     * packets, or accept udp or icmp, but ignore tcp.  aaarrrgh, well
     * this works.  -ddc */
    char to[] = { 0xfe, 0xfd, 0x0, 0x0, 0x0, 0x0 }; 
    char *from = to;
    short protocol = htons(type);
    if(length+16 >= sizeof(big)){
	fprintf(stderr, "bimtun writetun pkt too big\n");
	return;
    }
    memset(big, 0, 16);
    memcpy(big+2, from, sizeof(from)); // linux won't accept ethertap packets from eth addr 0.
    memcpy(big+8, to, sizeof(to)); // linux TCP doesn't like packets to 0??
    memcpy(big+14, &protocol, 2);
    memcpy(big+16, data, length);

    num_written = write(_fd, big, length + 16);
    num_expected_written = (int) length + 16;
#else
    num_written = write(_fd, data, length);
    num_expected_written = (int) length;
#endif

    if (num_written != num_expected_written &&
	(!_ignore_q_errs || !_printed_write_err || (errno != ENOBUFS))) {
	_printed_write_err = true;
	perror("KernelTun write");
    }

    p->kill();
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
ELEMENT_REQUIRES(userlevel)
EXPORT_ELEMENT(KernelTun)
