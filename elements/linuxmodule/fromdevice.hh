#ifndef FROMDEVICE_HH
#define FROMDEVICE_HH

/*
 * =c
 * FromDevice(DEVNAME)
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
 * Linux won't see any packets from the device.
 * If you want Linux to process packets, you should
 * hand them to ToLinux.
 *
 * FromDevice accesses packets the same way Linux does: through interrupts.
 * This is bad for performance. If you care about performance and have a
 * polling-capable device, use PollDevice instead.
 *
 * =a PollDevice
 * =a ToDevice
 * =a FromLinux
 * =a ToLinux
 * =a FromDevice.u */

#include "anydevice.hh"

#define FROMDEV_QSIZE 512
class FromDevice : public AnyDevice {
  
 public:
  
  FromDevice();
  ~FromDevice();
  
  const char *class_name() const		{ return "FromDevice"; }
  const char *processing() const		{ return PUSH; }
  FromDevice *clone() const			{ return new FromDevice; }
  
  int configure(const Vector<String> &, ErrorHandler *);
  int initialize(ErrorHandler *);
  void uninitialize();
  void take_state(Element *, ErrorHandler *);
  
  /* process a packet. return 0 if not wanted after all. */
  int got_skb(struct sk_buff *);
  
  void run_scheduled();
  
 private:
  
  bool _registered;
  unsigned _drops;
  unsigned _puller_ptr;
  unsigned _pusher_ptr;

  Packet* _queue[FROMDEV_QSIZE];
  unsigned next_i(unsigned i) const { return (i!=(FROMDEV_QSIZE-1) ? i+1 : 0); }
  
};

#endif
