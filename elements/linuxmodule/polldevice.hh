#ifndef POLLDEVICE_HH
#define POLLDEVICE_HH

/*
 * =c
 * PollDevice(DEVNAME [, PROMISC])
 * =s devices
 * polls packets from network device (kernel)
 * =d
 * Poll packets received by the Linux network interface named DEVNAME.
 * Packets will be pushed to output 0. The packets include the link-level
 * header.
 *
 * If PROMISC is set (by default, it is not), then the device is put into
 * promiscuous mode.
 *
 * Linux won't see any packets from the device. If you want Linux to process
 * packets, you should hand them to ToLinux. Also, if you would like to send
 * packets while using PollDevice, you should also define a ToDevice on the
 * same device.
 *
 * This element can only be used with devices that support the Click polling
 * extension. We have written polling patches for the Tulip Ethernet driver.
 *
 * This element is only available in the Linux kernel module.
 *
 * =h packets read-only
 * Returns the number of packets ToDevice has pulled.
 *
 * =h reset_counts write-only
 * Resets C<packets> counter to zero when written.
 *
 * =a FromDevice, ToDevice, FromLinux, ToLinux */

#include "elements/linuxmodule/anydevice.hh"

class PollDevice : public AnyDevice {
 public:
  
  PollDevice();
  ~PollDevice();
  
  const char *class_name() const		{ return "PollDevice"; }
  const char *processing() const		{ return PUSH; }
  PollDevice *clone() const			{ return new PollDevice; }
  
  int configure(const Vector<String> &, ErrorHandler *);
  int initialize(ErrorHandler *);
  void uninitialize();
  void add_handlers();
  
  /* process a packet. return 0 if not wanted after all. */
  int got_skb(struct sk_buff *);

  void run_scheduled();

  void reset_counts();
  
  unsigned long _npackets;
#if CLICK_DEVICE_STATS
  unsigned long long _time_poll;
  unsigned long long _time_refill;
  unsigned long long _perfcnt1_poll;
  unsigned long long _perfcnt1_refill;
  unsigned long long _perfcnt1_pushing;
  unsigned long long _perfcnt2_poll;
  unsigned long long _perfcnt2_refill;
  unsigned long long _perfcnt2_pushing;
  unsigned long _activations;
#endif
#if CLICK_DEVICE_THESIS_STATS || CLICK_DEVICE_STATS
  unsigned long long _push_cycles;
#endif
    
 private:

  bool _registered;
  bool _promisc;

};

#endif 

