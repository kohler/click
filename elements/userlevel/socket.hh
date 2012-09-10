// -*- mode: c++; c-basic-offset: 2 -*-
#ifndef CLICK_SOCKET_HH
#define CLICK_SOCKET_HH
#include <click/element.hh>
#include <click/string.hh>
#include <click/task.hh>
#include <click/timer.hh>
#include <click/notifier.hh>
#include "../ip/iproutetable.hh"
#include <sys/un.h>
CLICK_DECLS

/*
=c

Socket("TCP", IP, PORTNUMBER [, LOCALIP] [, LOCALPORTNUMBER] [, I<KEYWORDS>])
Socket("UDP", IP, PORTNUMBER [, LOCALIP] [, LOCALPORTNUMBER] [, I<KEYWORDS>])
Socket("UNIX", FILENAME [, LOCALFILENAME] [, I<KEYWORDS>])
Socket("UNIX_DGRAM", FILENAME [, LOCALFILENAME] [, I<KEYWORDS>])

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

If "LOCALIP"/"LOCALPORTNUMBER" or "LOCALFILENAME" is specified, CLIENT
is assumed if not set and the specified local address/port/file will
be bound before the connection attempt is made. If CLIENT is set to
false, any "LOCALIP"/"LOCALPORTNUMBER" and "LOCALFILENAME" arguments
are ignored.

Socket inputs are agnostic, i.e., they may be either "pull" or
"push". If pushed, packets will block on the underlying socket;
otherwise, the socket will pull packets as it can accept them. For
best performance, place a Notifier element (such as NotifierQueue)
upstream of a "pull" Socket.

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

=item SNDBUF

Unsigned integer. Sets the maximum size in bytes of the underlying
socket send buffer. The default value is set by the wmem_default
sysctl and the maximum allowed value is set by the wmem_max sysctl.

=item RCVBUF

Unsigned integer. Sets the maximum size in bytes of the underlying
socket receive buffer. The default value is set by the rmem_default
sysctl and the maximum allowed value is set by the rmem_max sysctl.

=item TIMESTAMP

Boolean. If set, sets the timestamp field on received packets to the
current time. Default is true.

=item ALLOW

The name of an IPRouteTable element, like RadixIPLookup or
DirectIPLookup. If set and the Socket element is a server, the Socket
element will lookup source IP addresses of clients in the specified
IPRouteTable before accepting a connection (if SOCK_STREAM) or
datagram (if SOCK_DGRAM). If the address is found, the connection or
datagram is accepted. If the address is not found, the DENY table will
then be checked (see below).

=item DENY

The name of an IPRouteTable element, like RadixIPLookup or
DirectIPLookup. If set and the Socket element is a server, the Socket
element will lookup source IP addresses of clients in the specified
IPRouteTable before accepting a connection (if SOCK_STREAM) or
datagram (if SOCK_DGRAM). If the address is found, the connection or
datagram is dropped, otherwise it is accepted. Note that the ALLOW
table, if specified, is checked first. Wildcard matches may be
specified with netmasks; for example, to deny all hosts, specify a
route to "0.0.0.0/0" in the DENY table.

=item VERBOSE

Boolean. When true, Socket will print messages whenever it accepts a
new connection or drops an old one. Default is false.

=item PROPER

Boolean. PlanetLab specific. If true and Click has been configured
--with-proper, use Proper to bind a reserved port.

=item HEADROOM

Integer. Per-packet headroom. Defaults to 28.

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

  // A bi-directional client socket bound to a particular local port
  ... -> Socket(TCP, 1.2.3.4, 80, 0.0.0.0, 54321) -> ...

  // A localhost server socket
  allow :: RadixIPLookup(127.0.0.1 0);
  deny :: RadixIPLookup(0.0.0.0/0	0);
  allow -> deny -> allow; // (makes the configuration valid)
  Socket(TCP, 0.0.0.0, 80, ALLOW allow, DENY deny) -> ...

=a RawSocket */

class Socket : public Element { public:

  Socket() CLICK_COLD;
  ~Socket() CLICK_COLD;


  const char *class_name() const	{ return "Socket"; }
  const char *port_count() const	{ return "0-1/0-1"; }
  const char *processing() const	{ return "a/h"; }
  const char *flow_code() const		{ return "x/y"; }
  const char *flags() const		{ return "S3"; }

  virtual int configure(Vector<String> &conf, ErrorHandler *) CLICK_COLD;
  virtual int initialize(ErrorHandler *) CLICK_COLD;
  virtual void cleanup(CleanupStage) CLICK_COLD;

  void add_handlers() CLICK_COLD;
  bool run_task(Task *);
  void selected(int fd, int mask);
  void push(int port, Packet*);

  bool allowed(IPAddress);
  void close_active(void);
  int write_packet(Packet*);

protected:
  Task _task;
  Timer _timer;

private:
  int _fd;	// socket descriptor
  int _active;	// connection descriptor

  // local address to bind()
  union { struct sockaddr_in in; struct sockaddr_un un; } _local;
  socklen_t _local_len;

  // remote address to connect() to or sendto() (for
  // non-connection-mode sockets)
  union { struct sockaddr_in in; struct sockaddr_un un; } _remote;
  socklen_t _remote_len;

  NotifierSignal _signal;	// packet is available to pull()
  WritablePacket *_rq;		// queue to receive pulled packets
  int _backoff;			// backoff timer for when sendto() blocks
  Packet *_wq;			// queue to store pulled packet for when sendto() blocks
  int _events;			// keeps track of the events for which select() is waiting

  int _family;			// AF_INET or AF_UNIX
  int _socktype;		// SOCK_STREAM or SOCK_DGRAM
  int _protocol;		// for AF_INET, IPPROTO_TCP, IPPROTO_UDP, etc.
  IPAddress _local_ip;		// for AF_INET, address to bind()
  uint16_t _local_port;		// for AF_INET, port to bind()
  String _local_pathname;	// for AF_UNIX, file to bind()
  IPAddress _remote_ip;		// for AF_INET, address to connect() to or sendto()
  uint16_t _remote_port;	// for AF_INET, port to connect() to or sendto()
  String _remote_pathname;	// for AF_UNIX, file to sendto()

  bool _timestamp;		// set the timestamp on received packets
  int _sndbuf;			// maximum socket send buffer in bytes
  int _rcvbuf;			// maximum socket receive buffer in bytes
  int _snaplen;			// maximum received packet length
  unsigned _headroom;
  int _nodelay;			// disable Nagle algorithm
  bool _verbose;		// be verbose
  bool _client;			// client or server
  bool _proper;			// (PlanetLab only) use Proper to bind port
  IPRouteTable *_allow;		// lookup table of good hosts
  IPRouteTable *_deny;		// lookup table of bad hosts

  int initialize_socket_error(ErrorHandler *, const char *);

};

CLICK_ENDDECLS
#endif
