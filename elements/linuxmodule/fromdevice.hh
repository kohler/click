#ifndef FROMDEVICE_HH
#define FROMDEVICE_HH

/*
 * =c
 * FromDevice(DEVNAME [, PROMISC])
 * =s devices
 * reads packets from network device (kernel)
 * =d
 *
 * This manual page describes the Linux kernel module version of the
 * FromDevice element. For the user-level element, read the FromDevice.u
 * manual page.
 * 
 * Intercepts all packets received by the Linux network interface
 * named DEVNAME and pushes them out output 0.
 * The packets include the link-level header.
 *
 * If PROMISC is set (by default, it is not), then the device is put into
 * promiscuous mode.
 *
 * Linux won't see any packets from the device.
 * If you want Linux to process packets, you should
 * hand them to ToLinux.
 *
 * FromDevice accesses packets the same way Linux does: through interrupts.
 * This is bad for performance. If you care about performance and have a
 * polling-capable device, use PollDevice instead.
 *
 * =a PollDevice, ToDevice, FromLinux, ToLinux, FromDevice.u */

#include "elements/linuxmodule/anydevice.hh"

class FromDevice : public AnyDevice {
  
  bool _registered;
  bool _promisc;
  unsigned _drops;
  unsigned _puller_ptr;
  unsigned _pusher_ptr;

  static const int QSIZE = 512;
  Packet *_queue[QSIZE];
  unsigned next_i(unsigned i) const	{ return (i!=(QSIZE-1) ? i+1 : 0); }
  
 public:
  
  FromDevice();
  ~FromDevice();

  const char *class_name() const	{ return "FromDevice"; }
  const char *processing() const	{ return PUSH; }
  FromDevice *clone() const		{ return new FromDevice; }

  int configure(const Vector<String> &, ErrorHandler *);
  int initialize(ErrorHandler *);
  void uninitialize();
  void take_state(Element *, ErrorHandler *);
  
  /* process a packet. return 0 if not wanted after all. */
  int got_skb(struct sk_buff *);
  
  void run_scheduled();
  
};

#endif
