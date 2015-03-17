// -*- mode: c++; c-basic-offset: 2 -*-
#ifndef CLICK_MCASTSOCKET_HH
#define CLICK_MCASTSOCKET_HH
#include <click/element.hh>
#include <click/string.hh>
#include <click/task.hh>
#include <click/notifier.hh>
#include <sys/un.h>
CLICK_DECLS

/*
=c

McastSocket(MCASTIP, MCASTPORT [, SOURCEIP] [, SOURCEPORT] [, I<KEYWORDS>])

=s comm

a multicast UDP socket transport (user-level)

=d

Transports packets over multicast UDP. Packets do not flow through
McastSocket elements (i.e., McastSocket is an "x/y" element). Instead,
input packets are sent via multicast to any number of remote hosts or
processes, and packets from the remote hosts or processes are emitted
on the output.

If "SOURCEIP" (and optionally "SOURCEPORT") is specified, the address
is used as the source of outgoing packets, and the multicast traffic is
only sent and received on the interface designated by SOURCEIP.

If "SOURCEIP" is not specified, the system routing table determines the
source address and interface for sending and receiving the multicast
traffic.

Note that since the McastSocket listens on the same multicast address
as it sends to, it will by default receive copies of its own packets.
If a source IP is specified, McastSocket will automatically drop these
looping packets. If a source IP is not specified, disabling the LOOP
option can be an alternative, if no other subscribing processes run on
the same host as the Click process.

McastSocket inputs are agnostic, i.e., they may be either "pull" or
"push". If pushed, packets will block on the underlying socket;
otherwise, the socket will pull packets as it can accept them. For
best performance, place a Notifier element (such as NotifierQueue)
upstream of a "pull" McastSocket.

Keyword arguments are:

=over 8

=item LOOP

Boolean. Whether processes on the local machine (including this one!)
should receive copies of the outgoing traffic (IP_MULTICAST_LOOP).
The default is true.

=item SNAPLEN

Unsigned integer. Maximum length of packets that can be
received. Default is 2048 bytes.

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

=item HEADROOM

Integer. Per-packet headroom. Defaults to 28.

=back

=e

Start the following Click router:

  link :: McastSocket(239.0.0.0, 1234);

  arpR :: ARPResponder(10.0.0.1/32 02:02:02:02:02:02) -> link;
  arpQ :: ARPQuerier(10.0.0.1, 02:02:02:02:02:02) -> link;

  ip :: Strip(14)
  -> CheckIPHeader()
  -> IPClassifier(icmp and dst host 10.0.0.1)
  -> CheckICMPHeader()
  -> ICMPPingResponder()
  -> arpQ;

  // Note DROP_OWN to prevent multicast loop from messing things up.
  link
  -> HostEtherFilter(02:02:02:02:02:02, DROP_OWN true, DROP_OTHER true)
  -> Classifier(
      12/0806 20/0001, // ARP query
      12/0806 20/0002, // ARP reply
      12/0800, // IP
  ) => arpR, [1]arpQ, ip;

Then grab any Linux live CD image and start a QEMU virtual machine with
the following options:

  qemu -net nic -net socket,mcast=239.0.0.0:1234 -cdrom livecd.iso

After adding an IP address, you will be able to ping 10.0.0.1 from the
virtual machine. The use of multicast transparently bridges any number
of Click and QEMU processes on any number of hosts on a LAN.

=a Socket
*/


class McastSocket : public Element {
public:
    McastSocket() CLICK_COLD;
    ~McastSocket() CLICK_COLD;

    const char *class_name() const { return "McastSocket"; }
    const char *port_count() const { return "0-1/0-1"; }
    const char *processing() const { return "a/h"; }
    const char *flow_code()  const { return "x/y"; }
    const char *flags()      const { return "S3"; }

    virtual int configure(Vector<String> &conf, ErrorHandler *) CLICK_COLD;
    virtual int initialize(ErrorHandler *) CLICK_COLD;
    virtual void cleanup(CleanupStage) CLICK_COLD;

    void add_handlers() CLICK_COLD;
    bool run_task(Task *);
    void push(int port, Packet*);

private:
    Task _task;
    int _recv_sock;
    int _send_sock;

    NotifierSignal _signal; // packet is available to pull()
    WritablePacket *_rq;    // queue to receive pulled packets
    Packet *_wq;            // queue to store pulled packet for when sendto() blocks

    bool _loop;         // IP_MULTICAST_LOOP socket option
    bool _timestamp;    // set the timestamp on received packets
    int _rcvbuf;        // maximum socket receive buffer in bytes
    int _snaplen;       // maximum received packet length
    int _sndbuf;        // maximum socket send buffer in bytes
    unsigned _headroom;

    struct sockaddr_in _mcast;
    struct sockaddr_in _source;

    void cleanup();
    void selected(int fd, int mask);
    int write_packet(Packet*);
    int initialize_socket_error(ErrorHandler *, const char *);
};


CLICK_ENDDECLS
#endif
