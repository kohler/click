/*
 * kerneltap.{cc,hh} -- element accesses network via /dev/tun device
 * Robert Morris, Douglas S. J. De Couto, Eddie Kohler
 *
 * Copyright (c) 1999-2000 Massachusetts Institute of Technology
 *
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files (the "Software"),
 * to deal in the Software without restriction, subject to the following
 * conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * Further elaboration of this license, including a DISCLAIMER OF ANY
 * WARRANTY, EXPRESS OR IMPLIED, is provided in the LICENSE file, which is
 * also accessible at http://www.pdos.lcs.mit.edu/click/license.html
 */

#ifdef HAVE_CONFIG_H
# include <config.h>
#endif
#include <click/config.h>
#include <click/package.hh>
#include "kerneltap.hh"
#include <click/error.hh>
#include <click/bitvector.hh>
#include <click/confparse.hh>
#include <click/glue.hh>
#include <click/click_ether.h>
#include "elements/standard/scheduleinfo.hh"
#include <unistd.h>
#include <fcntl.h>
#include <sys/ioctl.h>
#include <arpa/inet.h>
#if defined(__FreeBSD__) || defined(__OpenBSD__)
#include <net/if.h>
#include <net/if_tun.h>
#endif

KernelTap::KernelTap()
  : Element(1, 1), _task(this)
{
  MOD_INC_USE_COUNT;
  _fd = -1;
}

KernelTap::~KernelTap()
{
  MOD_DEC_USE_COUNT;
}

KernelTap *
KernelTap::clone() const
{
  return new KernelTap();
}

Bitvector
KernelTap::forward_flow(int) const
{
  // packets never travel from input to output
  return Bitvector(1, false);
}

Bitvector
KernelTap::backward_flow(int) const
{
  return Bitvector(1, false);
}

int
KernelTap::configure(const Vector<String> &conf, ErrorHandler *errh)
{
  _gw = IPAddress();
  _headroom = 0;
  if (cp_va_parse(conf, this, errh,
		  cpIPPrefix, "network address", &_near, &_mask,
		  cpOptional,
		  cpIPAddress, "default gateway", &_gw,
		  cpUnsigned, "packet data headroom", &_headroom,
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

/*
 * Find an kill()d /dev/tun* device, return a fd to it.
 * Exits on failure.
 */
int
KernelTap::alloc_tun(struct in_addr near, struct in_addr mask,
		     ErrorHandler *errh)
{
  int fd;
  char tmp[512], tmp0[64], tmp1[64], dev_prefix[64];
  int saved_errno = 0;

#if !(defined(__linux__) || defined(__FreeBSD__) || defined(__OpenBSD__))
  return errh->error("KernelTap is not yet supported on this system\n(Please report this message to click@pdos.lcs.mit.edu.)");
#endif
  
#ifdef __linux__
  strcpy(dev_prefix, "tap");
#else
  strcpy(dev_prefix, "tun");
#endif

  for (int i = 0; i < 32; i++) {
    sprintf(tmp, "/dev/%s%d", dev_prefix, i);
    fd = open(tmp, O_RDWR | O_NONBLOCK);
    if (fd < 0) {
      if (saved_errno == 0 || errno != ENOENT)
        saved_errno = errno;
      continue;
    }

    _dev_name = String(dev_prefix) + String(i);

#if defined(TUNSIFMODE) || defined(__FreeBSD__)
    {
      int mode = IFF_BROADCAST;
      if (ioctl(fd, TUNSIFMODE, &mode) != 0)
	return errh->error("TUNSIFMODE failed: %s", strerror(errno));
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
      close(fd);
# if defined(__linux__)
    // Is Ethertap available? If it is moduleified, then it might not be.
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
      if (system(tmp) != 0) {
	close(fd);
	return errh->error("%s: %s", tmp, strerror(errno));
      }
    }
    
    return fd;
  }

  return errh->error("could not allocate a /dev/%s* device: %s",
                     dev_prefix, strerror(saved_errno));
}

void
KernelTap::dealloc_tun()
{
  String cmd = "/sbin/ifconfig " + _dev_name + " down";
  if (system(cmd.cc()) != 0) 
    click_chatter("%s: failed: %s", id().cc(), cmd.cc());
}

int
KernelTap::initialize(ErrorHandler *errh)
{
  _fd = alloc_tun(_near, _mask, errh);
  if (_fd < 0)
    return -1;
  if (input_is_pull(0))
    ScheduleInfo::join_scheduler(this, &_task, errh);
  add_select(_fd, SELECT_READ);
  return 0;
}

void
KernelTap::uninitialize()
{
  _task.unschedule();
  if (_fd >= 0) {
    close(_fd);
    remove_select(_fd, SELECT_READ);
  }
  dealloc_tun();
}

void
KernelTap::selected(int fd)
{
  int cc;
  unsigned char b[2048];

  if (fd != _fd)
    return;
  
  cc = read(_fd, b, sizeof(b));
  if (cc > 0) {
#if defined (__OpenBSD__) || defined(__FreeBSD__)
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
      click_chatter("KernelTap: don't know af %d", af);
      p->kill();
      return;
    }
    memcpy(p->data() + sizeof(click_ether), b + 4, cc - 4);
    output(0).push(p);
#elif defined (__linux__)
    // Linux prefixes packet 2 bytes of 0, then ether_header.
    Packet *p = Packet::make(_headroom, b + 2, cc - 2, 0);
    output(0).push(p);
#endif
  } else {
    perror("KernelTap read");
  }
}

void
KernelTap::run_scheduled()
{
  if (Packet *p = input(0).pull()) {
    push(0, p); 
  }
  _task.fast_reschedule();
}

void
KernelTap::push(int, Packet *p)
{
  // Every packet has a 14-byte Ethernet header.
  // Extract the packet type, then ignore the Ether header.

  click_ether *e = (click_ether *) p->data();
  if(p->length() < sizeof(*e)){
    click_chatter("KernelTap: packet to small");
    p->kill();
    return;
  }
  int type = ntohs(e->ether_type);
  const unsigned char *data = p->data() + sizeof(*e);
  unsigned length = p->length() - sizeof(*e);

#if defined (__OpenBSD__) || defined(__FreeBSD__)
  char big[2048];
  int af;

  if(type == ETHERTYPE_IP){
    af = AF_INET;
  } else if(type == ETHERTYPE_IP6){
    af = AF_INET6;
  } else {
    click_chatter("KernelTap: unknown ether type %04x", type);
    p->kill();
    return;
  }

  if(length+4 >= sizeof(big)){
    click_chatter("KernelTap: packet too big (%d bytes)", length);
    p->kill();
    return;
  }
  af = htonl(af);
  memcpy(big, &af, 4);
  memcpy(big+4, data, length);
  if(write(_fd, big, length+4) != (int)length+4){
    perror("write tun");
  }
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
  if (write(_fd, big, length+16) != (int)length+16){
    perror("write tun");
  }
#else
  if(write(_fd, data, length) != (int) length){
    perror("write tun");
  }
#endif

  p->kill();
}

void
KernelTap::add_handlers()
{
  if (input_is_pull(0))
    add_task_handlers(&_task);
}

ELEMENT_REQUIRES(userlevel)
EXPORT_ELEMENT(KernelTap)
