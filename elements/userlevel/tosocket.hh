// -*- mode: c++; c-basic-offset: 2 -*-
#ifndef CLICK_TOSOCKET_HH
#define CLICK_TOSOCKET_HH
#include <click/element.hh>
#include <click/string.hh>
#include <click/task.hh>
#include <click/notifier.hh>
#include <sys/un.h>
CLICK_DECLS

/*
=c

ToSocket("TCP", IP, PORTNUMBER [, I<KEYWORDS>])
ToSocket("UDP", IP, PORTNUMBER [, I<KEYWORDS>])
ToSocket("UNIX", FILENAME [, I<KEYWORDS>])

=s devices

sends data to socket (user-level)

=d

Sends data to the specified socket. Specifying a zero IP address
reverses the normal connection sense for TCP ToSockets, i.e. the
FromSocket will attempt to listen(2) instead of connect(2).

Keyword arguments are:

=over 8

=item SNAPLEN

Unsigned integer. Maximum packet length. This value (minus 4 If FRAME
is true), represents the MTU of the ToSocket if it is used as a packet
sink.

=item FRAME

Boolean. If true, frame packets in the data stream with a 4 byte
record boundary in network order that represents the length of the
packet (including the record boundary). Default is true.

=item NODELAY

Boolean. Applies to TCP sockets only. If set, disable the Nagle
algorithm. This means that segments are always sent as soon as
possible, even if there is only a small amount of data. When not set,
data is buffered until there is a sufficient amount to send out,
thereby avoiding the frequent sending of small packets, which results
in poor utilization of the network. Default is true.

=item VERBOSE

Boolean. When true, ToSocket will print messages whenever it accepts a
new connection or drops an old one. Default is false.

=back

=e

  ... -> ToSocket(1.2.3.4, UDP, 47)

=a FromSocket, FromDump, ToDump, FromDevice, FromDevice.u, ToDevice, ToDevice.u */

class ToSocket : public Element { public:

  ToSocket();
  ~ToSocket();

  const char *class_name() const	{ return "ToSocket"; }
  const char *processing() const	{ return AGNOSTIC; }
  
  int configure(Vector<String> &conf, ErrorHandler *);
  int initialize(ErrorHandler *);
  void cleanup(CleanupStage);
  void add_handlers();

  void notify_noutputs(int);

  void selected(int);

  void push(int port, Packet *);
  bool run_task();

protected:
  Task _task;
  void send_packet(Packet *);

 private:

  bool _verbose;
  int _fd;
  int _active;
  int _family;
  int _socktype;
  int _protocol;
  IPAddress _ip;
  int _port;
  String _pathname;
  int _snaplen;
  union { struct sockaddr_in in; struct sockaddr_un un; } sa;
  socklen_t sa_len;
  NotifierSignal _signal;
  int _nodelay;
  bool _frame;

  int initialize_socket_error(ErrorHandler *, const char *);

};

CLICK_ENDDECLS
#endif
