// -*- mode: c++; c-basic-offset: 4 -*-
#ifndef CLICK_FROMHOST_HH
#define CLICK_FROMHOST_HH
#include <click/element.hh>
#include <click/etheraddress.hh>
#include <click/timer.hh>

/*
=c

FromHost(DEVNAME, ADDR/MASK [, I<KEYWORDS>])

=s sources

reads packets from Linux

=d

Captures packets orginating from the Linux kernel and pushes them on output
0. Output packets have Ethernet headers; only the protocol field is
interesting.

Installs a fake interface called DEVNAME, and changes the routing table so
that every packet destined for ADDR/MASK is sent through that interface.
The packet then leaves on output 0. The device's native address is ADDR.

After the fake device is created, the effect of bringing up the interface
and changing the routing table is analogous to:

  % /sbin/ifconfig DEVNAME up
  % /sbin/route add -net ADDR netmask MASK DEVNAME

This element is only available in the Linux kernel module.

Keyword arguments are:

=over 8

=item ETHER

Ethernet address. Specifies the fake device's Ethernet address. Default is
00:01:02:03:04:05.

=back

=n

Linux will send ARP queries to the fake device. You must respond to these
queries in order to receive any IP packets, but you can obviously respond
with any Ethernet address you'd like. Here is one common idiom:

  FromHost(fake0, 192.0.0.1/8)
    -> fromhost_cl :: Classifier(12/0806, 12/0800);
  fromhost_cl[0] -> ARPResponder(0.0.0.0/0 1:1:1:1:1:1) -> ToLinux;
  fromhost_cl[1] -> ... // IP packets

=e

  FromHost(fake0, 192.0.0.1/8) -> ...;

=a ToHost, FromDevice, PollDevice, ToDevice */

#include <click/cxxprotect.h>
CLICK_CXX_PROTECT
#include <linux/netdevice.h>
#include <linux/route.h>
CLICK_CXX_UNPROTECT
#include <click/cxxunprotect.h>

#include "elements/linuxmodule/anydevice.hh"
class EtherAddress;

class FromHost : public AnyDevice { public:

    enum { CONFIGURE_PHASE_FROMHOST = CONFIGURE_PHASE_DEFAULT,
	   CONFIGURE_PHASE_TODEVICE = CONFIGURE_PHASE_FROMHOST + 1 };

    FromHost();
    ~FromHost();

    const char *class_name() const	{ return "FromHost"; }
    FromHost *clone() const;
    const char *processing() const	{ return PUSH; }

    net_device_stats *stats()		{ return &_stats; }

    int configure_phase() const		{ return CONFIGURE_PHASE_FROMHOST; }
    int configure(Vector<String> &, ErrorHandler *);
    int initialize(ErrorHandler *);
    void cleanup(CleanupStage);

    int set_device_addresses(ErrorHandler *);
    
  private:

    EtherAddress _macaddr;
    IPAddress _destaddr;
    IPAddress _destmask;
    net_device_stats _stats;

    Timer _wakeup_timer;

};

#endif
