#ifndef FROMLINUX_HH
#define FROMLINUX_HH
#include "element.hh"

/*
 * =c
 * FromLinux(DEVNAME, ADDR/MASK)
 * =d
 * Captures packets orginating from the Linux kernel and pushes
 * them on output 0.
 *
 * Installs a fake interface called DEVNAME, and changes the routing
 * table so that every packet destined for ADDR/MASK is sent
 * through that interface.  The packet then leaves on output 0.
 *
 * After the fake device is created, the effect of bringing up the interface
 * and changing the routing table is analogous to:
 *
 * = % /sbin/ifconfig DEVNAME up
 * = % /sbin/route add -net ADDR netmask MASK DEVNAME
 *
 * =e
 * = FromLinux(fake0, 192.0.0.0/8) -> ...;
 *
 * =a ToLinux
 * =a FromDevice
 * =a PollDevice
 * =a ToDevice
 */

extern "C" {
#include <linux/netdevice.h>
#include <linux/route.h>
}

#include "anydevice.hh"

class FromLinux : public AnyDevice {

  IPAddress _destaddr;
  IPAddress _destmask;

  struct enet_statistics _stats;
  struct rtentry *_rt;

  int init_rt();
  int init_dev();

 public:
  
  FromLinux();
  ~FromLinux();
  FromLinux *clone() const;

  static void static_initialize();
  static void static_cleanup();
  
  const char *class_name() const	{ return "FromLinux"; }
  const char *processing() const	{ return PUSH; }

  struct enet_statistics *stats() const	{ return &_stats; }
  
  int configure(const Vector<String> &, ErrorHandler *);
  int initialize(ErrorHandler *);
  void uninitialize();

};

#endif FROMLINUX_HH
