#ifndef TOLINUXSNIFFERS_HH
#define TOLINUXSNIFFERS_HH
#include "element.hh"

/*
 * =c
 * ToLinuxSniffers()
 * =d
 *
 * Hands packets to any packet sniffers registered with Linux, such as packet
 * sockets. Expects packets with Ethernet headers.
 *
 * Packets are not passed to the ordinary Linux networking stacks.
 * 
 * =a ToLinux
 * =a FromLinux
 * =a FromDevice
 * =a PollDevice
 * =a ToDevice */

class ToLinuxSniffers : public Element {
 public:
  
  ToLinuxSniffers();
  ~ToLinuxSniffers();
  
  const char *class_name() const		{ return "ToLinuxSniffers"; }
  const char *processing() const		{ return PUSH; }
  
  ToLinuxSniffers *clone() const;
  
  void push(int port, Packet *);

};

#endif
