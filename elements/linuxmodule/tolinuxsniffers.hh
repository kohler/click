#ifndef TOLINUXSNIFFERS_HH
#define TOLINUXSNIFFERS_HH
#include "elements/linuxmodule/fromlinux.hh"

/*
 * =c
 * ToLinuxSniffers([DEVNAME])
 * =s sinks
 * sends packets to Linux packet sniffers
 * =d
 *
 * Hands packets to any packet sniffers registered with Linux, such as packet
 * sockets. Packets are not passed to the ordinary Linux networking stack.
 * Expects packets with Ethernet headers.
 *
 * If DEVNAME is present, each packet is marked to appear as if it originated
 * from that network device. As with ToDevice, DEVNAME can be an Ethernet
 * address.
 * 
 * This element is only available in the Linux kernel module.
 *
 * =a ToLinux, FromLinux, FromDevice, PollDevice, ToDevice */

class ToLinuxSniffers : public Element {

  struct device *_dev;
  
 public:
  
  ToLinuxSniffers();
  ~ToLinuxSniffers();
  
  const char *class_name() const		{ return "ToLinuxSniffers"; }
  const char *processing() const		{ return PUSH; }
  const char *flags() const			{ return "S2"; }

  int configure_phase() const	{ return FromLinux::TODEVICE_CONFIGURE_PHASE; }
  int configure(const Vector<String> &, ErrorHandler *);
  ToLinuxSniffers *clone() const;
  
  void push(int port, Packet *);

};

#endif

