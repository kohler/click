#ifndef CLICK_TODEVICE_HH
#define CLICK_TODEVICE_HH

/*
=c

ToDevice(DEVNAME [, BURST, I<KEYWORDS>])

=s devices

sends packets to network device (kernel)

=d

This manual page describes the Linux kernel module version of the ToDevice
element. For the user-level element, read the ToDevice.u manual page.

Pulls packets from its single input and sends them out the Linux network
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

The Linux networking code may also send packets out the device. If the device
is in polling mode, Click will try to ensure that Linux eventually sends its
packets. Linux may cause the device to be busy when a ToDevice wants to send a
packet. Click is not clever enough to re-queue such packets, and discards
them.

In Linux 2.2, whether or not the device is running in polling mode, ToDevice
depends on the device driver's send operation for synchronization (e.g. tulip
send operation uses a bit lock). In Linux 2.4, we use the device's "xmit_lock"
to synchronize.

Packets sent via ToDevice will not be received by any packet sniffers on the
machine. Use Tee and ToHostSniffers to send packets to sniffers explicitly.

=h packets read-only

Returns the number of packets ToDevice has pulled.

=h reset_counts write-only

Resets C<packets> counter to zero when written.

=a FromDevice, PollDevice, FromHost, ToHost, ToDevice.u, Tee,
ToHostSniffers

*/

#include "elements/linuxmodule/anydevice.hh"
#include <click/notifier.hh>

class ToDevice : public AnyTaskDevice { public:
  
  ToDevice();
  ~ToDevice();
  
  const char *class_name() const	{ return "ToDevice"; }
  const char *processing() const	{ return PULL; }
  ToDevice *clone() const		{ return new ToDevice; }
  
  int configure_phase() const		{ return CONFIGURE_PHASE_TODEVICE; }
  int configure(Vector<String> &, ErrorHandler *);
  int initialize(ErrorHandler *);
  void cleanup(CleanupStage);
  void add_handlers();
  
  bool run_task();

  void reset_counts();
  void change_device(net_device *);
  bool tx_intr();

#if CLICK_DEVICE_STATS
  // Statistics.
  uint64_t _time_clean;
  uint64_t _time_freeskb;
  uint64_t _time_queue;
  uint64_t _perfcnt1_pull;
  uint64_t _perfcnt1_clean;
  uint64_t _perfcnt1_freeskb;
  uint64_t _perfcnt1_queue;
  uint64_t _perfcnt2_pull;
  uint64_t _perfcnt2_clean;
  uint64_t _perfcnt2_freeskb;
  uint64_t _perfcnt2_queue;
  uint32_t _activations; 
#endif
  uint32_t _npackets;
#if CLICK_DEVICE_THESIS_STATS || CLICK_DEVICE_STATS
  uint64_t _pull_cycles;
#endif
  uint32_t _rejected;
  uint32_t _hard_start;
  uint32_t _busy_returns;

#if HAVE_LINUX_POLLING
  bool polling() const			{ return _dev && _dev->polling > 0; }
#else
  bool polling() const			{ return false; }
#endif
  
 private:

  unsigned _burst;
  int _dev_idle;
  NotifierSignal _signal;
  
  int queue_packet(Packet *p);
  
};

#endif
