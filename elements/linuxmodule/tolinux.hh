#ifndef TOLINUX_HH
#define TOLINUX_HH
#include "elements/linuxmodule/fromlinux.hh"

/*
 * =c
 * ToLinux([DEVNAME])
 * =s sinks
 * sends packets to Linux
 * =d
 *
 * Hands packets to the ordinary Linux protocol stack.
 * Expects packets with Ethernet headers.
 * 
 * You should probably give Linux IP packets addressed to
 * the local machine (including broadcasts), and a copy
 * of each ARP reply.
 *
 * If DEVNAME is present, each packet is marked to appear as if it originated
 * from that network device. As with ToDevice, DEVNAME can be an Ethernet
 * address.
 * 
 * This element is only available in the Linux kernel module.
 *
 * =a ToLinuxSniffers, FromLinux, FromDevice, PollDevice, ToDevice
 */

class ToLinux : public Element {
  
  struct device *_dev;
  
 public:
  
  ToLinux();
  ~ToLinux();
  
  const char *class_name() const		{ return "ToLinux"; }
  const char *processing() const		{ return PUSH; }
  const char *flags() const			{ return "S2"; }
  
  int configure_phase() const	{ return FromLinux::CONFIGURE_PHASE_TODEVICE; }
  int configure(const Vector<String> &, ErrorHandler *);
  ToLinux *clone() const;
  
  void push(int port, Packet *);

};

#endif
