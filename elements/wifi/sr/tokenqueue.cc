// -*- c-basic-offset: 4 -*-
/*
 * tokenqueue.{cc,hh} -- NotifierQueue with FIFO and LIFO inputs
 * John Bicket
 *
 * Copyright (c) 2003 International Computer Science Institute
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
#include <click/confparse.hh>
#include <click/error.hh>
#include <click/glue.hh>
#include <click/ipaddress.hh>
#include <click/straccum.hh>
#include <clicknet/ether.h>
#include <click/elemfilter.hh>
#include <elements/wifi/sr/path.hh>
#include <elements/standard/notifierqueue.hh>
#include <elements/wifi/sr/srforwarder.hh>
#include "srpacket.hh"
#include <click/router.hh>
#include <click/llrpc.h>
#include "tokenqueue.hh"
CLICK_DECLS

#define max(a, b) ((a) > (b) ? (a) : (b))
#define min(a, b) ((a) < (b) ? (a) : (b))

TokenQueue::TokenQueue()
    : _timer(this)
{
    set_ninputs(3);
    set_noutputs(2);
    _catchup_timeout = Timestamp(2, 0);
    _tokens = 0;
    _retransmits = 0;
    _normal = 0;
}

TokenQueue::~TokenQueue()
{
}

void *
TokenQueue::cast(const char *n)
{
    if (strcmp(n, "TokenQueue") == 0)
	return (TokenQueue *)this;
    else
	return NotifierQueue::cast(n);
}

int
TokenQueue::configure (Vector<String> &conf, ErrorHandler *errh)
{

    ActiveNotifier::initialize(router());
    
  int ret;
  int threshold = 0;
  _debug = false;
  int packet_to = 0;
  int new_capacity = 1000;
  ret = cp_va_parse(conf, this, errh,
		    cpKeywords,
		    "ETHTYPE", cpUnsigned, "Ethernet encapsulation type", &_et,
		    "LENGTH", cpUnsigned, "maximum queue length", &new_capacity,
		    "PACKET_TIMEOUT", cpUnsigned, "packet timeout", &packet_to,
		    "THRESHOLD", cpInteger, "packets", &threshold,
		    "SR", cpElement, "SRForwarder element", &_sr_forwarder,
		    "DEBUG", cpBool, "Debug", &_debug,
                    cpEnd);
  if (ret < 0) {
    return ret;
  }

  if (!_et) 
      return errh->error("ETHTYPE not specified");

  _capacity = new_capacity;
  ret = set_packet_timeout(errh, packet_to);
  if (ret < 0) {
    return ret;
  }

  ret = set_threshold(errh, threshold);
  if (ret < 0) {
    return ret;
  }

  if (!_sr_forwarder) 
    return errh->error("SR not specified");
  if (_sr_forwarder->cast("SRForwarder") == 0) 
    return errh->error("SR element is not a SRForwarder");

  /* convehop path_duration from ms to a struct timeval */
  _active_duration = Timestamp::make_msec(15 * 1000);

  /* convehop path_duration from ms to a struct timeval */
  _clear_duration = Timestamp::make_msec(30 * 1000);
  
  _timer.initialize(this);
  _timer.schedule_now();
  return 0;
}
void
TokenQueue::run_timer() 
{
  Vector<Path> to_clear;
  Vector<Path> not_active;

  for (PathIter iter = _paths.begin(); iter; iter++) {
      const PathInfo &nfo = iter.value();
      if (nfo._active && nfo.active_timedout()) {
	  not_active.push_back(nfo._p);
      }
  }

  for (int x = 0; x < not_active.size(); x++) {
      PathInfo *nfo = _paths.findp(not_active[x]);
      nfo->_active = false;
      if (_debug) {
	  StringAccum sa;
	  sa << id() << " " << Timestamp::now();
	  sa << " mark_inactive " << path_to_string(nfo->_p);
	  click_chatter("%s", sa.take_string().cc());
      }
  }
  _timer.schedule_after_ms(_active_duration.sec()/2);
}
TokenQueue::PathInfo *
TokenQueue::find_path_info(Path p)
{
    PathInfo *nfo = _paths.findp(p);

    if (nfo) {
	return nfo;
    } 

    nfo = _paths.findp(reverse_path(p));
    if (nfo) {
	return nfo;
    }

    _paths.insert(p, PathInfo(p, this));
    nfo = _paths.findp(p);
    nfo->_towards = p[p.size()-1];
    nfo->reset();
    return _paths.findp(p);
}
bool
TokenQueue::ready_for(const Packet *p_in, Path match) {
    click_ether *eh = (click_ether *) p_in->data();
    if (eh->ether_type != htons(_et)) {
	return true;
    }
    

    struct srpacket *pk = (struct srpacket *) (eh+1);
    Path p = pk->get_path();

    if (match == p) {
	return true;
    } else if (match.size()) {
	return false;
    }

    PathInfo *nfo = find_path_info(p);
    if (!nfo) {
	click_chatter("TokenQueue %s: couldn't find info for %s!\n",
		      id().cc(),
		      path_to_string(p).cc());
	return false;
    }

    /* 
     * pk->seq() being nonzero means it's a retransmit
     */
    if (pk->seq()) {
	return true;
    }

    /*
     * if it's going in the correct direction
     */
    if (nfo->_towards == p[p.size()-1] && nfo->_token) {
	return true;
    }

    if (!nfo->_active) {
	return true;
    }
    return false;
    
}

Packet *
TokenQueue::pull(int)
{
    Packet *packet = NULL;
    WritablePacket *p_in = NULL;
    bool follow_up = false;
    Path p = Path();
    PathInfo *nfo = NULL;
    Timestamp now = Timestamp::now();
    if (!_normal && !_tokens && !_retransmits) {
	goto done;
    }
    packet = yank1(yank_filter(this, Path()));
    if (packet) {
	p_in = packet->uniqueify();
	click_ether *eh = (click_ether *) p_in->data();

	if (eh->ether_type != htons(_et)) {
	    _normal--;
	    goto done;
	}
	struct srpacket *pk = (struct srpacket *) (eh+1);
	p = pk->get_path();
	nfo = find_path_info(p);
	if (pk->seq()) {
	    /*
	     * this is a retransmit, just send it now
	     */
	    if (_debug) {
		StringAccum sa;
		sa << id() << " " << now;
		sa << " seq " << pk->seq();
		sa << " retransmit";
		sa << " towards " << p[p.size()-1].s();
		sa << " tx " << nfo->_packets_tx;
		sa << " tx_time " << now - nfo->_first_tx;
		sa << " total_time " << now - nfo->_first_rx;
		sa << " token " << pk->flag(FLAG_SCHEDULE_TOKEN);
		click_chatter("%s", sa.take_string().cc());
	    }
	    _retransmits--;
	    goto done;
	}
	follow_up = (yank1_peek(yank_filter(this, p)));
    } else {
	/* find an expired token */
	for (PathIter iter = _paths.begin(); iter; iter++) {
	    const PathInfo &nfo = iter.value();
	    /* 
	     * only send a fake if I'm active
	     */
	    if (nfo._token &&
		nfo._active) {
		p = nfo._p;
		break;
	    }
	}
	if (!p.size()) {
	    goto done;
	}
	nfo = find_path_info(p);
	if (nfo->_towards != p[p.size()-1]) {
	    p = reverse_path(p);
	}
	
	/* fake up a token packet */
	packet = Packet::make((unsigned int)0);
	packet = _sr_forwarder->encap(packet, p, 0);
	if (!packet) {
	    goto done;
	    return 0;
	}
	p_in = packet->uniqueify();
	click_ether *eh = (click_ether *) p_in->data();
	struct srpacket *pk = (struct srpacket *) (eh+1);
	pk->set_flag(FLAG_SCHEDULE | FLAG_SCHEDULE_TOKEN | FLAG_SCHEDULE_FAKE);
	if (_debug) {
	    StringAccum sa;
	    sa << id() << " " << now;
	    sa << " fake    ";
	    sa << " towards " << p[p.size()-1].s();
	    sa << " rx " << nfo->_packets_rx;
	    click_chatter("%s", sa.take_string().cc());
	}	
    }


    if (p_in) {
	click_ether *eh = (click_ether *) p_in->data();
	struct srpacket *pk = (struct srpacket *) (eh+1);

	pk->set_seq(nfo->_packets_tx++);
	if (nfo->_packets_tx == 1) {
	    nfo->_first_tx = now;
	    if (nfo->is_endpoint(_sr_forwarder->ip())) {
		++nfo->_seq;
	    }
	    if (_debug) {
		StringAccum sa;
		sa << id() << " " << now;
		sa << " first_tx";
		sa << " seq " << nfo->_seq;
		sa << " towards " << nfo->_towards;
		sa << " rx " << nfo->_packets_rx;
		click_chatter("%s", sa.take_string().cc());
	    }
	}
	pk->set_seq2(nfo->_seq);
	if (pk->flag(FLAG_SCHEDULE_FAKE) || nfo->_packets_tx == _threshold || !follow_up) {
	    pk->set_flag(FLAG_SCHEDULE_TOKEN);
	    if (_debug) {
		StringAccum sa;
		sa << id() << " " << now;
		sa << " token_tx";
		sa << " seq " << nfo->_seq;
		sa << " towards " << nfo->_towards;
		sa << " tx " << nfo->_packets_tx;
		sa << " tx_time " << now - nfo->_first_tx;
		sa << " total_time " << now - nfo->_first_rx;
		click_chatter("%s", sa.take_string().cc());
	    }
	    
	    nfo->_token = false;
	    nfo->_rx_token = false;
	    _tokens--;
	    nfo->_packets_tx = 0;
	    nfo->_packets_rx = 0;
	    nfo->_tokens_passed++;
	    nfo->_towards = nfo->other_endpoint(nfo->_towards);
	}

	nfo->_last_tx = now;
	if (pk->flag(FLAG_SCHEDULE_FAKE)) {
	    nfo->_active = false;
	} else {
	    nfo->_last_real = now;
	    nfo->_active = true;
	}

	/* finally, we altered the packet, so we need to redo 
	 * the checksum
	 */
	pk->set_checksum();
    }
    
 done:
    if (_normal == 0 && _tokens == 0 && _retransmits == 0) {
	if (++_sleepiness == SLEEPINESS_TRIGGER) {
	    sleep_listeners();	
	}
    } else {
	_sleepiness = 0;
    }
    return p_in;
}



int 
TokenQueue::bubble_up(Packet *p_in)
{
    click_ether *eh = (click_ether *) p_in->data();
    struct srpacket *pk = (struct srpacket *) (eh+1);
    Path p = pk->get_path();
    bool reordered = false;
    for (int x = _head; x != _tail; x = next_i(x)) {
	click_ether *eh2 =  (click_ether *) _q[x]->data();
	if (eh2->ether_type == htons(_et)){
	    struct srpacket *pk2 = (struct srpacket *) (eh2+1);
	    Path p2 = pk2->get_path();
	    if (p == p2) {
		if (pk->data_seq() == pk2->data_seq()) {
		    /* packet dup */
		    return 0;
		} else if (pk->data_seq() < pk2->data_seq()) {
		    if (!reordered) {
			reordered = true;
			struct timeval now;
			click_gettimeofday(&now);
			StringAccum sa;
			sa << "TokenQueue " << now;
			sa << " reordering ";
			sa << " pk->seq " << pk->data_seq();
			sa << " pk2->seq " << pk2->data_seq();
			sa << " on ";
			sa << path_to_string(p);
			click_chatter("%s", sa.take_string().cc());
		    }
		    Packet *tmp = _q[x];
		    _q[x] = p_in;
		    p_in = tmp;
		    p = p2;
		}
	    
	    
	    }
	}

    }

    eh = (click_ether *) p_in->data();
    pk = (struct srpacket *) (eh+1);

    PathInfo *nfo = find_path_info(p);
    if (!enq(p_in)) {
	/* mark the ecn bit of the next packet */
	nfo->_congestion = true;
    } else {
	if (nfo->_congestion) {
	    pk->set_flag(FLAG_ECN);
	    struct timeval now;
	    click_gettimeofday(&now);
	    StringAccum sa;
	    sa << "TokenQueue " << now;
	    sa << " ECN";
	    sa << " pk->seq " << pk->data_seq();
	    sa << path_to_string(nfo->_p);
	    click_chatter("%s", sa.take_string().cc());
	}
	nfo->_congestion = false;
    }
    return 0;

}

void
TokenQueue::process_source(struct srpacket *pk) 
{
    Path p = pk->get_path();
    PathInfo *nfo = NULL;
    
    nfo = _paths.findp(p);
    if (!nfo) {
	nfo = _paths.findp(reverse_path(p));
    }
    if (!nfo) {
	if (_debug) {
	    StringAccum sa;
	    sa << id() << " " << Timestamp::now();
	    sa << " create:  new_path " << path_to_string(p);
	    click_chatter("%s", sa.take_string().cc());
	}
	_paths.insert(p, PathInfo(p, this));
	nfo = _paths.findp(p);

	nfo->_last_tx = nfo->_last_rx = nfo->_first_tx = nfo->_first_rx
	    = nfo->_last_real = Timestamp::now();
	nfo->_active = false;
    }

    /* we want the token if we're not active
     * or if we're new *
     */
    if (!nfo->_active) {
	nfo->_last_real = Timestamp::now();
	nfo->_active = true;
	nfo->_token = true;
	_tokens++;
	nfo->_towards = nfo->other_endpoint(_sr_forwarder->ip());
    }
}
void
TokenQueue::process_forward(struct srpacket *pk) 
{
    Path p = pk->get_path();
    PathInfo *nfo = find_path_info(p);
    Timestamp now = Timestamp::now();

    IPAddress towards = p[p.size()-1];

    if (pk->seq2() > nfo->_seq) {
	if (_debug) {
	    click_chatter("seq no reset");
	}
	if (nfo->_token) {
	    _tokens--;
	}
	 nfo->reset_rx(pk->seq2(), towards);
    } else if (nfo->_seq != pk->seq2()) {
	if (_debug) {
	    StringAccum sa;
	    sa << id() << " " << now;
	    sa << " old_seq";
	    sa << " towards " << p[p.size()-1];
	    sa << " expected " << nfo->_seq;
	    click_chatter("%s", sa.take_string().cc());
	}
	return;
    }

    if (nfo->_towards != towards) {
	if (nfo->_towards.addr() < towards.addr()) {
	if (_debug) {
	    click_chatter("towards reset");
	}
	if (nfo->_token) {
	    _tokens--;
	}
	 nfo->reset_rx(pk->seq2(), towards);
	} else {
	    if (_debug) {
		StringAccum sa;
		sa << id() << " " << now;
		sa << " dup_token";
		sa << " seq " << pk->seq2();
		sa << " towards " << p[p.size()-1];
		sa << " expected " << nfo->_seq;
		click_chatter("%s", sa.take_string().cc());
	    }
	    return;
	}
    }
    /* a packet that I'm forwarding */
    if (!pk->flag(FLAG_SCHEDULE_FAKE)) {
	nfo->_last_real = now;
    }
    nfo->_packets_rx++;
    if (nfo->_packets_rx == 1) {
	nfo->_first_rx = now;
	if (_debug) {
	    StringAccum sa;
	    sa << id() << " " << now;
	    sa << " first_rx";
	    sa << " seq " << pk->seq2();
	    sa << " since_tx " << now - nfo->_last_tx;
	    click_chatter("%s", sa.take_string().cc());
	}
    } 
    nfo->_last_rx = now;
    if (pk->flag(FLAG_SCHEDULE_TOKEN)) {
	if (nfo->_token) {
	    StringAccum sa;
	    sa << id() << " " << now;
	    sa << " dup_token";
	    sa << " seq " << pk->seq2();
	    click_chatter("%s", sa.take_string().cc());
	} else {
	    nfo->_expected_rx = pk->seq() + 1;
	    nfo->_rx_token = true;
	    if (_debug) {
		StringAccum sa;
		sa << id() << " " << now;
		sa << " token_rx";
		sa << " seq " << pk->seq2();
		sa << " expected " << nfo->_expected_rx;
		sa << " packets_rx " << nfo->_packets_rx;
		sa << " rx_time " << now - nfo->_first_rx;
		click_chatter("%s", sa.take_string().cc());
	    }
	}
    }

    if (!nfo->_token && nfo->_rx_token && nfo->_expected_rx >= nfo->_packets_rx) {
	/* I have now received all the packets */
	if (_debug) {
	    StringAccum sa;
	    sa << id() << " " << now;
	    sa << " final_rx";
	    sa << " seq " << pk->seq2();
	    sa << " rx_time " << now - nfo->_first_rx;
	    click_chatter("%s", sa.take_string().cc());
	}
	nfo->_token = true;
	_tokens++;
	if (nfo->is_endpoint(_sr_forwarder->ip())) {
	    nfo->_towards = nfo->other_endpoint(nfo->_towards);
	}
    }
}

void
TokenQueue::push(int port, Packet *p_in)
{

    WritablePacket *p_out = p_in->uniqueify();
    
    if (!p_out) {
	return;
    }
    click_ether *eh = (click_ether *) p_in->data();
    if (eh->ether_type != htons(_et)) {
	if (enq(p_in)) {
	    _normal++;
	}
	goto done;
    } else if (port == 2) {
	_retransmits++;
	if (_debug) {
	    struct timeval now;
	    click_gettimeofday(&now);
	    StringAccum sa;
	    sa << id() << " " << now;
	    sa << " retransmit";
	    click_chatter("%s", sa.take_string().cc());
	}

    } else {
	click_ether *eh = (click_ether *) p_out->data();
	struct srpacket *pk = (struct srpacket *) (eh+1);
	if (port == 1) {
	    process_forward(pk);
	    if (pk->flag(FLAG_SCHEDULE_FAKE)) {
		p_out->kill();
	    } else {
		output(1).push(p_out);
	    }
	    return;
	} else if (_sr_forwarder->ip() == pk->get_link_node(0)) {
	    process_source(pk);
	}
	if (pk->flag(FLAG_SCHEDULE_FAKE)) {
	    p_out->kill();
	    return;
	}  
	pk->set_seq(0);
	pk->unset_flag(FLAG_SCHEDULE_TOKEN);
    }

    bubble_up(p_out);

 done: 
    if ((_normal > 0 || _tokens > 0  || _retransmits > 0) && !signal_active()) {
	/* there is work to be done! */
	wake_listeners();
    }
}

String
TokenQueue::static_print_stats(Element *f, void *)
{
    TokenQueue *d = (TokenQueue *) f;
    return d->print_stats();
}

String
TokenQueue::print_stats()
{
  StringAccum sa;

  Timestamp now = Timestamp::now();

  sa << " tokens " << _tokens;
  sa << " retransmits " << _retransmits;
  sa << " normal " << _normal;
  sa << " signal " << signal_active();
  sa << "\n";

  for (PathIter iter = _paths.begin(); iter; iter++) {
      const PathInfo &nfo = iter.value();
      sa << "[ " << path_to_string(nfo._p) << "] :";
      sa << " seq " << nfo._seq;
      sa << " token " << nfo._token;
      sa << " last_rx " << now - nfo._last_rx;
      sa << " packets_rx " << nfo._packets_rx;
      sa << " expected_rx " << nfo._expected_rx;
      sa << " last_tx " << (now - nfo._last_tx);
      sa << " packets_tx " << nfo._packets_tx;
      sa << "\n";
  }

  return sa.take_string();
}
String
TokenQueue::static_print_debug(Element *f, void *)
{
  StringAccum sa;
  TokenQueue *d = (TokenQueue *) f;
  sa << d->_debug << "\n";
  return sa.take_string();
}

String
TokenQueue::static_print_packet_timeout(Element *f, void *)
{
  StringAccum sa;
  TokenQueue *d = (TokenQueue *) f;
  sa << d->_max_tx_packet_ms << "\n";
  return sa.take_string();
}

String
TokenQueue::static_print_threshold(Element *f, void *)
{
  StringAccum sa;
  TokenQueue *d = (TokenQueue *) f;
  sa << d->_threshold << "\n";
  return sa.take_string();
}



int
TokenQueue::static_clear(const String &arg, Element *e,
			void *, ErrorHandler *errh) 
{
  TokenQueue *n = (TokenQueue *) e;
  bool b;

  if (!cp_bool(arg, &b))
    return errh->error("`clear' must be a boolean");

  if (b) {
    n->clear();
  }
  return 0;
}

void
TokenQueue::clear() 
{
  
  struct timeval now;
  click_gettimeofday(&now);
  Vector<Path> to_clear;
  for (PathIter iter = _paths.begin(); iter; iter++) {
      PathInfo nfo = iter.value();
      to_clear.push_back(nfo._p);
  }

  for (int x = 0; x < to_clear.size(); x++) {
      click_chatter("TokenQueue %s: removing %s\n", 
		    id().cc(),
		    path_to_string(to_clear[x]).cc());
      _paths.remove(to_clear[x]);
  }
}

int
TokenQueue::static_write_debug(const String &arg, Element *e,
			void *, ErrorHandler *errh) 
{
  TokenQueue *n = (TokenQueue *) e;
  bool b;

  if (!cp_bool(arg, &b))
    return errh->error("`debug' must be a boolean");

  n->_debug = b;
  return 0;
}


int
TokenQueue::static_write_packet_timeout(const String &arg, Element *e,
			void *, ErrorHandler *errh) 
{
  TokenQueue *n = (TokenQueue *) e;
  unsigned int b;

  if (!cp_unsigned(arg, &b))
    return errh->error("`packet_timeout' must be a unsigned int");

  return n->set_packet_timeout(errh, b);
}

int
TokenQueue::set_packet_timeout(ErrorHandler *errh, unsigned int x) 
{

  if (!x) {
    return errh->error("PACKET_TIMEOUT must not be 0");
  }
  _max_tx_packet_ms = x;
  return 0;
}

int
TokenQueue::static_write_threshold(const String &arg, Element *e,
			void *, ErrorHandler *errh) 
{
  TokenQueue *n = (TokenQueue *) e;
  unsigned int b;

  if (!cp_unsigned(arg, &b))
    return errh->error("`threshold' must be a unsigned int");

  return n->set_threshold(errh, b);
}

int
TokenQueue::set_threshold(ErrorHandler *errh, int x) 
{

  if (x < 0) {
    return errh->error("THRESHOLD must be > 0");
  }
  _threshold = x;
  return 0;
}




void 
TokenQueue::add_handlers()
{
    add_write_handler("clear", static_clear, 0);
    add_read_handler("stats", static_print_stats, 0);
    add_write_handler("debug", static_write_debug, 0);
    add_read_handler("debug", static_print_debug, 0);
    
    add_write_handler("threshold", static_write_threshold, 0);
    add_read_handler("threshold", static_print_threshold, 0);
    
    add_write_handler("packet_timeout", static_write_packet_timeout, 0);
    add_read_handler("packet_timeout", static_print_packet_timeout, 0);
    
    NotifierQueue::add_handlers();
}
// generate template instances
#include <click/bighashmap.cc>
#include <click/vector.cc>
#if EXPLICIT_TEMPLATE_INSTANCES
template class HashMap<Path, PathInfo>;
template class Vector< Vector<IPAddress> >;
#endif

CLICK_ENDDECLS
ELEMENT_REQUIRES(NotifierQueue)
EXPORT_ELEMENT(TokenQueue)
