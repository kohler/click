/*
 * todevice.{cc,hh} -- element writes packets to network via pcap library
 * Douglas S. J. De Couto, Eddie Kohler, John Jannotti
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
#include "todevice.hh"
#include <click/error.hh>
#include <click/etheraddress.hh>
#include <click/confparse.hh>
#include <click/router.hh>
#include <click/standard/scheduleinfo.hh>

#include <stdio.h>
#include <assert.h>
#include <unistd.h>

#if TODEVICE_BSD_DEV_BPF
# include <fcntl.h>
# include <sys/types.h>
# include <sys/socket.h>
# include <sys/ioctl.h>
# include <net/if.h>
#elif TODEVICE_LINUX
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

ToDevice::ToDevice()
  : Element(1, 0), _fd(-1), _my_fd(false), _task(this)
{
  MOD_INC_USE_COUNT;
#if TODEVICE_BSD_DEV_BPF
  _pcap = 0;
#endif
}

ToDevice::~ToDevice()
{
  MOD_DEC_USE_COUNT;
  uninitialize();
}

ToDevice *
ToDevice::clone() const
{
  return new ToDevice;
}

int
ToDevice::configure(const Vector<String> &conf, ErrorHandler *errh)
{
  if (cp_va_parse(conf, this, errh,
		  cpString, "interface name", &_ifname,
		  0) < 0)
    return -1;
  if (!_ifname)
    return errh->error("interface not set");
  return 0;
}

int
ToDevice::initialize(ErrorHandler *errh)
{
  _fd = -1;

#if TODEVICE_BSD_DEV_BPF
  
  /* pcap_open_live() doesn't open for writing. */
  for (int i = 0; i < 16 && _fd < 0; i++) {
    char tmp[64];
    sprintf(tmp, "/dev/bpf%d", i);
    _fd = open(tmp, 1);
  }
  if (_fd < 0)
    return(errh->error("open /dev/bpf* for write: %s", strerror(errno)));

  struct ifreq ifr;
  strncpy(ifr.ifr_name, _ifname, sizeof(ifr.ifr_name));
  ifr.ifr_name[sizeof(ifr.ifr_name) - 1] = 0;
  if (ioctl(_fd, BIOCSETIF, (caddr_t)&ifr) < 0)
    return errh->error("BIOCSETIF %s failed", ifr.ifr_name);

#elif TODEVICE_LINUX
  
  // find a FromDevice and reuse its socket if possible
  for (int ei = 0; ei < router()->nelements() && _fd < 0; ei++) {
    Element *e = router()->element(ei);
    FromDevice *fdev = (FromDevice *)e->cast("FromDevice");
    if (fdev && fdev->ifname() == _ifname && fdev->fd() >= 0) {
      _fd = fdev->fd();
      _my_fd = false;
    }
  }

  if (_fd < 0) {
    _fd = FromDevice::open_packet_socket(_ifname, errh);
    _my_fd = true;
  }

  if (_fd < 0) return -1;

#else
  
  return errh->error("ToDevice is not supported on this platform");
  
#endif

  if (input_is_pull(0))
    ScheduleInfo::join_scheduler(this, &_task, errh);
  return 0;
}

void
ToDevice::uninitialize()
{
#if TODEVICE_BSD_DEV_BPF
  if (_pcap) pcap_close(_pcap);
  _pcap = 0;
#endif
#if TODEVICE_LINUX
  if (_fd >= 0 && _my_fd) close(_fd);
  _fd = -1;
#endif
}

void
ToDevice::send_packet(Packet *p)
{
  struct timeval tp;
  int retval;
  const char *syscall;

  gettimeofday(&tp, NULL);
  //click_chatter("pOUT_TODEV: (%d,%d)", tp.tv_sec, tp.tv_usec);

#if TODEVICE_WRITE
  retval = (write(_fd, p->data(), p->length()) > 0 ? 0 : -1);
  syscall = "write";
#elif TODEVICE_SEND
  retval = send(_fd, p->data(), p->length(), 0);
  syscall = "send";
#else
  retval = 0;
#endif

  if (retval < 0)
    click_chatter("ToDevice(%s) %s: %s", _ifname.cc(), syscall, strerror(errno));
  p->kill();
}

void
ToDevice::push(int, Packet *p)
{
  assert(p->length() >= 14);
  send_packet(p);
}

void
ToDevice::run_scheduled()
{
  // XXX reduce tickets when idle
  if (Packet *p = input(0).pull())
    send_packet(p); 
  _task.fast_reschedule();
}

void
ToDevice::add_handlers()
{
  if (input_is_pull(0))
    add_task_handlers(&_task);
}

ELEMENT_REQUIRES(FromDevice userlevel)
EXPORT_ELEMENT(ToDevice)
