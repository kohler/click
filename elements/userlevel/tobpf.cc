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

#if defined(__FreeBSD__) && defined(HAVE_PCAP)
# include <fcntl.h>
# include <sys/types.h>
# include <sys/socket.h>
# include <sys/ioctl.h>
# include <net/if.h>
#endif

#ifdef __linux__
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
  if (_pcap) pcap_close(_pcap);
  _pcap = 0;
  return cp_va_parse(conf, this, errh,
		     cpString, "interface name", &_ifname,
		     0);
}

int
ToBPF::initialize(ErrorHandler *errh)
{
#if defined(__FreeBSD__) && defined(HAVE_PCAP)
  
  /* FreeBSD pcap_open_live() doesn't open for writing. */
  if(_fd >= 0)
    return(0);
  else if(!_ifname)
    return errh->error("interface not set");

  int i;
  for(i = 0; i < 16; i++){
    char tmp[64];
    sprintf(tmp, "/dev/bpf%d", i);
    int fd = open(tmp, 1);
    if(fd >= 0){
      _fd = fd;
      break;
    }
  }
  if(_fd < 0)
    return(errh->error("ToBPF: can't open a bpf"));

  struct ifreq ifr;
  (void)strncpy(ifr.ifr_name, _ifname.mutable_c_str(), sizeof(ifr.ifr_name));
  if (ioctl(_fd, BIOCSETIF, (caddr_t)&ifr) < 0)
    return errh->error("ToBPF: BIOCSETIF %s failed", ifr.ifr_name);
  
#else
  
  if (_pcap || _fd >= 0)
    return 0;
  else if (!_ifname)
    return errh->error("interface not set");

  /*
   * Try to find a FromBPF with the same device and re-use its _pcap.
   * If we don't, Linux will give ToBPF's packets to FromBPF.
   */

  /* XXX will need to be redone if libpcap ever starts using PF_SOCKET
     rather than SOCK_PACKET */
  for(int fi = 0; fi < router()->nelements(); fi++){
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
# ifdef HAVE_PCAP
    if (!_pcap)
      return errh->error("%s: %s", _ifname.cc(), ebuf);
# else
    errh->warning("dropping all packets: not compiled with pcap support");
# endif
    _fd = pcap_fileno(_pcap);
  }

# ifdef __linux__
  {
#  if 1 /* Kuznetsov patch */
    struct ifreq ifr;
    strcpy(ifr.ifr_name, _ifname);
    if (ioctl(_fd, SIOCGIFINDEX, &ifr) < 0) {
      int err = errno;
      if (_pcap) pcap_close(_pcap);
      return errh->error("bad ioctl: %s", strerror(err));
    }

    struct sockaddr_ll sll;
    sll.sll_family = AF_PACKET;
    sll.sll_ifindex = ifr.ifr_ifindex;
    sll.sll_protocol = 0;
    if (bind(_fd, (struct sockaddr *)&sll, sizeof(sll)) < 0) {
      int err = errno;
      if (_pcap) pcap_close(_pcap);
      return errh->error("cannot bind: %s", strerror(err));
    }

#  else
    struct sockaddr sa;
    strcpy(sa.sa_data, _ifname);
    if (bind(_fd, &sa, sizeof(sa) < 0)) {
      int err = errno;
      if (_pcap) pcap_close(_pcap);
      return errh->error("cannot bind: %s", strerror(err));
    }
#  endif
  }
# endif

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

#ifdef HAVE_PCAP
# ifdef __linux__
  if (send(_fd, p->data(), p->length(), 0) < 0)
    click_chatter("ToBPF(%s) send: %s", _ifname.cc(), strerror(errno));
# endif

# if defined(__FreeBSD__) || defined(__OpenBSD__)
  int ret = write(_fd, p->data(), p->length());
  if(ret <= 0){
    fprintf(stderr, "ToBPF: write %d to _fd %d ret %d errno %d : %s\n",
            p->length(), _fd, ret, errno, strerror(errno));
  }
# endif
#endif

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
