/*
 * todevice.{cc,hh} -- element writes packets to network via pcap library
 * Douglas S. J. DeCouto, Eddie Kohler, John Jannotti
 *
 * Copyright (c) 1999-2000 Massachusetts Institute of Technology.
 *
 * This software is being provided by the copyright holders under the GNU
 * General Public License, either version 2 or, at your discretion, any later
 * version. For more information, see the `COPYRIGHT' file in the source
 * distribution.
 */

#ifdef HAVE_CONFIG_H
# include <config.h>
#endif
#include "todevice.hh"
#include "error.hh"
#include "etheraddress.hh"
#include "confparse.hh"
#include "router.hh"
#include "elements/standard/scheduleinfo.hh"

#include <stdio.h>
#include <assert.h>
#include <unistd.h>
#include <errno.h>

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
  : Element(1, 0), _fd(-1), _my_fd(false)
{
#if TODEVICE_BSD_DEV_BPF
  _pcap = 0;
#endif
}

ToDevice::~ToDevice()
{
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
    return(errh->error("can't open a bpf"));

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
    ScheduleInfo::join_scheduler(this, errh);
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
  unschedule();
}

void
ToDevice::send_packet(Packet *p)
{
  int retval;
  const char *syscall;

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
  reschedule();
}

ELEMENT_REQUIRES(FromDevice userlevel)
EXPORT_ELEMENT(ToDevice)
