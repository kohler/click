#ifndef CLICK_TOHOSTSNIFFERS_HH
#define CLICK_TOHOSTSNIFFERS_HH
#include "elements/linuxmodule/fromhost.hh"

/*
 * =c
 * ToHostSniffers([DEVNAME])
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
 * =a ToHost, FromLinux, FromDevice, PollDevice, ToDevice */

class ToHostSniffers : public Element { public:

  ToHostSniffers();
  ~ToHostSniffers();
  
  const char *class_name() const		{ return "ToHostSniffers"; }
  const char *processing() const		{ return PUSH; }
  const char *flags() const			{ return "S2"; }
  ToHostSniffers *clone() const;

  int configure_phase() const	{ return FromHost::CONFIGURE_PHASE_TODEVICE; }
  int configure(const Vector<String> &, ErrorHandler *);
  void uninitialize();
  
  void push(int port, Packet *);

 private:

  net_device *_dev;
  
};

#endif

