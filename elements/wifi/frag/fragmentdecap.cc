/*
 * wifiencap.{cc,hh} -- encapsultates 802.11 packets
 * John Bicket
 *
 * Copyright (c) 2004 Massachusetts Institute of Technology
 *
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files (the "Software"),
 * to deal in the Software without restriction, subject to the conditions
 * listed in the Click LICENSE file. These conditions include: you must
 * preserve this copyright notice, and you cannot mention the copyright
 * holders in advertising related to the Software without their permission.
 * The Software is provided WITHOUT ANY WARRANTY, EXPRESS OR IMPLIED. This
 * notice is a summary of the Click LICENSE file; the license in that file is
 * legally binding.
 */

#include <click/config.h>
#include "fragmentdecap.hh"
#include <click/etheraddress.hh>
#include <click/confparse.hh>
#include <click/error.hh>
#include <click/glue.hh>
#include <clicknet/wifi.h>
#include <clicknet/llc.h>
#include <click/packet_anno.hh>
#include "frag.hh"
CLICK_DECLS


FragmentDecap::FragmentDecap()
{
}

FragmentDecap::~FragmentDecap()
{
}

int
FragmentDecap::configure(Vector<String> &conf, ErrorHandler *errh)
{

  _debug = false;
  if (cp_va_parse(conf, this, errh,
		  /* not required */
		  cpKeywords,
		  "DEBUG", cpBool, "Debug", &_debug,
		  cpEnd) < 0)
    return -1;
  return 0;
}

Packet *
FragmentDecap::simple_action(Packet *p)
{

  if (p->length() < sizeof(struct click_ether)) {
    click_chatter("%{element}: packet too small: %d vs %d\n",
		  this,
		  p->length(),
		  sizeof(struct click_ether));

    p->kill();
    return 0;
	      
  }


  
  struct frag_header *fh = (struct frag_header *) p->data();

  WritablePacket *p_out = Packet::make(sizeof(click_ether) + 
				       fh->num_frags * fh->frag_size);

  memcpy(p_out->data(), 
	 p->data(),
	 sizeof(click_ether));

  for (int x = 0; x < fh->num_frags; x++) {
    memcpy(p_out->data() + sizeof(click_ether) + x*fh->frag_size,
	   p->data() + sizeof(frag_header) + x*(sizeof(frag) + fh->frag_size) + sizeof(frag),
	   fh->frag_size);
  }

  p->kill();
  return p_out;
}


enum {H_DEBUG, };

static String 
FragmentDecap_read_param(Element *e, void *thunk)
{
  FragmentDecap *td = (FragmentDecap *)e;
    switch ((uintptr_t) thunk) {
      case H_DEBUG:
	return String(td->_debug) + "\n";
    default:
      return String();
    }
}
static int 
FragmentDecap_write_param(const String &in_s, Element *e, void *vparam,
		      ErrorHandler *errh)
{
  FragmentDecap *f = (FragmentDecap *)e;
  String s = cp_uncomment(in_s);
  switch((int)vparam) {
  case H_DEBUG: {    //debug
    bool debug;
    if (!cp_bool(s, &debug)) 
      return errh->error("debug parameter must be boolean");
    f->_debug = debug;
    break;
  }
  }
  return 0;
}
 
void
FragmentDecap::add_handlers()
{
  add_read_handler("debug", FragmentDecap_read_param, (void *) H_DEBUG);

  add_write_handler("debug", FragmentDecap_write_param, (void *) H_DEBUG);
}

#include <click/hashmap.cc>
#if EXPLICIT_TEMPLATE_INSTANCES
template class HashMap<int, FragmentDecap::PacketInfo>;
#endif
EXPORT_ELEMENT(FragmentDecap)
CLICK_ENDDECLS

