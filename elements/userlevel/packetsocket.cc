/*
 * packetsocket.{cc,hh} -- element accesses network via linux packet sockets
 * Douglas S. J De Couto
 *
 * Copyright (c) 1999-2000 Massachusetts Institute of Technology.
 *
 * This software is being provided by the copyright holders under the GNU
 * General Public License, either version 2 or, at your discretion, any later
 * version. For more information, see the `COPYRIGHT' file in the source
 * distribution.
 */

/*
 * RTM asks "why isn't there just one user-level FromDevice that does
 * the right thing (or is compiled from the right file) for each O/S?"
 */

#ifdef HAVE_CONFIG_H
# include <config.h>
#endif
#include "packetsocket.hh"

#ifdef __linux__

#include "error.hh"
#include "packet.hh"
#include "confparse.hh"
#include "glue.hh"
#include "elements/standard/scheduleinfo.hh"
#include <fcntl.h>
#include <sys/ioctl.h>
#include <arpa/inet.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/ioctl.h>
#include <net/if.h>
#include <asm/types.h>
#include <linux/if_packet.h>
#include <linux/if_ether.h>  
#include <netinet/in.h>
#include <errno.h>
#include <unistd.h>


PacketSocket::PacketSocket()
{
  add_input();
  add_output();
  _fd = -1;
}

PacketSocket::~PacketSocket()
{
}

PacketSocket *
PacketSocket::clone() const
{
  return new PacketSocket();
}

int
PacketSocket::configure(const Vector<String> &conf, ErrorHandler *errh)
{
  if (cp_va_parse(conf, this, errh,
		  cpString, "device name", &_dev,
		  cpOptional,
		  cpBool, "be promiscuous", &_promisc,
		  cpEnd) < 0)
    return -1;

  return 0;
}

int
PacketSocket::initialize(ErrorHandler *errh)
{
  _fd = socket(PF_PACKET, SOCK_RAW, htons(ETH_P_ALL));
  if (_fd == -1)
    return errh->error("%s, socket: %s", _dev.cc(), strerror(errno));

  struct ifreq ifr;
  // get interface index
  memset(&ifr, 0, sizeof(ifr));
  strncpy(ifr.ifr_name, _dev.cc(), sizeof(ifr.ifr_name));
  int res = ioctl(_fd, SIOCGIFINDEX, &ifr);
  if (res != 0) {
    close(_fd);
    return errh->error("%s, SIOCGIFINDEX: %s", _dev.cc(), strerror(errno));
  }
  _ifindex = ifr.ifr_ifindex;

  if (_promisc) {
    // set promiscuous mode
    memset(&ifr, 0, sizeof(ifr));
    strncpy(ifr.ifr_name, _dev.cc(), sizeof(ifr.ifr_name));
    res = ioctl(_fd, SIOCGIFFLAGS, &ifr);
    if (res != 0) {
      close(_fd);
      return errh->error("%s, SIOCGIFFLAGS: %s", _dev.cc(), strerror(errno));
    }
    ifr.ifr_flags |= IFF_PROMISC;
    res = ioctl(_fd, SIOCSIFFLAGS, &ifr);
    if (res != 0) {
      close(_fd);
      return errh->error("%s, SIOCSIFFLAGS: %s", _dev.cc(), strerror(errno));
    }
  }

  // bind to the specified interface.  from packet man page, only
  // sll_protocol and sll_ifindex fields are used; also have to set
  // sll_family
  sockaddr_ll sa;
  memset(&sa, 0, sizeof(sa));
  sa.sll_family = AF_PACKET;
  sa.sll_protocol = htons(ETH_P_ALL);
  sa.sll_ifindex = _ifindex;
  res = bind(_fd, (sockaddr *) &sa, sizeof(sa));
  if (res != 0) {
    close(_fd);
    return errh->error("%s, bind: %s", _dev.cc(), strerror(errno));
  }

  if (input_is_pull(0))
    ScheduleInfo::join_scheduler(this, errh);
  return 0;
}

void
PacketSocket::uninitialize()
{
  unschedule();
  if (_fd >= 0)
    close(_fd);
}

void
PacketSocket::selected(int fd)
{
  if (fd != _fd)
    return;

  char buf[2048]; // XXX 

  struct sockaddr_ll sa;
  memset(&sa, 0, sizeof(sa));
  socklen_t fromlen = sizeof(sa);
  int res = recvfrom(_fd, buf, sizeof(buf), 0, (sockaddr *) &sa, &fromlen);
  if (res > 0) {
    if (sa.sll_pkttype != PACKET_OUTGOING)
      output(0).push(Packet::make(buf, res));
  }
  else {
    perror("PacketSocket::selected");
  }
}

void
PacketSocket::run_scheduled()
{
  if (Packet *p = input(0).pull()) {
    push(0, p); 
  }
  reschedule();
}

void
PacketSocket::push(int, Packet *p)
{
  struct sockaddr_ll sa;
  memset(&sa, 0, sizeof(sa));
  sa.sll_ifindex = _ifindex;
  socklen_t tolen = sizeof(sa);
  int res = sendto(_fd, p->data(), p->length(), 0, (sockaddr *) &sa, tolen);
  if (res < 0)
    perror("PacketSocket::push");
}

#else /* not __linux__ */
PacketSocket::PacketSocket()
{
}
PacketSocket::~PacketSocket()
{
}
PacketSocket *
PacketSocket::clone() const
{
  return new PacketSocket();
}
#endif

EXPORT_ELEMENT(PacketSocket)
