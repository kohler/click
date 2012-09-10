#ifndef CLICK_TODEVICE_BSDMODULE_HH
#define CLICK_TODEVICE_BSDMODULE_HH

/*
=title ToDevice.b

=c

ToDevice(DEVNAME [, BURST, I<KEYWORDS>])

=s netdevices

sends packets to network device (BSD kernel)

=d

This manual page describes the FreeBSD kernel module version of the ToDevice
element. For the user-level element, read the ToDevice.u manual page.

Pulls packets from its single input and sends them out the FreeBSD network
interface named DEVNAME. DEVNAME may also be an Ethernet address, in which
case ToDevice searches for a device with that address.

Sends up to BURST packets each time it is scheduled. By default, BURST is 16.
For good performance, you should set BURST to be 8 times the number of
elements that could generate packets for this device.

Packets must have a link header. For Ethernet, ToDevice makes sure every
packet is at least 60 bytes long.

Keyword arguments are:

=over 8

=item BURST

Unsigned integer. Same as the BURST argument.

=item ALLOW_NONEXISTENT

Allow nonexistent devices. If true, and no device named DEVNAME exists when
the router is initialized, then ToDevice will report a warning (rather than an
error). Later, while the router is running, if a device named DEVNAME appears,
ToDevice will seamlessly begin sending packets to it. Default is false.

=back

=n

The FreeBSD networking code may also send packets out the device. Click won't
see those packets. Worse, FreeBSD may cause the device to be busy when a
ToDevice wants to send a packet. Click is not clever enough to re-queue
such packets, and discards them.

ToDevice's depend on the net driver's send operation for synchronization

=h packets read-only

Returns the number of packets ToDevice has pulled.

=h reset_counts write-only

Resets C<packets> counter to zero when written.

=a FromDevice, FromHost, ToHost, ToDevice.u */

#include "elements/bsdmodule/anydevice.hh"
#include <click/notifier.hh>
CLICK_DECLS

class ToDevice : public AnyDevice {

 public:

  ToDevice() CLICK_COLD;
  ~ToDevice() CLICK_COLD;

  const char *class_name() const	{ return "ToDevice"; }
  const char *port_count() const	{ return PORTS_1_0; }
  const char *processing() const	{ return PULL; }

  int configure_phase() const		{ return CONFIGURE_PHASE_TODEVICE; }
  int configure(Vector<String> &, ErrorHandler *) CLICK_COLD;
  int initialize(ErrorHandler *) CLICK_COLD;
  void cleanup(CleanupStage) CLICK_COLD;
  void add_handlers() CLICK_COLD;

  bool run_task(Task *);

  void reset_counts();

  unsigned _npackets;
  unsigned long _busy_returns;

 private:

  unsigned _burst;
  NotifierSignal _signal;

};

CLICK_ENDDECLS
#endif
