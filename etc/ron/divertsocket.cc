/*
 * fromdevice.{cc,hh} -- element diverts IP packets into Click using divert sockets
 * Alexander Yip
 *
 * Copyright (c) 2001 Massachusetts Institute of Technology
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
#include "divertsocket.hh"
#include <click/error.hh>
#include <click/confparse.hh>
#include <click/glue.hh>
#include <unistd.h>
//#include <fcntl.h>

# include <net/if.h>

#if defined(__FreeBSD__ )
# include <sys/socket.hh>
# include <sys/types.h>
# include <net/if_packet.h>
# include <features.h>

#elif defined(__linux__)
# include <netinet/ip.h>
# include <netinet/in.h>
# include <netinet/tcp.h>
# include <netinet/udp.h>
# include <net/if.h>
# include <sys/param.h>

# include <linux/types.h>
# include <linux/icmp.h>

# if __GLIBC__ >= 2 && __GLIBC_MINOR__ >= 1
#  include <netpacket/packet.h>
#  include <net/ethernet.h>
# else
#  include <linux/if_packet.h>
#  include <linux/if_ether.h>
# endif
#endif

#include <click/click_ip.h>

DivertSocket::DivertSocket() : Element(0,1) 
{
  MOD_INC_USE_COUNT;
  _fd = -1;
}


DivertSocket::~DivertSocket() 
{
  MOD_DEC_USE_COUNT;
  uninitialize();
}

DivertSocket *
DivertSocket::clone() const
{
  return new DivertSocket;
}

int
DivertSocket::configure(const Vector<String> &conf, ErrorHandler *errh)
{
  unsigned char protocol;
  unsigned int sportl, sporth, dportl, dporth;
  IPAddress saddr, smask, daddr, dmask;
  String inout;

  if (cp_va_parse(conf, this, errh,
		  cpByte, "protocol", &protocol,
		  cpIPPrefix, "src address", &saddr, &smask,
		  cpUnsigned, "src port low", &sportl,
		  cpUnsigned, "src port high", &sporth,

		  cpIPPrefix, "dst address", &daddr, &dmask,
		  cpUnsigned, "dst port low", &dportl,
		  cpUnsigned, "dst port high", &dporth,
		  
		  cpString, "in/out", &inout,
		  cpEnd) < 0) 
    return -1;

  if ( (inout != "in") && (inout != "out"))
    return -1;

  

  return 0;
}
		  

int
DivertSocket::initialize(ErrorHandler *) 
{
  struct sockaddr_in bindPort, sin;

  // Setup socket
  _fd = socket(AF_INET, SOCK_RAW, IPPROTO_DIVERT);
  if (_fd == -1) {
    errh->error("DivertSocket: %s", strerror(errno));
    return -1;
  }
  bindPort.sin_family=AF_INET;
  bindPort.sin_port=htons(atol(2002));
  bindPort.sin_addr.s_addr=0;

  ret=bind(_fd, (struct sockaddr *)&bindPort, sizeof(struct sockaddr_in));

  if (ret != 0) {
    close(_fd);
    errh->error("DivertSocket: %s", strerror(errno));
    return -1;
  }

  fcntl(_fd, F_SETFL, O_NONBLOCK);
  
  
  // Setup firewall
  if (system("/sbin/ipfw add 50 divert 2002 ip from 18.239.0.139 to me in") != 0) {
    close (_fd);
    errh->error("ipfw failed");
    return -1;
  }

  

  add_select(_fd, SELECT_READ);
  return 0;
}

  
void 
DivertSocket::uninitialize()
{
  if (_fd >= 0) {
    close (_fd);
    remove_select(_fd, SELECT_READ);
    _fd = -1;
  }
}


void
DivertSocket::selected(int fd) 
{
  struct sockaddr_ll sa;
  socklen_t fromlen;
  
  WritablePacket *p;
  int len;

  if (fd != _fd) 
    return;

  fromlen = sizeof(sa);
  p  = Packet::make(2, 0, _packetbuf_size, 2046, 0); // YIPAL bufsize
  len = recvfrom(_fd, p->data(), p->length(), 0, (sockaddr *)&sa, &fromlen);

  if (len > 0 && sa.sll_pkttype != PACKET_OUTGOING) {
    p->change_headroom_and_length(2, len);
    p->set_packet_type_anno((Packet::PacketType)sa.sll_pkttype);
    (void) ioctl(_fd, SIOCGSTAMP, &p->timestamp_anno());
    if (!_force_ip || check_force_ip(p))
      output(0).push(p);
  } else {
    p->kill();
    if (len <= 0 && errno != EAGAIN)
      click_chatter("FromDevice(%s): recvfrom: %s", _ifname.cc(), strerror(errno));
  }
}

ELEMENT_REQUIRES(userlevel)
EXPORT_ELEMENT(DivertSocket)
