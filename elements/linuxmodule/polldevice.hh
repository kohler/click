#ifndef CLICK_POLLDEVICE_HH
#define CLICK_POLLDEVICE_HH

/*
=c

PollDevice(DEVNAME [, PROMISC, BURST, I<KEYWORDS>])

=s devices

polls packets from network device (kernel)

=d

Poll packets received by the Linux network interface named DEVNAME. Packets
will be pushed to output 0. The packets include the link-level header. DEVNAME
may also be an Ethernet address, in which case PollDevice searches for a
device with that address.

Each time PollDevice is scheduled, it emits at most BURST packets. By default,
BURST is 8.

If PROMISC is set (by default, it is not), then the device is put into
promiscuous mode.

This element is only available in the Linux kernel module.

Keyword arguments are:

=over 8

=item PROMISC

Boolean. Same as the PROMISC argument.

=item BURST

Unsigned integer. Same as the BURST argument.

=item ALLOW_NONEXISTENT

Allow nonexistent devices. If true, and no device named DEVNAME exists when
the router is initialized, then PollDevice will report a warning (rather than
an error). Later, while the router is running, if a device named DEVNAME
appears, PollDevice will seamlessly begin emitting its packets. Default is
false.

=back

=n

Linux won't see any packets from the device. If you want Linux to process
packets, you should hand them to ToLinux. Also, if you would like to send
packets while using PollDevice, you should also define a ToDevice on the same
device.

This element can only be used with devices that support the Click polling
extension. We have written polling patches for the Tulip Ethernet driver.

=h packets read-only

Returns the number of packets ToDevice has pulled.

=h reset_counts write-only

Resets C<packets> counter to zero when written.

=a FromDevice, ToDevice, FromLinux, ToLinux */

#include "elements/linuxmodule/anydevice.hh"

class PollDevice : public AnyDevice { public:
  
  PollDevice();
  ~PollDevice();
  
  const char *class_name() const		{ return "PollDevice"; }
  const char *processing() const		{ return PUSH; }
  PollDevice *clone() const			{ return new PollDevice; }
  
  int configure(Vector<String> &, ErrorHandler *);
  int initialize(ErrorHandler *);
  void cleanup(CleanupStage);
  void add_handlers();

  void change_device(net_device *);
  /* process a packet. return 0 if not wanted after all. */
  int got_skb(struct sk_buff *);

  void run_scheduled();

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
  uint64_t _push_cycles;
#endif
    
  uint32_t _buffers_reused;

 private:

  unsigned _burst;

};

#endif 

