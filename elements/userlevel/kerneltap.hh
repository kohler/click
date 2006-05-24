// -*- c-basic-offset: 4 -*-
#ifndef CLICK_KERNELTAP_HH
#define CLICK_KERNELTAP_HH
#include "kerneltun.hh"
CLICK_DECLS

/*
=c

KernelTap(ADDR/MASK [, GATEWAY, I<keywords> ETHER, MTU, HEADROOM, IGNORE_QUEUE_OVERFLOWS])

=s comm

interface to /dev/tap or ethertap (user-level)

=d

Reads Ethernet packets from and writes Ethernet packets to a /dev/tun* or
/dev/tap* device.  This allows a user-level Click to hand packets to the
virtual Ethernet device.  KernelTap will also transfer packets from the
virtual Ethernet device.

KernelTap allocates a /dev/tun* or tap* device (this might fail) and runs
ifconfig(8) to set the interface's local (i.e., kernel) address to ADDR and
the netmask to MASK.  If a nonzero GATEWAY IP address (which must be on the
same network as the tun) is specified, then KernelTap tries to set up a
default route through that host.

Keyword arguments are:

=over 8

=item ETHER

Ethernet address.  Specifies the fake device's Ethernet address. Default is
00:01:02:03:04:05.

=item MTU

Integer.  The interface's maximum transmission unit.  Default is 2048.

=item HEADROOM

Integer.  The number of bytes left empty before the packet data (to leave room
for additional encapsulation headers).  Default is 0.

=item IGNORE_QUEUE_OVERFLOWS

Boolean.  If true, don't print more than one error message when there are
queue overflow errors (ENOBUFS) when sending or receiving packets via the tun
device.  Default is false.

=back

=n

Linux will send ARP queries to the fake device. You must respond to these
queries in order to receive any IP packets, but you can obviously respond
with any Ethernet address you'd like. Here is one common idiom:
 
  tap0 :: KernelTap(192.0.0.1/8)
       -> fromhost_cl :: Classifier(12/0806, 12/0800);
  fromhost_cl[0] -> ARPResponder(0.0.0.0/0 1:1:1:1:1:1) -> tap0;
  fromhost_cl[1] -> ... // IP packets
 
An error like "could not allocate a /dev/tap* device : No such file or
directory" usually means that you have not enabled /dev/tap* in your
kernel. 

=a ToHost, ifconfig(8) */

class KernelTap : public KernelTun { public:
  
    KernelTap();
  
    const char *class_name() const	{ return "KernelTap"; }
    
};

CLICK_ENDDECLS
#endif
