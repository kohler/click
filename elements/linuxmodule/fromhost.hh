// -*- mode: c++; c-basic-offset: 4 -*-
#ifndef CLICK_FROMHOST_HH
#define CLICK_FROMHOST_HH
#include <click/element.hh>
#include <click/etheraddress.hh>
#include <click/timer.hh>
#include <click/notifier.hh>

/*
=c

FromHost(DEVNAME, PREFIX [, I<KEYWORDS>])

=s comm

reads packets from Linux

=d

Captures packets orginating from the Linux kernel and pushes them on output 0.
The type of packet depends on the TYPE keyword argument.  For TYPE ETHER,
output packets have Ethernet headers; only the protocol field is interesting.
For TYPE IP, output packets are IP packets.  TYPE ETHER is the default,
although TYPE IP is probably more useful.

Installs a fake interface called DEVNAME, and changes the routing table so
that every packet destined for PREFIX = ADDR/MASK is sent through that
interface.  The packet then leaves on output 0. The device's native address is
ADDR.

After the fake device is created, the effect of bringing up the interface
and changing the routing table is analogous to:

  % /sbin/ifconfig DEVNAME up
  % /sbin/route add -net ADDR netmask MASK DEVNAME

This element is only available in the Linux kernel module.

Keyword arguments are:

=over 8

=item TYPE

Specifies the device type.  Valid options are C<ETHER> and C<IP>.  Currently
defaults to C<ETHER> with a warning.

=item ETHER

Ethernet address. Specifies the fake device's Ethernet address. Default is
00-01-02-03-04-05.

=back

=n

If TYPE is IP, FromHost will set the packet's IP header and destination IP
address annotations.  Packets with bad IP version or header length are dropped
or emitted on output 1 if it exists.  Note that FromHost doesn't check IP
checksums or full packet lengths.

If TYPE is ETHER, Linux will send ARP queries to the fake device. You must
respond to these queries in order to receive any IP packets, but you can
obviously respond with any Ethernet address you'd like. Here is one common
idiom:

  FromHost(fake0, 192.0.0.1/8, TYPE ETHER)
    -> fromhost_cl :: Classifier(12/0806, 12/0800);
  fromhost_cl[0] -> ARPResponder(0.0.0.0/0 1-1-1-1-1-1) -> ToHost;
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

    FromHost();
    ~FromHost();

    static void static_initialize();
    
    const char *class_name() const	{ return "FromHost"; }
    const char *port_count() const	{ return "0/1-2"; }
    const char *processing() const	{ return PUSH; }

    net_device_stats *stats()		{ return &_stats; }

    int configure_phase() const		{ return CONFIGURE_PHASE_FROMHOST; }
    int configure(Vector<String> &, ErrorHandler *);
    int initialize(ErrorHandler *);
    void cleanup(CleanupStage);

    int set_device_addresses(ErrorHandler *);

    bool run_task(Task *);
    
  private:

    EtherAddress _macaddr;
    IPAddress _destaddr;
    IPAddress _destmask;
    net_device_stats _stats;

    Task _task;
    Timer _wakeup_timer;

    Packet *_queue;		// to prevent race conditions
    NotifierSignal _nonfull_signal;

    net_device *new_device(const char *);
    static int fl_tx(struct sk_buff *, net_device *);
    
};

#endif
