/*
 * tobpf.{cc,hh} -- element writes packets to network via pcap library
 * John Jannotti
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
#include "tobpf.hh"
#include "error.hh"
#include "etheraddress.hh"
#include "confparse.hh"
#include "router.hh"
#include "elements/standard/scheduleinfo.hh"

#include <stdio.h>
#include <assert.h>
#include <unistd.h>
#include <errno.h>

#if TOBPF_BSD_DEV_BPF
# include <fcntl.h>
# include <sys/types.h>
# include <sys/socket.h>
# include <sys/ioctl.h>
# include <net/if.h>
#elif TOBPF_LINUX
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

ToBPF::ToBPF()
  : Element(1, 0), _fd(-1), _pcap(0)
{
}

ToBPF::ToBPF(const String &ifname)
  : Element(1, 0), _ifname(ifname), _fd(-1), _pcap(0)
{
}

ToBPF::~ToBPF()
{
  uninitialize();
}

ToBPF *
ToBPF::clone() const
{
  return new ToBPF(_ifname);
}

int
ToBPF::configure(const String &conf, ErrorHandler *errh)
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
ToBPF::initialize(ErrorHandler *errh)
{
  _fd = -1;
#if TOBPF_SENDTO
  _sendto_sa.sa_family = 0;
#endif
  
#if TOBPF_BSD_DEV_BPF
  
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

#elif TOBPF_LINUX
  
  /*
   * Try to find a FromBPF with the same device and re-use its _pcap.
   * If we don't, Linux will give ToBPF's packets to FromBPF.
   */
  for (int fi = 0; fi < router()->nelements() && _fd < 0; fi++) {
    Element *f = router()->element(fi);
    FromBPF *lr = (FromBPF *)f->cast("FromBPF");
    if (lr && lr->get_ifname() == _ifname && lr->get_pcap())
      _fd = pcap_fileno(lr->get_pcap());
  }
  
  if (_fd < 0) {
    char ebuf[PCAP_ERRBUF_SIZE];
    _pcap = pcap_open_live(_ifname.mutable_c_str(),
                           12000, // XXX snaplen
			   0,     // not promiscuous
                           0,     // don't batch packets
                           ebuf);
    if (!_pcap)
      return errh->error("%s: %s", _ifname.cc(), ebuf);
    _fd = pcap_fileno(_pcap);
  }

  // find device ifindex
  struct ifreq ifr;
  strncpy(ifr.ifr_name, _ifname, sizeof(ifr.ifr_name));
  ifr.ifr_name[sizeof(ifr.ifr_name) - 1] = 0;
  if (ioctl(_fd, SIOCGIFINDEX, &ifr) < 0) {
    int err = errno;
    if (_pcap) pcap_close(_pcap);
    return errh->error("bad ioctl: %s", strerror(err));
  }

  // bind the packet socket to the device
  int retval;

# if TOBPF_SEND
  struct sockaddr_ll sll;
  sll.sll_family = AF_PACKET;
  sll.sll_ifindex = ifr.ifr_ifindex;
  sll.sll_protocol = 0;
  retval = bind(_fd, (struct sockaddr *)&sll, sizeof(sll));
# endif

# if TOBPF_SENDTO
#  if TOBPF_SEND
  if (retval < 0 && errno == EINVAL) {
#  endif
    // assume that libpcap does not contain Kuznetsov patches;
    // use SOCK_PACKET method rather than newer PF_PACKET
    _sendto_sa.sa_family = AF_INET;
    strncpy(_sendto_sa.sa_data, _ifname, sizeof(_sendto_sa.sa_data));
    _sendto_sa.sa_data[sizeof(_sendto_sa.sa_data) - 1] = 0;
    retval = bind(_fd, &_sendto_sa, sizeof(_sendto_sa));
#  if TOBPF_SEND
  }
#  endif
# endif

  // return error
  if (retval < 0) {
    int err = errno;
    if (_pcap) pcap_close(_pcap);
    errh->error("cannot bind: %s", strerror(err));
# if !TOBPF_SENDTO
    errh->message("(Perhaps you have the wrong version of libpcap. Have you applied Alexey");
    errh->message("Kuznetsov's patches for Linux 2.2?)");
# elif !TOBPF_SEND
    errh->message("(Perhaps you have the wrong version of libpcap. Recompile with");
    errh->message("TOBPF_SEND support.)");
# endif
    return -1;
  }

#else
  
  return errh->error("ToBPF is not supported on this platform");
  
#endif

  if (input_is_pull(0))
    ScheduleInfo::join_scheduler(this, errh);
  return 0;
}

void
ToBPF::uninitialize()
{
  if (_pcap) pcap_close(_pcap);
  unschedule();
}

void
ToBPF::push(int, Packet *p)
{
  assert(p->length() >= 14);
  
  int retval;
  const char *syscall;

#if TOBPF_WRITE
  retval = (write(_fd, p->data(), p->length()) > 0 ? 0 : -1);
  syscall = "write";
#elif TOBPF_SEND && TOBPF_SENDTO
  if (_sendto_sa.sa_family > 0) {
    retval = sendto(_fd, p->data(), p->length(), 0, &_sendto_sa, sizeof(_sendto_sa));
    syscall = "sendto";
  } else {
    retval = send(_fd, p->data(), p->length(), 0);
    syscall = "send";
  }
#elif TOBPF_SEND
  retval = send(_fd, p->data(), p->length(), 0);
  syscall = "send";
#elif TOBPF_SENDTO
  retval = sendto(_fd, p->data(), p->length(), 0, &_sendto_sa, sizeof(_sendto_sa));
  syscall = "sendto";
#else
  retval = 0;
#endif

  if (retval < 0)
    click_chatter("ToBPF(%d) %s: %s", _ifname.cc(), syscall, strerror(errno));
  p->kill();
}

void
ToBPF::run_scheduled()
{
  // XXX reduce tickets when idle
  if (Packet *p = input(0).pull())
    push(0, p); 
  reschedule();
}

EXPORT_ELEMENT(ToBPF)
ELEMENT_REQUIRES(FromBPF)
