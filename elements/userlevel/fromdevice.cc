/*
 * fromdevice.{cc,hh} -- element reads packets live from network via pcap
 * Douglas S. J. De Couto, Eddie Kohler, John Jannotti
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
#include "fromdevice.hh"
#include "todevice.hh"
#include "error.hh"
#include "packet.hh"
#include "confparse.hh"
#include "glue.hh"
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>

#include <sys/ioctl.h>

#if FROMDEVICE_LINUX
# include <sys/socket.h>
# include <net/if.h>
# include <net/if_packet.h>
# include <features.h>
# if __GLIBC__ >= 2 && __GLIBC_MINOR__ >= 1
#  include <netpacket/packet.h>
#  include <net/ethernet.h>
# else
#  include <linux/if_packet.h>
#  include <linux/if_ether.h>
# endif
#endif

FromDevice::FromDevice()
  : _promisc(0), _packetbuf_size(0)
{
  add_output();
#if FROMDEVICE_PCAP
  _pcap = 0;
#endif
#if FROMDEVICE_LINUX
  _fd = -1;
  _packetbuf = 0;
#endif
}

FromDevice::~FromDevice()
{
  uninitialize();
}

FromDevice *
FromDevice::clone() const
{
  return new FromDevice;
}

int
FromDevice::configure(const Vector<String> &conf, ErrorHandler *errh)
{
  bool promisc = false;
  _packetbuf_size = 2048;
  if (cp_va_parse(conf, this, errh,
		  cpString, "interface name", &_ifname,
		  cpOptional,
		  cpBool, "be promiscuous?", &promisc,
#if FROMDEVICE_LINUX
		  cpUnsigned, "maximum packet length", &_packetbuf_size,
#endif
		  cpEnd) < 0)
    return -1;
  if (_packetbuf_size > 8192 || _packetbuf_size < 128)
    return errh->error("maximum packet length out of range");
  _promisc = promisc;
  return 0;
}

#if FROMDEVICE_LINUX
int
FromDevice::open_packet_socket(String ifname, ErrorHandler *errh)
{
  int fd = socket(PF_PACKET, SOCK_RAW, htons(ETH_P_ALL));
  if (fd == -1)
    return errh->error("%s: socket: %s", ifname.cc(), strerror(errno));

  // get interface index
  struct ifreq ifr;
  memset(&ifr, 0, sizeof(ifr));
  strncpy(ifr.ifr_name, ifname.cc(), sizeof(ifr.ifr_name));
  int res = ioctl(fd, SIOCGIFINDEX, &ifr);
  if (res != 0) {
    close(fd);
    return errh->error("%s: SIOCGIFINDEX: %s", ifname.cc(), strerror(errno));
  }
  int ifindex = ifr.ifr_ifindex;

  // bind to the specified interface.  from packet man page, only
  // sll_protocol and sll_ifindex fields are used; also have to set
  // sll_family
  sockaddr_ll sa;
  memset(&sa, 0, sizeof(sa));
  sa.sll_family = AF_PACKET;
  sa.sll_protocol = htons(ETH_P_ALL);
  sa.sll_ifindex = ifindex;
  res = bind(fd, (struct sockaddr *)&sa, sizeof(sa));
  if (res != 0) {
    close(fd);
    return errh->error("%s: bind: %s", ifname.cc(), strerror(errno));
  }

  // nonblocking I/O on the packet socket so we can poll
  fcntl(fd, F_SETFL, O_NONBLOCK);
  
  return fd;
}

int
FromDevice::set_promiscuous(int fd, String ifname, bool promisc)
{
  // get interface flags
  struct ifreq ifr;
  memset(&ifr, 0, sizeof(ifr));
  strncpy(ifr.ifr_name, ifname.cc(), sizeof(ifr.ifr_name));
  int res = ioctl(fd, SIOCGIFFLAGS, &ifr);
  if (res != 0)
    return -2;
  int was_promisc = (ifr.ifr_flags & IFF_PROMISC ? 1 : 0);

  // set or reset promiscuous flag
  if (was_promisc != promisc) {
    ifr.ifr_flags = (promisc ? ifr.ifr_flags | IFF_PROMISC : ifr.ifr_flags & ~IFF_PROMISC);
    res = ioctl(fd, SIOCSIFFLAGS, &ifr);
    if (res != 0)
      return -3;
  }

  return was_promisc;
}
#endif

int
FromDevice::initialize(ErrorHandler *errh)
{
  if (!_ifname)
    return errh->error("interface not set");

  /* 
   * Later versions of pcap distributed with linux (e.g. the redhat
   * linux pcap-0.4-16) want to have a filter installed before they
   * will pick up any packets.
   */

#if FROMDEVICE_PCAP
  
  assert(!_pcap);
  char *ifname = _ifname.mutable_c_str();
  char ebuf[PCAP_ERRBUF_SIZE];
  _pcap = pcap_open_live(ifname,
                         12000, /* XXX snaplen */
                         _promisc,
                         1,     /* timeout: don't wait for packets */
                         ebuf);
  if (!_pcap)
    return errh->error("%s: %s", ifname, ebuf);

  // nonblocking I/O on the packet socket so we can poll
  int fd = pcap_fileno(_pcap);
  fcntl(fd, F_SETFL, O_NONBLOCK);

#if defined(BIOCSSEESENT) || defined(__FreeBSD__)
  {
    int no = 0;
    if(ioctl(fd, BIOCSSEESENT, &no) != 0){
      return errh->error("FromDevice: BIOCSEESENT failed");
    }
  }
#endif

#if defined(BIOCIMMEDIATE) || defined(__FreeBSD__)
  {
    int yes = 1;
    if(ioctl(fd, BIOCIMMEDIATE, &yes) != 0){
      return errh->error("FromDevice: BIOCIMMEDIATE failed");
    }
  }
#endif

  bpf_u_int32 netmask;
  bpf_u_int32 localnet;
  if (pcap_lookupnet(ifname, &localnet, &netmask, ebuf) < 0) {
    errh->warning("%s: %s", ifname, ebuf);
  }
  
  struct bpf_program fcode;
  /* 
   * assume we can use 0 pointer for program string and get ``empty''
   * filter program.  
   */
  if (pcap_compile(_pcap, &fcode, 0, 0, netmask) < 0) {
    return errh->error("%s: %s", ifname, pcap_geterr(_pcap));
  }

  if (pcap_setfilter(_pcap, &fcode) < 0) {
    return errh->error("%s: %s", ifname, pcap_geterr(_pcap));
  }

  add_select(fd, SELECT_READ);

#elif FROMDEVICE_LINUX

  _fd = open_packet_socket(_ifname, errh);
  if (_fd < 0) return -1;

  int promisc_ok = set_promiscuous(_fd, _ifname, _promisc);
  if (promisc_ok < 0) {
    if (_promisc)
      errh->warning("cannot set promiscuous mode");
    _was_promisc = -1;
  } else
    _was_promisc = promisc_ok;

  // create packet buffer
  _packetbuf = new unsigned char[_packetbuf_size];
  if (!_packetbuf) {
    close(_fd);
    return errh->error("out of memory");
  }

  add_select(_fd, SELECT_READ);
  
#else

  return errh->error("FromDevice is not supported on this platform");
  
#endif

  return 0;
}

void
FromDevice::uninitialize()
{
#if FROMDEVICE_PCAP
  if (_pcap)
    pcap_close(_pcap);
  _pcap = 0;
#endif
#if FROMDEVICE_LINUX
  if (_was_promisc >= 0)
    set_promiscuous(_fd, _ifname, _was_promisc);
  if (_fd >= 0) {
    close(_fd);
    remove_select(_fd, SELECT_READ);
    _fd = -1;
  }
  delete[] _packetbuf;
  _packetbuf = 0;
#endif
}

#if FROMDEVICE_PCAP
void
FromDevice::get_packet(u_char* clientdata,
		       const struct pcap_pkthdr* pkthdr,
		       const u_char* data)
{
  FromDevice *fd = (FromDevice *) clientdata;
  int length = pkthdr->caplen;
  Packet* p = Packet::make(data, length);
  fd->output(0).push(p);
}
#endif

void
FromDevice::selected(int)
{
#ifdef FROMDEVICE_PCAP
  // Read and push() at most one packet.
  pcap_dispatch(_pcap, 1, FromDevice::get_packet, (u_char *) this);
#endif
#ifdef FROMDEVICE_LINUX
  struct sockaddr_ll sa;
  //memset(&sa, 0, sizeof(sa));
  socklen_t fromlen = sizeof(sa);
  int len = recvfrom(_fd, _packetbuf, _packetbuf_size, 0,
		     (sockaddr *)&sa, &fromlen);
  if (len > 0) {
    if (sa.sll_pkttype != PACKET_OUTGOING)
      output(0).push(Packet::make(_packetbuf, len));
  } else if (errno != EAGAIN)
    click_chatter("FromDevice(%s): recvfrom: %s", _ifname.cc(), strerror(errno));
#endif
}

ELEMENT_REQUIRES(userlevel)
EXPORT_ELEMENT(FromDevice)
