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
#include "fragmentack.hh"
#include <click/etheraddress.hh>
#include <click/confparse.hh>
#include <click/error.hh>
#include <click/glue.hh>
#include <clicknet/wifi.h>
#include <clicknet/llc.h>
#include <click/packet_anno.hh>
#include <click/straccum.hh>
#include "frag.hh"
CLICK_DECLS


FragmentAck::FragmentAck()
  : Element(1, 1)
{
}

FragmentAck::~FragmentAck()
{
}

void
FragmentAck::notify_noutputs(int n)
{
  set_noutputs(n < 2 ? 1 : 2);
}

int
FragmentAck::configure(Vector<String> &conf, ErrorHandler *errh)
{

  _window_size = 30;
  _ack_timeout_ms = 0;
  _debug = false;
  if (cp_va_parse(conf, this, errh,
		  cpOptional,
		  cpKeywords,
		  "ETHTYPE", cpUnsigned, "Ethernet encapsulation type", &_et,
		  "ETH", cpEtherAddress, "EtherAddress", &_en,
		  "WINDOW", cpUnsigned, "", &_window_size,
		  "ACK_TIMEOUT", cpUnsigned, "", &_ack_timeout_ms,
		  "DEBUG", cpBool, "", &_debug,
		  cpEnd) < 0)
    return -1;
  return 0;
}


void 
FragmentAck::send_ack(EtherAddress src) 
{
  WindowInfo *nfo = _frags.findp(src);
  if (!nfo) {
    return;
  }
  
  int num_acked = nfo->frags_rx.size();
  WritablePacket *p = Packet::make(frag_ack::packet_size(num_acked));
  click_ether *eh = (click_ether *) p->data();

  memcpy(eh->ether_shost, _en.data(), 6);
  memcpy(eh->ether_dhost, src.data(), 6);
  eh->ether_type = htons(_et);

  struct frag_ack *ack = (struct frag_ack *) (p->data() + sizeof(click_ether));

  ack->num_acked = num_acked;

  StringAccum sa;
  sa << "|";
  int frag_num = 0;

  for (int x = 0; x < nfo->frags_rx.size(); x++) {
    struct fragid f = nfo->frags_rx[x];
    sa << " " << (int) f.packet_num << " " << 
      (int) f.frag_num << " |";
    ack->set_packet(frag_num, f.packet_num);
    ack->set_frag(frag_num, f.frag_num);
    frag_num++;
  }


  if (_debug) {
    click_chatter("%{element} acking %s\n",
		  this,
		  sa.take_string().cc());
  }


  nfo->frags_rx.clear();
  nfo->waiting = false;
  output(1).push(p);
}

Packet *
FragmentAck::simple_action(Packet *p)
{

  struct frag_header *fh = (struct frag_header *) p->data();
  EtherAddress src = EtherAddress(fh->src);
  WindowInfo *nfo = _frags.findp(src);

  if (!nfo) {
    _frags.insert(src, WindowInfo(src));
    nfo = _frags.findp(src);
    nfo->waiting = false;
  }

  bool new_packets = false;
  for (int x = 0; x < fh->num_frags; x++) {
    struct frag *f = fh->get_frag(x);
    if (f->valid_checksum(fh->frag_size)) {
      new_packets |= nfo->add(fragid(f->packet_num, f->frag_num));
    }   
  } 
  
  if (new_packets && !nfo->waiting) {
    struct timeval now;
    click_gettimeofday(&now);
    nfo->first_rx = now;
  }

  if (fh->flags & FRAG_ACKME) {
    send_ack(src);
  }
  return p;
  
}



 
void
FragmentAck::add_handlers()
{
  add_default_handlers(true);
}
#include <click/vector.cc>
#include <click/hashmap.cc>
#if EXPLICIT_TEMPLATE_INSTANCES
template class HashMap<EtherAddress, FragmentAck::WindowInfo>;
#endif
EXPORT_ELEMENT(FragmentAck)
CLICK_ENDDECLS

