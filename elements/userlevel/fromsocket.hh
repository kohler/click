// -*- mode: c++; c-basic-offset: 2 -*-
#ifndef CLICK_FROMSOCKET_HH
#define CLICK_FROMSOCKET_HH
#include <click/element.hh>
CLICK_DECLS

/*
=title FromSocket.u

=c

FromSocket("TCP", IP, PORTNUMBER [, I<KEYWORDS>])
FromSocket("UDP", IP, PORTNUMBER [, I<KEYWORDS>])
FromSocket("UNIX", FILENAME [, I<KEYWORDS>])

=s devices

reads data from socket (user-level)

=d

Reads data from the specified socket. Specifying a non-zero IP address
reverses the normal connection sense for TCP FromSockets, i.e. the
FromSocket will attempt to connect(2) instead of listen(2).

Keyword arguments are:

=over 8

=item SNAPLEN

Unsigned integer. Maximum packet length. This value (minus 4 If FRAME
is true), represents the MRU of the FromSocket if it is used as a
packet source. If the MRU is violated by the peer, i.e. if a packet
longer than SNAPLEN (minus 4 is FRAME is true) is sent, the connection
may be terminated.

=item FRAME

Boolean. If true, assume that packets in the data stream are framed
with a 4 byte record boundary in network order that represents the
length of the packet (including the record boundary). Default is true.

=item VERBOSE

Boolean. When true, FromSocket will print messages whenever it accepts a
new connection or drops an old one. Default is false.

=back

=e

  FromSocket(UDP, 0.0.0.0, 47) -> ...

=a ToSocket, FromDump, ToDump, FromDevice, FromDevice.u, ToDevice, ToDevice.u */

class FromSocket : public Element { public:

  FromSocket();
  ~FromSocket();

  const char *class_name() const	{ return "FromSocket"; }
  FromSocket *clone() const		{ return new FromSocket; }

  int configure(Vector<String> &conf, ErrorHandler *);
  int initialize(ErrorHandler *);
  void cleanup(CleanupStage);

  void * handle(int);
  void selected(int);

 private:

  bool _verbose;
  int _fd;
  int _family;
  int _socktype;
  int _protocol;
  IPAddress _ip;
  int _port;
  String _pathname;
  unsigned _snaplen;
  bool _frame;

  Vector<WritablePacket *> _packets;
  Vector<int> _active;

  int initialize_socket_error(ErrorHandler *, const char *);

};

CLICK_ENDDECLS
#endif
