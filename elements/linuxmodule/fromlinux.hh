#ifndef FROMLINUX_HH
#define FROMLINUX_HH
#include <click/element.hh>

/*
 * =c
 * FromLinux(DEVNAME, ADDR/MASK)
 * =s
 * reads packets from Linux
 * V<sources>
 * =d
 *
 * Captures packets orginating from the Linux kernel and pushes them on output
 * 0. Output packets have Ethernet headers; only the protocol field is
 * interesting.
 *
 * Installs a fake interface called DEVNAME, and changes the routing table so
 * that every packet destined for ADDR/MASK is sent through that interface.
 * The packet then leaves on output 0. The device's native address is ADDR.
 *
 * After the fake device is created, the effect of bringing up the interface
 * and changing the routing table is analogous to:
 *
 *   % /sbin/ifconfig DEVNAME up
 *   % /sbin/route add -net ADDR netmask MASK DEVNAME
 *
 * This element is only available in the Linux kernel module.
 *
 * =n
 *
 * Linux will send ARP queries to the fake device. You must respond to these
 * queries in order to receive any IP packets, but you can obviously respond
 * with any Ethernet address you'd like. Here is one common idiom:
 *
 *   FromLinux(fake0, 192.0.0.1/8)
 *     -> fromlinux_cl :: Classifier(12/0806, 12/0800);
 *   fromlinux_cl[0] -> ARPResponder(0.0.0.0/0 1:1:1:1:1:1) -> ToLinux;
 *   fromlinux_cl[1] -> ... // IP packets
 *
 * =e
 *   FromLinux(fake0, 192.0.0.1/8) -> ...;
 *
 * =a ToLinux, FromDevice, PollDevice, ToDevice */

extern "C" {
#include <linux/netdevice.h>
#include <linux/route.h>
}

#include "elements/linuxmodule/anydevice.hh"

class FromLinux : public AnyDevice {

  IPAddress _destaddr;
  IPAddress _destmask;

  struct enet_statistics _stats;
  struct rtentry *_rt;

  int init_rt();
  int init_dev();

 public:

  enum { FROMLINUX_CONFIGURE_PHASE = CONFIGURE_PHASE_DEFAULT,
	 TODEVICE_CONFIGURE_PHASE };
  
  FromLinux();
  ~FromLinux();

  const char *class_name() const	{ return "FromLinux"; }
  FromLinux *clone() const;
  const char *processing() const	{ return PUSH; }

  enet_statistics *stats()		{ return &_stats; }

  int configure_phase() const		{ return FROMLINUX_CONFIGURE_PHASE; }
  int configure(const Vector<String> &, ErrorHandler *);
  int initialize(ErrorHandler *);
  int initialize_device(ErrorHandler *);
  void uninitialize();

};

#endif
