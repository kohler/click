// -*- c-basic-offset: 4 -*-
#ifndef CLICK_KERNELTAP_HH
#define CLICK_KERNELTAP_HH
#include <click/element.hh>
#include <click/ipaddress.hh>
#include <click/etheraddress.hh>
#include <click/task.hh>
CLICK_DECLS

/*
=c

KernelTap(ADDR/MASK [, GATEWAY, HEADROOM] [, I<KEYWORDS>])

=s devices

user-level interface to /dev/tap or ethertap

=d

Reads packets from and writes packets to a /dev/tun* or /dev/tap*
device.  This allows a user-level Click to hand packets to the virtual
ethernet device. KernelTap will also transfer packets from the virtual
ethernet device.

KernelTap allocates a /dev/tun* or tap* device (this might fail) and
runs ifconfig(8) to set the interface's local (i.e., kernel) address
to ADDR and the netmask to MASK. If a nonzero GATEWAY IP address
(which must be on the same network as the tun) is specified, then
KernelTap tries to set up a default route through that host.
HEADROOM is the number of bytes left empty before the packet data 
(to leave room for additional encapsulation headers). Default HEADROOM is 0.

Keyword arguments are:

=over 8

=item IGNORE_QUEUE_OVERFLOWS

Boolean.  If true, don't print more than one error message when
there are queue overflows error when sending/receiving packets
to/from the tun device (e.g. there was an ENOBUFS error).  Default is false.

=back

=item ETHER

Ethernet address. Specifies the fake device's Ethernet address. Default is
00:01:02:03:04:05.

=back

=n

Linux will send ARP queries to the fake device. You must respond to these
queries in order to receive any IP packets, but you can obviously respond
with any Ethernet address you'd like. Here is one common idiom:
 
tap0 :: KernelTap(192.0.0.1/8)
-> fromhost_cl :: Classifier(12/0806, 12/0800);
fromhost_cl[0] -> ARPResponder(0.0.0.0/0 1:1:1:1:1:1) -> tap0;
fromhost_cl[1] -> ... // IP packets
 
 =e
 
 KernelTap(192.0.0.1/8) -> ...;
=n

An error like "could not allocate a /dev/tap* device : No such file or
directory" usually means that you have not enabled /dev/tap* in your
kernel. 

=a ToLinux, ifconfig(8) */

class KernelTap : public Element { public:
  
    KernelTap();
    ~KernelTap();
  
    const char *class_name() const	{ return "KernelTap"; }
    const char *processing() const	{ return "a/h"; }
    const char *flow_code() const	{ return "x/y"; }
    const char *flags() const		{ return "S3"; }
  
    int configure(Vector<String> &, ErrorHandler *);
    int initialize(ErrorHandler *);
    void cleanup(CleanupStage);
    void add_handlers();

    void selected(int fd);

    void push(int port, Packet *);
    bool run_task();

  private:

    enum { DEFAULT_MTU = 2048 };
    enum Type { LINUX_UNIVERSAL, LINUX_ETHERTAP, BSD_TUN, OSX_TUN };

    int _fd;
    int _mtu_in;
    int _mtu_out;
    Type _type;
    String _dev_name;
    IPAddress _near;
    IPAddress _mask;
    IPAddress _gw;
    int _headroom;
    Task _task;


    EtherAddress _macaddr;

    bool _ignore_q_errs;
    bool _printed_write_err;
    bool _printed_read_err;

    static String print_dev_name(Element *e, void *);

#if HAVE_LINUX_IF_TUN_H
    int try_linux_universal(ErrorHandler *);
#endif
    int try_tun(const String &, ErrorHandler *);
    int alloc_tun(ErrorHandler *);
    int setup_tun(struct in_addr near, struct in_addr mask, ErrorHandler *);
    void dealloc_tun();
    
};

CLICK_ENDDECLS
#endif
