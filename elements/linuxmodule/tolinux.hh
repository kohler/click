#ifndef TOLINUX_HH
#define TOLINUX_HH
#include "element.hh"

/*
 * =c
 * ToLinux()
 * =d
 * Hands packets to the ordinary Linux protocol stack.
 * Expects packets with Ethernet headers.
 * 
 * You should probably give Linux IP packets addressed to
 * the local machine (including broadcasts), and a copy
 * of each ARP reply.
 *
 * This element is only available in the Linux kernel module.
 *
 * =a ToLinuxSniffers
 * =a FromLinux
 * =a FromDevice
 * =a PollDevice
 * =a ToDevice
 */

class ToLinux : public Element {
 public:
  
  ToLinux();
  ~ToLinux();
  
  const char *class_name() const		{ return "ToLinux"; }
  const char *processing() const		{ return PUSH; }
  
  ToLinux *clone() const;
  
  void push(int port, Packet *);

};

#endif TOLINUX_HH
