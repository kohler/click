// -*- mode: c++; c-basic-offset: 2 -*-
#ifndef CLICK_FROMRAWSOCKET_HH
#define CLICK_FROMRAWSOCKET_HH
#include <click/element.hh>
CLICK_DECLS

/*
=c

FromRawSocket("TCP", <TCP source port number> [, I<KEYWORDS>])
FromRawSocket("UDP", <UDP source port number> [, I<KEYWORDS>])
FromRawSocket("GRE", <GRE key or PPTP call ID> [, I<KEYWORDS>])
FromRawSocket("ICMP", <ICMP identifier> [, I<KEYWORDS>])
FromRawSocket("ICMP_TCP", <TCP source port number> [, I<KEYWORDS>])
FromRawSocket("ICMP_UDP", <UDP source port number> [, I<KEYWORDS>])
FromRawSocket("ICMP_GRE", <GRE key or PPTP call ID> [, I<KEYWORDS>])

=s devices

reads data from safe raw socket (user-level)

=d

Reads data from the specified PlanetLab 2.0 safe raw IPv4 socket (see
http://www.planet-lab.org/raw_sockets/). The safe raw IPv4 socket must
be bound to a source port number in the case of TCP/UDP, a GRE key or
PPTP call ID in the case of GRE, or an identifier in the case of
ICMP. In the case of ICMP_TCP, ICMP_UDP, or ICMP_GRE, specify a source
port number, GRE key, or PPTP call ID to receive ICMP errors
associated with those connections.

Keyword arguments are:

=over 8

=item SNAPLEN

Unsigned integer. Maximum packet length. This value
represents the MRU of the FromRawSocket if it is used as a
packet source. If the MRU is violated by the peer, i.e. if a packet
longer than SNAPLEN is sent, the connection may be terminated.

=item SNIFF

Boolean. When true, FromRawSocket will be a raw "sniffer" socket.

=back

=e

  FromRawSocket(UDP, 53) -> ...

=a ToRawSocket, FromSocket, ToSocket */

class FromRawSocket : public Element { public:

  FromRawSocket();
  ~FromRawSocket();

  const char *class_name() const	{ return "FromRawSocket"; }

  int configure(Vector<String> &conf, ErrorHandler *);
  int initialize(ErrorHandler *);
  void cleanup(CleanupStage);

  int protocol() const			{ return _protocol; }
  int port() const			{ return _port; }
  int fd() const			{ return _fd; }

  void selected(int);

 private:

  int _fd;
  int _protocol;
  uint16_t _port;
  int _snaplen;
  bool _sniff;

  WritablePacket *_packet;

  int initialize_socket_error(ErrorHandler *, const char *);

};

CLICK_ENDDECLS
#endif
