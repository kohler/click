// -*- mode: c++; c-basic-offset: 4 -*-
#ifndef CLICK_TOHOSTSNIFFERS_HH
#define CLICK_TOHOSTSNIFFERS_HH
#include "elements/linuxmodule/tohost.hh"

/*
 * =c
 * ToHostSniffers([DEVNAME, I<keywords> SNIFFERS, ALLOW_NONEXISTENT])
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
 * =n
 *
 * ToHostSniffers behaves exactly like ToHost, except that the SNIFFERS
 * keyword argument defaults to true.
 *
 * =a ToHost, FromHost, FromDevice, PollDevice, ToDevice */

class ToHostSniffers : public ToHost { public:

    ToHostSniffers();
    ~ToHostSniffers();

    const char *class_name() const		{ return "ToHostSniffers"; }
    ToHostSniffers *clone() const;
  
};

#endif

