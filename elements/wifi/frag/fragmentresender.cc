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
#include "fragmentresender.hh"
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


void bubble_sort(Vector<int> v) {
  for (int x = 0; x < v.size() - 1; x++) {
    for (int y = 0; y < v.size() - 1 - x; y++) {
      if (v[y+1] < v[y]) {
	int tmp = v[y];
	v[y] = v[y+1];
	v[y+1] = tmp;
      }
    }
  }

}

String print_fragids(Vector<fragid> v) {
  StringAccum sa;
  sa << "|";
  for (int x = 0; x < v.size(); x++) {
    sa << " " << v[x].packet_num << " " << v[x].frag_num << " |";
  }
  return sa.take_string();
}

void bubble_sort(Vector<fragid> *v) {


  click_chatter("%s\n", print_fragids(*v).cc());
}


void 
FragmentResender::fix() {
  for (int x = 0; x < outstanding.size() - 1; x++) {
    for (int y = 0; y < outstanding.size() - 1 - x; y++) {
      if (outstanding[y+1] > outstanding[y]) {
	fragid tmp = outstanding[y];
	outstanding[y] = outstanding[y+1];
	outstanding[y+1] = tmp;
      }
    }
  }

  for (int x = 0; x < outstanding.size(); x++) {
    if (!outstanding[x].valid()) {
      while (outstanding.size() > x) {
	outstanding.pop_back();
      }
      break;
    }
  }

  resend_limit = outstanding.size();
  resend_index = 0;

}


FragmentResender::FragmentResender()
  : Element(2, 2),
    _timer(this),
    _max_retries(15)
{
}

FragmentResender::~FragmentResender()
{
}

int
FragmentResender::configure(Vector<String> &conf, ErrorHandler *errh)
{

  _debug = false;
  _wait_for_ack = false;
  _send_ack = false;
  _window_size = 1;
  _ack_timeout_ms = 100;
  resend_index = 0;
  resend_limit = 0;
  if (cp_va_parse(conf, this, errh,
		  /* not required */
		  cpKeywords,
		  "WINDOW", cpUnsigned, "", &_window_size,
		  "ACK_TIMEOUT", cpUnsigned, "", &_ack_timeout_ms,
		  "DEBUG", cpBool, "Debug", &_debug,
		  "ETHTYPE", cpUnsigned, "Ethernet encapsulation type", &_et,
		  cpEnd) < 0)
    return -1;
  return 0;
}


int
FragmentResender::initialize (ErrorHandler *)
{
  _timer.initialize (this);
  return 0;
}


void
FragmentResender::run_timer ()
{
  if (_debug) {
    struct timeval  now;
    click_gettimeofday(&now);
    StringAccum sa;
    sa << now;
    sa << " ack_timeout";
    click_chatter("%{element} %s\n", 
		  this,
		  sa.take_string().cc());
  }
  _send_ack = true;

}

void
FragmentResender::process_ack(Packet *p) {
  struct frag_ack *ack = (struct frag_ack *) (p->data() + sizeof(click_ether));

  _timer.unschedule();

  _wait_for_ack = false;

  int min_packet_acked = 65535;
  int max_packet_acked = -1;

  for (int x = 0; x < ack->num_acked; x++) {
    int packet = ack->get_packet(x);
    int frag = ack->get_frag(x);
    struct fragid ack = fragid(packet, frag);

    min_packet_acked = MIN(packet, min_packet_acked);
    max_packet_acked = MAX(packet, max_packet_acked);

    for (int y = 0; y < outstanding.size(); y++) {
      if (ack == outstanding[y]) {
	outstanding[y].mark_invalid();
      }
    }

    PacketInfo *nfo = _packets.findp(packet);
    if (!nfo || frag > nfo->frag_status.size()) {
      click_chatter("%{element} weird fragid %s\n",
		    this,
		    ack.s().cc());
      continue;
    }
    nfo->frag_status[frag] = 1;

    if (nfo->done()) {
      click_chatter("%{element} done with packet %d\n",
		    this,
		    packet);
      if (nfo->p) {
	nfo->p->kill();
	nfo->p = 0;
      }
      _packets.remove(packet);
    }
  }

  if (_debug) {
    struct timeval  now;
    click_gettimeofday(&now);
    StringAccum sa;
    sa << now;
    sa << " processing ack "
       << "[ " << min_packet_acked << ", " 
       << max_packet_acked << " ]";
    click_chatter("%{element} %s\n", 
		  this,
		  sa.take_string().cc());
  }

  fix();
  print_window();

  _timer.unschedule();
  _send_ack = false;
  _wait_for_ack = false;

  return;
}
  

void 
FragmentResender::push(int, Packet *p) {

  process_ack(p);
  p->kill();
  return;


}

Vector<int>
FragmentResender::get_packets() {
  Vector<int> frags;
  
  for (PIIter iter = _packets.begin(); iter; iter++) {
    frags.push_back(iter.key());
  }
  bubble_sort(frags);
  return frags;
}

void
FragmentResender::print_window() {
  Vector<int> packets = get_packets();
  StringAccum sa;

  for (int x = 0; x < packets.size(); x++) {
    PacketInfo *nfo = _packets.findp(packets[x]);
    if (!nfo ) {
      click_chatter("%{element} print_window fuck packet %d : %s\n",
		    this,
		    packets[x],
		    print_fragids(outstanding).cc());
      return;
    }
    sa << "packet " << packets[x];
    sa << " [";
    for (int y = 0; y < nfo->frag_status.size(); y++) {
      sa << " " << y;
      if (nfo->frag_status[y]) {
	sa << " 1 ";
      } else {
	sa << " 0 ";
      }
    }
    sa << "]\n";
  }

  if (_debug) {
    click_chatter("%{element} window: index %d limit %d %s", 
		  this,
		  resend_index,
		  resend_limit,
		  sa.take_string().cc());
  }
}

Packet *
FragmentResender::ack_request() {
  WritablePacket *p = Packet::make(sizeof(struct frag_header));
  struct frag_header *fh = (struct frag_header *) p->data();
  
  int num_frags = 0;
  bool header_done = false;

  _timer.unschedule();

  if (!outstanding.size()) {
    click_chatter("%{element} ack_request, but no OUSTANDING\n",
		  this);
    return 0;
  }
  int x = 0;
  PacketInfo *nfo = 0;

  for ( x = 0; x < outstanding.size(); x++) {
    if (outstanding[x].valid()) {
      nfo = _packets.findp(outstanding[x].packet_num);
      if (nfo) {
	break;
      }
    }
  }

  if (!nfo) {
    click_chatter("%{element} couldn't find packet to ack\n",
		  this);
    fix();
    print_window();
    goto done;
  }
  
  memcpy(fh, nfo->p->data(), sizeof(struct frag_header));
  header_done = true;

  fh->flags = FRAG_ACKME;
  fh->ether_type = htons(_et);
  if (_debug) {
    struct timeval  now;
    click_gettimeofday(&now);
    StringAccum sa;
    sa << now;
    sa << " ack_request "
       << " resend_index " << resend_index
       << " oustanding_size " << outstanding.size()
       << " ackme " << (fh->flags & FRAG_ACKME);
    click_chatter("%{element} %s\n", 
		  this,
		  sa.take_string().cc());
  }
  fh->num_frags = num_frags;
  fh->num_frags_packet = 0;
  fh->set_checksum();

  _timer.schedule_after_ms(_ack_timeout_ms);
 done:
  _wait_for_ack = true;
  _send_ack = false;

  return p;
}
Packet *
FragmentResender::do_resend() {
  
  unsigned max_len = 1600;
  Packet *p = Packet::make(max_len);
  struct frag_header *fh = (struct frag_header *) p->data();
  
  int num_frags = 0;
  bool header_done = false;

  int min_packet_resent = 0;
  int max_packet_resent = 0;

  int index_change = 0;
  for (int x = 0; x < outstanding.size(); x++) {
    int actual_index = (resend_index + x) % outstanding.size();
    int packet = outstanding[actual_index].packet_num;
    int frag = outstanding[actual_index].frag_num;

    if (header_done && 
	fh->packet_size(num_frags + 1, fh->frag_size ) > max_len) {
      goto done;
    }
    
    index_change++;
    PacketInfo *nfo = _packets.findp(packet);
    if (!nfo ) {
      continue;
    }
    
    if (nfo->frag_sends[frag]++ > _max_retries) {
      click_chatter("%{element} dropping frag %s\n",
		    this,
		    fragid(packet, frag).s().cc());
      for (int y = 0; y < outstanding.size(); y++) {
	if (outstanding[y].packet_num == packet) {
	  outstanding[y].mark_invalid();
	}
      }
      if (nfo->p) {
	nfo->p->kill();
	nfo->p = 0;
      }
      _packets.remove(packet);
      continue;
    }
    min_packet_resent = MIN(packet, min_packet_resent);
    max_packet_resent = MAX(packet, max_packet_resent);
    
    if (!header_done) {
      memcpy(fh, nfo->p->data(), sizeof(struct frag_header));
      header_done = true;
      min_packet_resent = packet;
    }
    fh->frag_size = ((struct frag_header *)nfo->p->data())->frag_size;
    memcpy(fh->get_frag(num_frags),
	   ((struct frag_header *)nfo->p->data())->get_frag(frag),
	   sizeof(struct frag) + fh->frag_size);
    num_frags++;
  }
 done:

  resend_index += index_change;
      

  //fix();  
  fh->flags = FRAG_RESEND;

  if (_debug) {
    struct timeval  now;
    click_gettimeofday(&now);
    StringAccum sa;
    sa << now;
    sa << " resend frags " << num_frags
       << " resend_index " << resend_index
       << " oustanding_size " << outstanding.size()
       << " ackme " << (fh->flags & FRAG_ACKME)
       << " [ " << min_packet_resent << ", " 
       << max_packet_resent << " ]";
    click_chatter("%{element} %s\n", 
		  this,
		  sa.take_string().cc());
  }
  fh->num_frags = num_frags;
  //fh->num_frags_packet = 0;
  fh->set_checksum();
  p->take(max_len - fh->packet_size(fh->num_frags, fh->frag_size));

  return p;


}

Packet *
FragmentResender::pull(int port) {

  if (_wait_for_ack) {
    return 0;
  }

  if (port == 0) {
    if (_send_ack || (unsigned) outstanding.size() > _window_size) {
      return ack_request();
    }
    return 0;
  }

  if (resend_index < resend_limit) {
    return do_resend();
  }

  Packet *p = input(0).pull();

  if (!p) {
    if (outstanding.size()) {
      if (_debug) {
	click_chatter("%{element} marking send_ack_request\n",
		      this);
	print_window();
      }
      _send_ack = true;
    }
    return p;
  }

  struct frag_header *fh = (struct frag_header *) p->data();
  EtherAddress dst = EtherAddress(fh->dst);

  _packets.insert(fh->packet_num, PacketInfo());
  PacketInfo *nfo = _packets.findp(fh->packet_num);
  nfo->dst = dst;
  click_gettimeofday(&nfo->last_tx);

  for (int x = 0; x < fh->num_frags; x++) {
    nfo->frag_status.push_back(0);
    nfo->frag_sends.push_back(1);
    outstanding.push_back(fragid(fh->packet_num, x));
  }
  nfo->p = p->clone();

  if (_debug) {
    struct timeval  now;
    click_gettimeofday(&now);
    StringAccum sa;
    sa << now;
    sa << " send packet  " << fh->packet_num;
    click_chatter("%{element} %s\n", 
		  this,
		  sa.take_string().cc());
  }

  return p;
}


enum {H_DEBUG, };

static String 
FragmentResender_read_param(Element *e, void *thunk)
{
  FragmentResender *td = (FragmentResender *)e;
    switch ((uintptr_t) thunk) {
      case H_DEBUG:
	return String(td->_debug) + "\n";
    default:
      return String();
    }
}
static int 
FragmentResender_write_param(const String &in_s, Element *e, void *vparam,
		      ErrorHandler *errh)
{
  FragmentResender *f = (FragmentResender *)e;
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
FragmentResender::add_handlers()
{
  add_default_handlers(true);

  add_read_handler("debug", FragmentResender_read_param, (void *) H_DEBUG);

  add_write_handler("debug", FragmentResender_write_param, (void *) H_DEBUG);
}

#include <click/hashmap.cc>
#if EXPLICIT_TEMPLATE_INSTANCES
template class HashMap<int, FragmentResender::PacketInfo>;
#endif
EXPORT_ELEMENT(FragmentResender)
CLICK_ENDDECLS

