// -*- mode: c++; c-basic-offset: 2 -*-
#ifndef CLICK_RAWSOCKET_HH
#define CLICK_RAWSOCKET_HH
#include <click/element.hh>
#include <click/task.hh>
#include <click/timer.hh>
#include <click/notifier.hh>

#ifdef __linux__
# define FROMDEVICE_ALLOW_LINUX 1
#endif

CLICK_DECLS

/*
=c

RawSocket("TCP", <TCP source port number> [, I<KEYWORDS>])
RawSocket("UDP", <UDP source port number> [, I<KEYWORDS>])
RawSocket("GRE", <GRE key or PPTP call ID> [, I<KEYWORDS>])
RawSocket("ICMP", <ICMP identifier> [, I<KEYWORDS>])

=s comm

transports raw IP packets via safe raw sockets (user-level)

=d

Reads data from and writes data to a raw IPv4 socket. The raw IPv4
socket may optionally be bound to a source port number in the case of
TCP/UDP, a GRE key or PPTP call ID in the case of GRE, or an
identifier in the case of ICMP.  For PlanetLab Linux (versions with VMNET+ onwards)
and TCP or UDP protocol, a seperate SOCK_STREAM or SOCK_DGRAM will be
created and  bound to the specified port. This triggers the slice port registration
mechanism in VMNET+.

Keyword arguments are:

=over 8

=item SNAPLEN

Unsigned integer. Maximum packet length. This value
represents the MRU of the RawSocket if it is used as a
packet source. If the MRU is violated by the peer, i.e. if a packet
longer than SNAPLEN is sent, the connection may be terminated.

=item HEADROOM

Unsigned Integer. Amount of headroom to reserve in packets created
by this element. This could be useful for encapsulation protocols
which add headers to the packet, and can avoid expensive push
operations later in the packet's life.

=back

=e

  RawSocket(UDP, 53) -> ...

=a Socket */

class RawSocket : public Element { public:

  RawSocket() CLICK_COLD;
  ~RawSocket() CLICK_COLD;

  const char *class_name() const	{ return "RawSocket"; }
  const char *port_count() const	{ return "0-1/0-1"; }
  const char *processing() const	{ return "l/h"; }
  const char *flow_code() const		{ return "x/y"; }
  const char *flags() const		{ return "S3"; }

  int configure(Vector<String> &conf, ErrorHandler *) CLICK_COLD;
  int initialize(ErrorHandler *) CLICK_COLD;
  void cleanup(CleanupStage) CLICK_COLD;
  void add_handlers() CLICK_COLD;

  bool run_task(Task *);
  void selected(int fd, int mask);
  void run_timer(Timer *);

protected:
  Task _task;
  Timer _timer;

private:
  int _fd;			// socket descriptor
  int _port_register_socket;      // socket for planetlab port registration
  int _protocol;		// IP protocol to bind
  uint16_t _port;		// (PlanetLab only) port to bind
  int _snaplen;			// maximum received packet length
  unsigned _headroom;           // header length to set aside in the packet

  NotifierSignal _signal;	// packet is available to pull()
  WritablePacket *_rq;		// queue to receive pulled packets
  int _backoff;			// backoff timer for when sendto() blocks
  Packet *_wq;			// queue to store pulled packet for when sendto() blocks
  int _events;			// keeps track of the events for which select() is waiting

  int initialize_socket_error(ErrorHandler *, const char *);

};

CLICK_ENDDECLS
#endif
