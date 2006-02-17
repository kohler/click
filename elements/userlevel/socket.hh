// -*- mode: c++; c-basic-offset: 2 -*-
#ifndef CLICK_SOCKET_HH
#define CLICK_SOCKET_HH
#include <click/element.hh>
#include <click/string.hh>
#include <click/task.hh>
#include <click/timer.hh>
#include <click/notifier.hh>
#include <sys/un.h>
CLICK_DECLS

/*
=c

Socket("TCP", IP, PORTNUMBER [, I<KEYWORDS>])
Socket("UDP", IP, PORTNUMBER [, I<KEYWORDS>])
Socket("UNIX", FILENAME [, I<KEYWORDS>])
Socket("UNIX_DGRAM", FILENAME [, I<KEYWORDS>])

=s comm

a socket transport (user-level)

=d

Transports packets over various types of sockets. Packets do not flow
through Socket elements (i.e., Socket is an "x/y" element). Instead,
input packets are sent to a remote host or process, and packets
received from the remote host or process are emitted on the output.

A Socket element of type "TCP" or "UNIX" may be either a server (the
default if CLIENT is not set) or a client (if CLIENT is set or if the
element has no outputs). If a server, the specified address/port/file
is bound and connections are accepted one at a time. If a client, a
connection attempt is made to the specified address/port/file during
element initialization.

A Socket element of type "UDP" or "UNIX_DGRAM" may also be either a
server or client. However, because datagram sockets are not connection
oriented, a datagram server may receive (and thus emit) packets from
multiple remote hosts or processes. If a server, input packets are
sent to the last remote host or process to send a packet to the
server. If a client, input packets are sent to the specified
address/port/file.

For convenience, if a client UDP Socket is configured with a zero IP
address, the Socket will send input packets to the destination IP
annotation of each packet.

Keyword arguments are:

=over 8

=item SNAPLEN

Unsigned integer. Maximum length of packets that can be
received. Default is 2048 bytes.

=item NODELAY

Boolean. Applies to TCP sockets only. If set, disable the Nagle
algorithm. This means that segments are always sent as soon as
possible, even if there is only a small amount of data. When not set,
data is buffered until there is a sufficient amount to send out,
thereby avoiding the frequent sending of small packets, which results
in poor utilization of the network. Default is true.

=item CLIENT

Boolean. If set, forces the socket to connect() (if SOCK_STREAM) to
the specified address/port (if AF_INET) or file handle (if AF_UNIX),
instead of bind()-ing and listen()-ing to it.

Default is false. However, if a Socket element has no output and
CLIENT is unspecified, it is assumed to be a client socket. If a
Socket element has no input and CLIENT is unspecified, it is assumed
to be a server socket.

=item VERBOSE

Boolean. When true, Socket will print messages whenever it accepts a
new connection or drops an old one. Default is false.

=back

=e

  // A server socket
  Socket(TCP, 0.0.0.0, 80) -> ...

  // A client socket
  ... -> Socket(TCP, 1.2.3.4, 80)

  // A bi-directional server socket (handles one client at a time)
  ... -> Socket(TCP, 0.0.0.0, 80) -> ...

  // A bi-directional client socket
  ... -> Socket(TCP, 1.2.3.4, 80, CLIENT true) -> ...

=a RawSocket */

class Socket : public Element { public:

  Socket();
  ~Socket();


  const char *class_name() const	{ return "Socket"; }
  const char *port_count() const	{ return "0-1/0-1"; }
  const char *processing() const	{ return "l/h"; }
  const char *flow_code() const		{ return "x/y"; }

  int configure(Vector<String> &conf, ErrorHandler *);
  int initialize(ErrorHandler *);
  void cleanup(CleanupStage);
  void add_handlers();

  bool run_task();
  void selected(int);
  void run_timer(Timer *);

protected:
  Task _task;
  Timer _timer;

private:
  int _fd;	// socket descriptor
  int _active;	// connection descriptor

  // bind() parameters during initialization, sendto() address for
  // non-connection-mode sockets
  union { struct sockaddr_in in; struct sockaddr_un un; } _sa;
  socklen_t _sa_len;

  NotifierSignal _signal;	// packet is available to pull()
  WritablePacket *_rq;		// queue to receive pulled packets
  int _backoff;			// backoff timer for when sendto() blocks
  Packet *_wq;			// queue to store pulled packet for when sendto() blocks
  int _events;			// keeps track of the events for which select() is waiting

  int _family;			// AF_INET or AF_UNIX
  int _socktype;		// SOCK_STREAM or SOCK_DGRAM
  int _protocol;		// for AF_INET, IPPROTO_TCP, IPPROTO_UDP, etc.
  IPAddress _ip;		// for AF_INET, address to bind()
  unsigned short _port;		// for AF_INET, port to bind()
  String _pathname;		// for AF_UNIX, file to bind()

  int _snaplen;			// maximum received packet length
  int _nodelay;			// disable Nagle algorithm
  bool _verbose;		// be verbose
  bool _client;			// client or server

  int initialize_socket_error(ErrorHandler *, const char *);

};

CLICK_ENDDECLS
#endif
