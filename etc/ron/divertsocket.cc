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
#include <fcntl.h>

# include <net/if.h>

#if defined(__FreeBSD__ )
# include <sys/ioctl.h>
# include <sys/socket.h>
# include <sys/types.h>
# include <net/if.h>

#elif defined(__linux__)
# include <features.h>
# include <net/if_packet.h>
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
		  
		  cpOptional,
		  cpString, "in/out", &inout,
		  cpEnd) < 0) 
    return -1;

  if ( (inout != "in") && (inout != "out"))
    return -1;
 
  printf("done configuring\n");

  return 0;
}
		  

int
DivertSocket::initialize(ErrorHandler *errh) 
{
  int ret;
  struct sockaddr_in bindPort; //, sin;


  // Setup socket
  _fd = socket(AF_INET, SOCK_RAW, IPPROTO_DIVERT);
  if (_fd == -1) {
    errh->error("DivertSocket: %s", strerror(errno));
    return -1;
  }
  bindPort.sin_family=AF_INET;
  bindPort.sin_port=htons(2002);
  bindPort.sin_addr.s_addr=0;

  ret=bind(_fd, (struct sockaddr *)&bindPort, sizeof(struct sockaddr_in));

  if (ret != 0) {
    close(_fd);
    errh->error("DivertSocket: %s", strerror(errno));
    return -1;
  }

  fcntl(_fd, F_SETFL, O_NONBLOCK);
  
  
  // Setup firewall
  if (system("/sbin/ipfw add 50 divert 2002 ip from 18.239.0.139 to me in via fxp0") != 0) {
    close (_fd);
    errh->error("ipfw failed");
    return -1;
  }

  

  add_select(_fd, SELECT_READ);
  printf("done initializing\n");
  return 0;
}

  
void 
DivertSocket::uninitialize()
{
  if (_fd >= 0) {
    system("/sbin/ipfw delete 50");
    close (_fd);
    remove_select(_fd, SELECT_READ);
    _fd = -1;
  }
}


void
DivertSocket::selected(int fd) 
{
  struct sockaddr_in sa;
  socklen_t fromlen;
  
  WritablePacket *p;
  int len;

  if (fd != _fd) 
    return;

  fromlen = sizeof(sa);
  p  = Packet::make(2, 0, 2046, 0); // YIPAL bufsize
  len = recvfrom(_fd, p->data(), p->length(), 0, (sockaddr *)&sa, &fromlen);

  if (len > 0 /* && sa.sll_pkttype != PACKET_OUTGOING) {
		 p->set_packet_type_anno((Packet::PacketType)sa.sll_pkttype);
	      */) {

    //(void) ioctl(_fd, SIOCGSTAMP, &p->timestamp_anno()); 
    // YIPAL fix this timestamp 
    
    p->change_headroom_and_length(2, len);		 
    output(0).push(p);

  } else {
    p->kill();
    if (len <= 0 && errno != EAGAIN)
      click_chatter("DivertSocket: recvfrom: %s", strerror(errno));
  }
}

ELEMENT_REQUIRES(userlevel)
EXPORT_ELEMENT(DivertSocket)


