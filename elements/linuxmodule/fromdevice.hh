#ifndef FROMDEVICE_HH
#define FROMDEVICE_HH

/*
 * =c
 * FromDevice(DEVNAME [, PROMISC, BURST])
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
 * The packets include the link-level header. Each time FromDevice is called,
 * at most BURST number of packets are sent. BURST is 8 by default.
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
#include "elements/standard/queue.hh"

class FromDevice : public AnyDevice, public Storage { public:
  
  FromDevice();
  ~FromDevice();

  const char *class_name() const	{ return "FromDevice"; }
  const char *processing() const	{ return PUSH; }
  FromDevice *clone() const		{ return new FromDevice; }
  void *cast(const char *);

  int configure(const Vector<String> &, ErrorHandler *);
  int initialize(ErrorHandler *);
  void uninitialize();
  void add_handlers();
  void take_state(Element *, ErrorHandler *);
  
  /* process a packet. return 0 if not wanted after all. */
  int got_skb(struct sk_buff *);
  
  void run_scheduled();

 private:

  bool _registered;
  bool _promisc;
  unsigned _burst;
  unsigned _drops;

  static const int QSIZE = 511;
  Packet *_queue[QSIZE+1];
 
};

#endif
