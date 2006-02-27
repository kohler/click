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
#include "fragment.hh"
#include <click/etheraddress.hh>
#include <click/confparse.hh>
#include <click/error.hh>
#include <click/glue.hh>
#include <clicknet/wifi.h>
#include <clicknet/llc.h>
#include <click/packet_anno.hh>
#include "frag.hh"
CLICK_DECLS


Fragment::Fragment()
{
}

Fragment::~Fragment()
{
}

int
Fragment::configure(Vector<String> &conf, ErrorHandler *errh)
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

void 
Fragment::push(int, Packet *p)
{

  struct frag_header *fh = (struct frag_header *) p->data();
  
  for (unsigned x = 0; x < fh->num_frags; x++) {
    struct frag *f = fh->get_frag(x);
    if (!f->valid_checksum(fh->frag_size)) {
      if (_debug) {
	click_chatter("%{element} frag invalid checksum %d %d\n",
		      this,
		      f->packet_num,
		      f->frag_num);
      }
      continue;
    }
    WritablePacket *frag = Packet::make(sizeof(struct frag) + 
					fh->frag_size + 
					sizeof(struct frag_header));

    struct frag_header *fh2 = (struct frag_header *) 
      frag->data();
       

    memcpy(frag->data(), 
	   p->data(), 
	   sizeof(struct frag_header));


    fh2->num_frags = 1;
    fh2->header_checksum = 0;
    fh2->header_checksum = click_in_cksum((unsigned char *) fh2, 
					 sizeof(frag_header));

    memcpy(frag->data() + sizeof(struct frag_header),
	   p->data() + sizeof(struct frag_header) + 
	   x*(fh->frag_size + sizeof(struct frag)),
	   sizeof(struct frag) + fh->frag_size);


    output(0).push(frag);
  }

  p->kill();
  return;
}


enum {H_DEBUG, };

static String 
Fragment_read_param(Element *e, void *thunk)
{
  Fragment *td = (Fragment *)e;
    switch ((uintptr_t) thunk) {
      case H_DEBUG:
	return String(td->_debug) + "\n";
    default:
      return String();
    }
}
static int 
Fragment_write_param(const String &in_s, Element *e, void *vparam,
		      ErrorHandler *errh)
{
  Fragment *f = (Fragment *)e;
  String s = cp_uncomment(in_s);
  switch((intptr_t)vparam) {
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
Fragment::add_handlers()
{
  add_read_handler("debug", Fragment_read_param, (void *) H_DEBUG);

  add_write_handler("debug", Fragment_write_param, (void *) H_DEBUG);
}
CLICK_ENDDECLS
EXPORT_ELEMENT(Fragment)
