// -*- mode: c++; c-basic-offset: 2 -*-
#ifndef CLICK_TORAWSOCKET_HH
#define CLICK_TORAWSOCKET_HH
#include <click/element.hh>
#include <click/string.hh>
#include <click/task.hh>
#include <click/notifier.hh>
CLICK_DECLS

/*
=c

ToRawSocket("TCP", <TCP source port number>)
ToRawSocket("UDP", <UDP source port number>)
ToRawSocket("GRE", <GRE key or PPTP call ID>)
ToRawSocket("ICMP", <ICMP identifier>)

=s devices

sends IP packets through a safe raw socket (user-level)

=d

Sends IP packets through a PlanetLab 2.0 safe raw IPv4 socket (see
http://www.planet-lab.org/raw_sockets/). The safe raw IPv4 socket must
be bound to a source port number in the case of TCP/UDP, a GRE key or
PPTP call ID in the case of GRE, or an identifier in the case of
ICMP. IP packets sent through the socket must have correct source IP
addresses and source port numbers/keys/call IDs/identifiers.

An instance of ToRawSocket will attempt to find a FromRawSocket in the
same configuration with the same protocol and bound source port and
reuse its socket.

=e

  ... -> ToRawSocket(UDP, 47)

=a FromRawSocket, FromSocket, ToSocket */

class ToRawSocket : public Element { public:

  ToRawSocket();
  ~ToRawSocket();

  const char *class_name() const	{ return "ToRawSocket"; }
  const char *processing() const	{ return AGNOSTIC; }
  
  int configure(Vector<String> &conf, ErrorHandler *);
  int initialize(ErrorHandler *);
  void cleanup(CleanupStage);
  void add_handlers();

  void notify_noutputs(int);

  void push(int port, Packet *);
  bool run_task();

protected:
  Task _task;
  void send_packet(Packet *);

 private:

  int _fd;
  bool _my_fd;
  int _protocol;
  int _port;
  NotifierSignal _signal;

  int initialize_socket_error(ErrorHandler *, const char *);

};

CLICK_ENDDECLS
#endif
