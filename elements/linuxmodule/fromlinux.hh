#ifndef FROMLINUX_HH
#define FROMLINUX_HH
#include "element.hh"

/*
 * =c
 * FromLinux(device-name, dest-addr, dest-mask)
 * =d
 * Captures packets orginating from the Linux kernel and pushes
 * them on output 0.
 *
 * Installs a fake interface called "device-name", and changes the routing
 * table so that every packet destined for "dest-addr/dest-mask" is sent
 * through that interface.  The packet then leaves on output 0.
 *
 * After the fake device is created, the effect of bringing up the interface
 * and changing the routing table is analogous to:
 *
 * = % /sbin/ifconfig <device-name> up
 * = % /sbin/route add -net <dest-addr> netmask <dest-mask> <device-name>
 *
 * =a ToLinux
 * =a FromDevice
 * =a ToDevice
 */

extern "C" {
#include <linux/netdevice.h>
#include <linux/route.h>
}

class FromLinux : public Element {

  String _devname;
  IPAddress _destaddr;
  IPAddress _destmask;

  struct device *_dev;
  struct rtentry *_rt;

  struct enet_statistics _stats;
  
  int init_rt();
  int init_dev();

 public:
  
  FromLinux();
  ~FromLinux();
  FromLinux *clone() const;
  
  const char *class_name() const	{ return "FromLinux"; }
  const char *processing() const	{ return PUSH; }

  struct enet_statistics *stats() const	{ return &_stats; }
  
  int configure(const String &, ErrorHandler *);
  int initialize(ErrorHandler *);
  void uninitialize();

};

#endif FROMLINUX_HH
