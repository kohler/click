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
#include <asm/types.h>
#include <asm/uaccess.h>
#include <linux/in.h>
#include <linux/if.h>
#include <linux/if_ether.h>
#include <linux/netdevice.h>
#include <linux/route.h>
#include <string.h>
#include <errno.h>
}

class FromLinux : public Element {
 public:
  
  FromLinux();
  ~FromLinux();
  FromLinux *clone() const;
  
  const char *class_name() const	{ return "FromLinux"; }
  const char *processing() const	{ return PUSH; }
  
  int configure(const String &, ErrorHandler *);
  int initialize(ErrorHandler *);
  void uninitialize();

private:

  String _devname;
  IPAddress _destaddr;
  IPAddress _destmask;

  struct device *_dev;
  struct rtentry *_rt;

  int init_rt(void);
  int init_dev(void);
};

struct fl_priv {
  struct enet_statistics stats;
  FromLinux *fl;
};

#endif FROMLINUX_HH
