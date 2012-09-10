#ifndef CLICK_POLLDEVICE_HH
#define CLICK_POLLDEVICE_HH

/*
=c

PollDevice(DEVNAME [, I<keywords> PROMISC, BURST, TIMESTAMP...])

=s netdevices

polls packets from network device (kernel)

=d

Poll packets received by the Linux network interface named DEVNAME. Packets
will be pushed to output 0. The packets include the link-level header. DEVNAME
may also be an Ethernet address, in which case PollDevice searches for a
device with that address.

Each time PollDevice is scheduled, it emits at most BURST packets. By default,
BURST is 8.

This element is only available in the Linux kernel module.

Keyword arguments are:

=over 8

=item PROMISC

Boolean.  If true, the device is put into promiscuous mode while FromDevice is
active.  Default is false.

=item BURST

Unsigned integer.  Sets the BURST parameter.

=item TIMESTAMP

Boolean.  If true, then ensure that received packets have correctly-set
timestamp annotations.  Default is true.

=item QUIET

Boolean.  If true, then suppress device up/down messages.  Default is false.

=item HEADROOM

Unsigned.  Amount of extra headroom to request on each packet.  Default is 64.

=item LENGTH

Unsigned integer.  Sets the minimum size requested for packet buffers.  Should
be at least as large as your interface's MTU; some cards require even more
data than that.  Defaults to a number derived from the driver, which is
usually the right answer.

=item ALLOW_NONEXISTENT

Allow nonexistent devices. If true, and no device named DEVNAME exists when
the router is initialized, then PollDevice will report a warning (rather than
an error). Later, while the router is running, if a device named DEVNAME
appears, PollDevice will seamlessly begin emitting its packets. Default is
false.

=item UP_CALL

Write handler.  If supplied, this handler is called when the device or link
comes up.

=item DOWN_CALL

Write handler.  If supplied, this handler is called when the device or link
goes down.

=back

=n

Linux won't see any packets from the device. If you want Linux to process
packets, you should hand them to ToHost. Also, if you would like to send
packets while using PollDevice, you should also define a ToDevice on the same
device.

This element can only be used with devices that support the Click polling
extension. We have written polling patches for the Tulip Ethernet driver.

Linux device drivers, and thus FromDevice, should set packets' timestamp,
packet-type, and device annotations.

=h count read-only

Returns the number of packets PollDevice has received from the input card.

=h reset_counts write-only

Resets C<count> counter to zero when written.

=a FromDevice, ToDevice, FromHost, ToHost */

#include "elements/linuxmodule/anydevice.hh"

class PollDevice : public AnyTaskDevice { public:

  PollDevice() CLICK_COLD;
  ~PollDevice() CLICK_COLD;

  static void static_initialize();
  static void static_cleanup();

  const char *class_name() const	{ return "PollDevice"; }
  const char *port_count() const	{ return PORTS_0_1; }
  const char *processing() const	{ return PUSH; }

  int configure_phase() const		{ return CONFIGURE_PHASE_POLLDEVICE; }
  int configure(Vector<String> &, ErrorHandler *) CLICK_COLD;
  int initialize(ErrorHandler *) CLICK_COLD;
  void cleanup(CleanupStage) CLICK_COLD;
  void add_handlers() CLICK_COLD;

  void change_device(net_device *);
  /* process a packet. return 0 if not wanted after all. */
  int got_skb(struct sk_buff *);

  bool run_task(Task *);

  void reset_counts();

  uint32_t _npackets;
#if CLICK_DEVICE_STATS
  uint64_t _time_poll;
  uint64_t _time_allocskb;
  uint64_t _time_refill;

  uint64_t _perfcnt1_poll;
  uint64_t _perfcnt1_refill;
  uint64_t _perfcnt1_allocskb;
  uint64_t _perfcnt1_pushing;
  uint64_t _perfcnt2_poll;
  uint64_t _perfcnt2_refill;
  uint64_t _perfcnt2_allocskb;
  uint64_t _perfcnt2_pushing;

  uint32_t _empty_polls;
  uint32_t _activations;
#endif
#if CLICK_DEVICE_THESIS_STATS || CLICK_DEVICE_STATS
  click_cycles_t _push_cycles;
#endif

  uint32_t _buffers_reused;

 private:

    unsigned _burst;
    unsigned _headroom;
    uint32_t _length;
    bool _user_length;

};

#endif

