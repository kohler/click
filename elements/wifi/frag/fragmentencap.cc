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
#include "fragmentencap.hh"
#include <click/etheraddress.hh>
#include <click/confparse.hh>
#include <click/error.hh>
#include <click/glue.hh>
#include <clicknet/wifi.h>
#include <clicknet/llc.h>
#include <click/packet_anno.hh>
#include "frag.hh"
CLICK_DECLS


FragmentEncap::FragmentEncap()
  : Element(1, 1)
{
  MOD_INC_USE_COUNT;
}

FragmentEncap::~FragmentEncap()
{
  MOD_DEC_USE_COUNT;
}

int
FragmentEncap::configure(Vector<String> &conf, ErrorHandler *errh)
{

  _debug = false;
  _frag_length = 100;
  _packet_num = 0;
  if (cp_va_parse(conf, this, errh,
		  /* not required */
		  cpKeywords,
		  "DEBUG", cpBool, "Debug", &_debug,
		  "LENGTH", cpUnsigned, "length", &_frag_length,
		  cpEnd) < 0)
    return -1;
  return 0;
}

Packet *
FragmentEncap::simple_action(Packet *p)
{

  if (!p) {
    return p;
  }
  if (p->length() < sizeof(struct click_ether)) {
    click_chatter("%{element}: packet too small: %d vs %d\n",
		  this,
		  p->length(),
		  sizeof(struct click_ether));

    p->kill();
    return 0;
	      
  }

  EtherAddress src;
  EtherAddress dst;
  uint16_t ethtype;

  click_ether *eh = (click_ether *) p->data();
  src = EtherAddress(eh->ether_shost);
  dst = EtherAddress(eh->ether_dhost);
  ethtype = eh->ether_type;


  unsigned data_length = p->length() - sizeof(click_ether);

  unsigned fragmentencaps = data_length / _frag_length;

  if (data_length % fragmentencaps != 0) {
    fragmentencaps++;
  }

  unsigned new_size = sizeof(frag_header) + 
    fragmentencaps * (_frag_length + sizeof(struct frag));

  
  WritablePacket *p_out = Packet::make(new_size);

  if (!p_out) {
    p->kill();
    return 0;
  }
  
  struct frag_header *fh = (struct frag_header *) p_out->data();

  memset(fh, 0, sizeof(struct frag_header));
  memcpy(p_out->data(), p->data(), sizeof(click_ether));
  fh->flags = 0;
  fh->num_frags = fragmentencaps;
  fh->num_frags_packet = fragmentencaps;
  fh->frag_size = _frag_length;
  fh->packet_num = _packet_num;
  fh->set_checksum();
  for (unsigned x = 0; x < fragmentencaps; x++) {

    struct frag *f = fh->get_frag(x);

    const unsigned char *data = p->data() + 
      sizeof(click_ether) + 
      x * fh->frag_size;

    memcpy(f->data(), data, fh->frag_size);
    f->frag_num = (uint16_t) x;
    f->packet_num = _packet_num;
    f->set_checksum(fh->frag_size);
  }

  _packet_num++;
  p->kill();
  return p_out;
}


enum {H_DEBUG, };

static String 
FragmentEncap_read_param(Element *e, void *thunk)
{
  FragmentEncap *td = (FragmentEncap *)e;
    switch ((uintptr_t) thunk) {
      case H_DEBUG:
	return String(td->_debug) + "\n";
    default:
      return String();
    }
}
static int 
FragmentEncap_write_param(const String &in_s, Element *e, void *vparam,
		      ErrorHandler *errh)
{
  FragmentEncap *f = (FragmentEncap *)e;
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
FragmentEncap::add_handlers()
{
  add_default_handlers(true);

  add_read_handler("debug", FragmentEncap_read_param, (void *) H_DEBUG);

  add_write_handler("debug", FragmentEncap_write_param, (void *) H_DEBUG);
}
CLICK_ENDDECLS
EXPORT_ELEMENT(FragmentEncap)
