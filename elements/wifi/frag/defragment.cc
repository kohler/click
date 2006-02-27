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
#include "defragment.hh"
#include <click/etheraddress.hh>
#include <click/confparse.hh>
#include <click/error.hh>
#include <click/glue.hh>
#include <clicknet/wifi.h>
#include <clicknet/llc.h>
#include <click/packet_anno.hh>
#include "frag.hh"
CLICK_DECLS


Defragment::Defragment()
{
}

Defragment::~Defragment()
{
}

int
Defragment::configure(Vector<String> &conf, ErrorHandler *errh)
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
Defragment::simple_action(Packet *p)
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
  struct frag *f = (struct frag *) (p->data() + sizeof(struct frag_header));


  if (!f->valid_checksum(fh->frag_size)) {
    click_chatter("%{element} frag failed checksum\n",
		  this);
    p->kill();
    return 0;
  }
  EtherAddress src = EtherAddress(fh->src);

  PacketInfo *nfo = _packets.findp(f->packet_num);

  if (!nfo) {
    _packets.insert(f->packet_num, PacketInfo(src, f->packet_num, 
					      fh->frag_size,
					      fh->num_frags_packet));
    nfo = _packets.findp(f->packet_num);
  }

  if (f->frag_num >= nfo->fragments.size()) {
    click_chatter("%{element} packet %d frag_num is %d size is %d\n",
		  this,
		  f->packet_num,
		  f->frag_num,
		  nfo->fragments.size());
    p->kill();
    return 0;
  }

  if (nfo->fragments[f->frag_num]) {
    click_chatter("%{element} repeat frag [%d %d]\n",
		  this,
		  f->packet_num,
		  f->frag_num);
    p->kill();
    return 0;
  }


  if (_debug) {
    click_chatter("%{element} got packet %d frag %d/%d rx %d\n",
		  this,
		  f->packet_num,
		  f->frag_num,
		  nfo->num_frags,
		  nfo->fragments_rx);
		  
  }

  nfo->fragments[f->frag_num] = p;
  nfo->fragments_rx++;

  if (nfo->fragments_rx != nfo->num_frags) {
    return 0;
  }
  int len = frag_header::packet_size(nfo->num_frags, nfo->frag_size);
  if (_debug) {
    click_chatter("%{element} received %d packets, defragmenting frag_size %d num_frags %d len %d\n",
		  this,
		  nfo->fragments_rx,
		  nfo->frag_size,
		  nfo->num_frags,
		  len);
  }

  WritablePacket *p_out = Packet::make(len);
  
  if (!p_out) {
    click_chatter("%{element} couldn't create packet\n",
		  this);
    return 0;
  }
  if (!nfo->fragments[0]) {
    click_chatter("%{element} fragment %d is null!\n",
		  this,
		  0);
  }

  memcpy(p_out->data(), 
	 nfo->fragments[0]->data(), 
	 sizeof(frag_header));
  struct frag_header *fh2 = (struct frag_header *) p_out->data();
  fh2->num_frags = fh2->num_frags_packet = nfo->num_frags;
  fh2->num_frags = nfo->num_frags;
  fh2->packet_num = nfo->packet;
  fh2->frag_size = nfo->frag_size;
  fh2->set_checksum();
  for (int x = 0; x < nfo->num_frags; x++) {
    if (!nfo->fragments[x]) {
      click_chatter("%{element} fragment %d is null!\n",
		    this,
		    x);
    }
    memcpy(fh2->get_frag(x),
	   nfo->fragments[x]->data() + sizeof(frag_header),
	   nfo->frag_size + sizeof(struct frag));
    nfo->fragments[x]->kill();
    nfo->fragments[x] = 0;
  }

  _packets.remove(nfo->packet);
  return p_out;
}


enum {H_DEBUG, };

static String 
Defragment_read_param(Element *e, void *thunk)
{
  Defragment *td = (Defragment *)e;
    switch ((uintptr_t) thunk) {
      case H_DEBUG:
	return String(td->_debug) + "\n";
    default:
      return String();
    }
}
static int 
Defragment_write_param(const String &in_s, Element *e, void *vparam,
		      ErrorHandler *errh)
{
  Defragment *f = (Defragment *)e;
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
Defragment::add_handlers()
{
  add_read_handler("debug", Defragment_read_param, (void *) H_DEBUG);

  add_write_handler("debug", Defragment_write_param, (void *) H_DEBUG);
}

#include <click/hashmap.cc>
#if EXPLICIT_TEMPLATE_INSTANCES
template class HashMap<int, Defragment::PacketInfo>;
#endif
EXPORT_ELEMENT(Defragment)
CLICK_ENDDECLS

