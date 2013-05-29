// -*- mode: c++; c-basic-offset: 4 -*-
#ifndef CLICK_FROMHOST_HH
#define CLICK_FROMHOST_HH
#include <click/element.hh>
#include <click/etheraddress.hh>
#include <click/timer.hh>
#include <click/notifier.hh>

/*
=c

FromHost(DEVNAME [, PREFIX, I<KEYWORDS>])

=s comm

reads packets from Linux

=d

Captures packets orginating from the Linux kernel and pushes them on output 0.
The type of packet depends on the TYPE keyword argument.  For TYPE ETHER,
output packets have Ethernet headers; only the protocol field is interesting.
For TYPE IP, output packets are IP packets.  TYPE ETHER is the default,
although TYPE IP is probably more useful.

Installs a fake interface called DEVNAME.  If PREFIX is given, changes the
routing table so that every packet destined for PREFIX = ADDR/MASK is sent
through that interface.  The packet then leaves on output 0. The device's
native address is ADDR.

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

=item PREFIX

Specifies the fake device's IP address and netmask.

=item ETHER

Ethernet address. Specifies the fake device's Ethernet address. Default is
00-01-02-03-04-05.

=item MTU

Integer.  The maximum transmission unit reported to Linux for the device.
Defaults to 1500.

=item CAPACITY

Unsigned.  Maximum length of the internal queue that collects packets from
Linux.  Defaults to 100.  If this queue overflows, packets will be silently
dropped.

=item CLEAR_ANNO

Boolean.  Sets whether or not to clear the user annotation area on packets
received from Linux.  This consists of the user annotations.
If false, this area is left as it was received from Linux.
(Note that the packet type, device, and header annotations are left as is.)
Defaults to true.

=item BURST

Integer.  Sets the BURST parameter affecting how many packets are
emitted per task execution.  BURST is 8 by default.

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

=h length r

Return the current length of the internal queue.

=h capacity r

Return the maximum length of the internal queue (the CAPACITY argument).

=h drops r

Return the number of packets dropped off the internal queue so far.

=a ToHost, FromDevice, PollDevice, ToDevice */

#include <click/cxxprotect.h>
CLICK_CXX_PROTECT
#include <linux/netdevice.h>
#include <linux/route.h>
CLICK_CXX_UNPROTECT
#include <click/cxxunprotect.h>
#include <click/standard/storage.hh>

#include "elements/linuxmodule/anydevice.hh"
class EtherAddress;

#if LINUX_VERSION_CODE < KERNEL_VERSION(2, 6, 32)
typedef int netdev_tx_t;
#endif

class FromHost : public AnyDevice, public Storage { public:

    FromHost() CLICK_COLD;
    ~FromHost() CLICK_COLD;

    static void static_initialize();

    const char *class_name() const	{ return "FromHost"; }
    const char *port_count() const	{ return "0/1-2"; }
    const char *processing() const	{ return PUSH; }
    void *cast(const char *name);

    int configure_phase() const		{ return CONFIGURE_PHASE_FROMHOST; }
    int configure(Vector<String> &, ErrorHandler *) CLICK_COLD;
    int initialize(ErrorHandler *) CLICK_COLD;
    void add_handlers() CLICK_COLD;
    void cleanup(CleanupStage) CLICK_COLD;

    int set_device_addresses(ErrorHandler *);

    bool run_task(Task *);

  private:

    EtherAddress _macaddr;
    IPAddress _destaddr;
    IPAddress _destmask;
#if LINUX_VERSION_CODE < KERNEL_VERSION(2, 6, 32)
    net_device_stats _stats;
#endif

    int _burst;

    Task _task;
    Timer _wakeup_timer;

    enum { smq_size = 4 };
    union {
	Packet * volatile smq[smq_size + 1];
	Packet * volatile *lgq;
    } _q;
    NotifierSignal _nonfull_signal;

    int _mtu;
    unsigned _drops;
    unsigned _ninvalid;

    static FromHost *configuring;
    net_device *new_device(const char *);
    static netdev_tx_t fl_tx(struct sk_buff *, net_device *);
    inline Packet * volatile *queue() {
	return _capacity <= smq_size ? _q.smq : _q.lgq;
    }

#if LINUX_VERSION_CODE >= KERNEL_VERSION(2, 6, 32)
    net_device_stats *stats()	{ return &_dev->stats; }
#else
    net_device_stats *stats()	{ return &_stats; }
    static net_device_stats *fl_stats(net_device *dev);
#endif

    enum { h_length, h_burst, h_ether };
    static String read_handler(Element *e, void *thunk) CLICK_COLD;
    static int write_handler(const String &, Element *, void *, ErrorHandler *) CLICK_COLD;
};

#endif
