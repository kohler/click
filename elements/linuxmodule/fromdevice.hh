#ifndef FROMDEVICE_HH
#define FROMDEVICE_HH

extern "C" {
#include <linux/netdevice.h>
}

/*
 * =c
 * FromDevice(devname)
 * =d
 * Intercepts all packets received by the Linux network interface
 * named devname and pushes them out output 0.
 * The packets include the link-level header.
 *
 * Linux won't see any packets from the device.
 * If you want Linux to process packets, you should
 * hand them to ToLinux.
 *
 * This element is only available inside the kernel module.
 *
 * =a ToDevice
 * =a ToLinux
 */

#include "element.hh"
#include "string.hh"
#include "glue.hh"

#define FROMDEV_QSIZE 512
class FromDevice : public Element {
  
 public:
  
  FromDevice();
  FromDevice(const String &);
  ~FromDevice();
  
  static void static_initialize();
  static void static_cleanup();
  
  const char *class_name() const		{ return "FromDevice"; }
  const char *processing() const		{ return PUSH; }
  
  FromDevice *clone() const;
  int configure(const String &, ErrorHandler *);
  int initialize(ErrorHandler *);
  void uninitialize();
  
  /* process a packet. return 0 if not wanted after all. */
  int got_skb(struct sk_buff *);
  
  void run_scheduled();
  int ifnum() 				{return _dev!=0 ? _dev->ifindex : -1;}
  
 private:
  
  String _devname;
  struct device *_dev;
  unsigned _registered;
  unsigned _drops;
  unsigned _puller_ptr;
  unsigned _pusher_ptr;

  Packet* _queue[FROMDEV_QSIZE];
  unsigned next_i(int i) const	{ return (i!=(FROMDEV_QSIZE-1) ? i+1 : 0); } 
};

#endif
