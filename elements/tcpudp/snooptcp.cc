/*
 * snooptcp.{cc,hh} -- element implements Snoop TCP a la Balakrishnan
 * Alex Snoeren, Eddie Kohler
 *
 * Copyright (c) 1999-2000 Massachusetts Institute of Technology
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
#ifdef __linux__
# define _BSD_SOURCE
#endif
#include "snooptcp.hh"
#include <click/ipaddress.hh>
#include <click/confparse.hh>
#include <click/error.hh>
#include <click/glue.hh>
#include <click/bitvector.hh>
#ifdef DEBUG
# define DEBUG_CHATTER(args...) click_chatter(args)
#else
# define DEBUG_CHATTER(args...) /* nothing */
#endif
CLICK_DECLS

SnoopTCP::SnoopTCP()
{
}

SnoopTCP::~SnoopTCP()
{
}

int
SnoopTCP::initialize(ErrorHandler *errh)
{
  return errh->error("SnoopTCP is not ready to use");
}


inline void
SnoopTCP::SCacheEntry::clear()
{
  assert(packet);
  packet->kill();
  packet = 0;
}


SnoopTCP::PCB::PCB()
  : _head(0), _tail(0), _s_exists(0), _s_alive(0), _mh_exists(0), _mh_alive(0)
{
}

SnoopTCP::PCB::~PCB()
{
  for (int i = _tail; i != _head; i = next_i(i))
    _s_cache[i].packet->kill();
}

void
SnoopTCP::PCB::clear(bool is_s)
{
  if (is_s && _s_exists) {
    // XXX untimeout
    for (int i = _tail; i != _head; i = next_i(i))
      _s_cache[i].packet->kill();
    _head = _tail = 0;
    // XXX if (!_mh_alive) clear(false);
    _s_exists = _s_alive = false;
  } else if (!is_s && _mh_exists) {
    _mh_exists = _mh_alive = false;
  }
  // XXX remove from hashtable
}

void
SnoopTCP::PCB::initialize(bool is_s, const click_tcp *tcph, int datalen)
{
  unsigned seq = ntohl(tcph->th_seq);

  if (is_s) {
    assert(!_s_exists);
    _s_exists = _s_alive = true;
    //cs->alloc = 0;
    _s_max = seq + datalen;	// replaces cs->last_seen
    _mh_last_ack = seq - 1;
    _mh_expected_dup_acks = 0;
    _mh_dup_acks = 0;
    //cs->iss = seq;
    //cs->expected_next_ack = cs->buftail;
    //if (tcpip_hdr->ti_flags & TH_ACK)
    //cs->wl_last_ack = ack;
    /*
     * Ideally, this should be initialized to the rtt estimate
     * from another connection to the same destination, if one
     * exists. For now, choose an uninformed and conservative
     * default.
     */
    //cs->srtt = SNOOP_RTTDEFAULT;
    //cs->rttdev = SNOOP_RTTDEVDEFAULT;
    //cs->timeout_pending = 0;
  } else {
    _mh_exists = _mh_alive = true;
  }
}

void
SnoopTCP::PCB::clean(unsigned ack)
{
  //snoop_untimeout(cs);
  Timestamp last_cleaned_time;

  int i = _tail;
  while (i != _head && SEQ_LEQ(_s_cache[i].seq + _s_cache[i].size, ack)) {
    SCacheEntry &cache = _s_cache[i];
    if (cache.snd_time > last_cleaned_time)
      last_cleaned_time = cache.snd_time;
    cache.clear();
    i = next_i(i);
  }
  _tail = i;

  // if (_head != _tail) snoop_timeout(cs);
}


void
SnoopTCP::PCB::s_ack(Packet *, const click_tcp *, int)
{
  // XXX rest
}


Packet *
SnoopTCP::PCB::s_data(Packet *p, const click_tcp *tcph, int datalen)
{
  // initialize if no connection (half-duplex, or Snoop came up in the middle
  // of a connection). always mark the connection alive
  if (!_s_exists)
    initialize(true, tcph, datalen);
  else
    _s_alive = true;

  bool full = next_i(_head) == _tail;
  int entry = -1;
  bool in_sequence = false;
  bool repeat_packet = false;
  unsigned seq = ntohl(tcph->th_seq);

  // insert the packet into the cache
  if (SEQ_GEQ(seq, _s_max)) {
    // common case
    // don't save packet if over high water mark
    // (at that point cache is reserved for earlier packets)
    if (s_cache_size() >= S_CACHE_HIGHWATER) return p;
    _s_max = seq + datalen;
    entry = _head;
    _head = next_i(_head);
    in_sequence = true;

  } else {
    for (int i = _tail; i != _head; i = next_i(i))
      if (_s_cache[i].seq == seq) {
	// a repeat packet
	// always keep the repeat (Hari does). an alternative would be
	// to keep the longer of the two
	_s_cache[i].packet->kill();
	repeat_packet = true;
	entry = i;
	break;

      } else if (SEQ_GT(_s_cache[i].seq, seq)) {
	// out-of-order packet
	if (full) return p;
	// XXX memmove??
	for (int j = _tail; j != i; j = next_i(j))
	  _s_cache[prev_i(j)] = _s_cache[j];
	entry = prev_i(i);
	_tail = prev_i(_tail);
	break;
      }
  }

  // cache packet at `_cache[entry]'
  assert(entry >= 0 && 0);
  _s_cache[entry].packet = p->clone();
  _s_cache[entry].seq = seq;
  _s_cache[entry].num_rxmit = 0;
  // mark as sender retransmission if it really was (we had cached the packet
  // before), or it was before all cached packets
  if (repeat_packet || (!in_sequence && entry == _tail))
    _s_cache[entry].sender_rxmit = 1;
  else
    _s_cache[entry].sender_rxmit = 0;
  _s_cache[entry].snd_time.assign_now();
  DEBUG_CHATTER("\t%d at %d\n", seq, entry);

  // XXX if (!in_sequence) snoop_untimeout();
  // XXX snoop_timeout();

  return p;
}



void
SnoopTCP::PCB::mh_new_ack(unsigned ack)
{
  int old_tail = -1;
  if (_tail != _head && _s_cache[_tail].num_rxmit)
    old_tail = _tail;

  clean(ack);

  //if ((cs->wi_state & SNOOP_RTTFLAG) && timerisset(&sndtime))
  //snoop_rtt(cs, &sndtime);

  // check for burst loss
  if (old_tail >= 0 && s_cache_size() > 1 && next_i(old_tail) == _tail
      && _s_cache[_tail].num_rxmit == 0)
    //snoop_rexmt_pkt(cs, packet,
    //IPTOS_LOWDELAY|IPTOS_RELIABILITY|
    //IPTOS_THROUGHPUT);
    /* do nothing */;

  //cs->wi_state |= SNOOP_RTTFLAG;
  _mh_expected_dup_acks = 0;
  _mh_dup_acks = 0;
  _mh_last_ack = ack;
}

#define SNOOP_RTX_THRESH 1

Packet *
SnoopTCP::PCB::mh_dup_ack(Packet *p, const click_tcp *tcph, unsigned ack)
{
  // if snoop cache empty, nothing to do
  if (_head == _tail)
    return p;

  // window change advertisements are not semantically duplicate acks
  if (_mh_last_win != ntohs(tcph->th_win))
    return p;

  // if we don't have the packet, nothing to do
  SCacheEntry &cache = _s_cache[_tail];
  if (SEQ_LT(ack, cache.seq))
    return p;

  // if sender retransmission, nothing to do
  if (cache.sender_rxmit)
    return p;

  // otherwise, a duplicate ack we can handle
  _mh_dup_acks++;

  if (_mh_dup_acks <= SNOOP_RTX_THRESH) {
    // ignore first SNOOP_RTX_THRESH duplicate acks
    p->kill();
    return 0;

  } else if (_mh_dup_acks == SNOOP_RTX_THRESH + 1) {
    // the SNOOP_RTX_THRESHth duplicate ack: calculate how many were
    // expected, generate a retransmission
    _mh_expected_dup_acks = s_cache_size() - _mh_dup_acks;
    if (!cache.num_rxmit)
      /*snoop_rexmt_pkt(cs, packet, IPTOS_LOWDELAY)*/;
    p->kill();
    return 0;

  } else if (_mh_dup_acks < _mh_expected_dup_acks) {
    // delete expected duplicate acks
    p->kill();
    return 0;

  } else {
    // too many duplicate acks; retransmit last packet once, then start
    // letting duplicate acks pass through
    // XXX COMPAT some changes here
    if (cache.num_rxmit < 2) {
      //snoop_rexmt_pkt(cs, packet, IPTOS_LOWDELAY|IPTOS_RELIABILITY|IPTOS_THROUGHPUT);
      p->kill();
      return 0;
    } else
      return p;
  }
}

Packet *
SnoopTCP::PCB::mh_ack(Packet *p, const click_tcp *tcph, int datalen)
{
  // if server connection is dead, do nothing
  if (!_s_exists)
    return p;

  unsigned ack = ntohl(tcph->th_ack);
  if (SEQ_GT(ack, _mh_last_ack))
    // new ack
    mh_new_ack(ack);

  else if (ack == _mh_last_ack && datalen == 0)
    // duplicate ack w/o data
    // (duplicate acks with data are not semantically "duplicate acks")
    p = mh_dup_ack(p, tcph, ack);

  else if (SEQ_LT(ack, _mh_last_ack))
    // spurious ack: ignore
    return p;

  _mh_last_win = ntohs(tcph->th_win);
  return p;
}


void
SnoopTCP::PCB::mh_data(Packet *, const click_tcp *tcph, int datalen)
{
  // initialize connection (starting up snoop in the middle of a connection)
  // or mark it alive
  if (datalen) {
    if (!_mh_exists)
      initialize(false, tcph, datalen);
    else
      _mh_alive = true;
  }
  // XXX rest
}



SnoopTCP::PCB *
SnoopTCP::find(unsigned s_ip, unsigned short s_port,
	       unsigned int mh_ip, unsigned short mh_port, bool create)
{
  IPFlowID q(s_ip, s_port, mh_ip, mh_port);

  if (PCB **pcbp = _map.findp(q))
    return *pcbp;
  else if (create) {
    PCB *pcb = new PCB();
    if (pcb) _map.insert(q, pcb);
    return pcb;
  } else
    return 0;
}

Packet *
SnoopTCP::handle_packet(int port, Packet *p)
{
  const click_ip *iph = p->ip_header();
  if (p->length() < 40 || iph->ip_p != IPPROTO_TCP) {
    DEBUG_CHATTER("Non TCP");
    // ignore non-TCP traffic
    return p;
  }

  const click_tcp *tcph = p->tcp_header();
  int header_len = (iph->ip_hl << 2) + (tcph->th_off << 2);
  int datalen = p->length() - header_len;

  // get or create corresponding PCB
  // don't create a PCB for packets w/o data
  PCB *pcb;
  if (port == 0)
    pcb = find(iph->ip_src.s_addr, tcph->th_sport,
	       iph->ip_dst.s_addr, tcph->th_dport, datalen > 0);
  else
    pcb = find(iph->ip_dst.s_addr, tcph->th_dport,
	       iph->ip_src.s_addr, tcph->th_sport, datalen > 0);
  if (!pcb)
    // out of space, could not create PCB
    return p;

  // SYN flag: initialize that side of the connection
  if (tcph->th_flags & TH_SYN) {
    DEBUG_CHATTER("SYN packet");
    pcb->clear(port == 0);
    pcb->initialize(port == 0, tcph, datalen);
    return p;
  }

  // FIN or RST: kill that side of the connection
  if (tcph->th_flags & (TH_FIN | TH_RST)) {
    pcb->clear(port == 0);
    return p;
  }

  if (port == 0) {
    if (tcph->th_flags & TH_ACK)
      pcb->s_ack(p, tcph, datalen);
    if (datalen > 0)
      p = pcb->s_data(p, tcph, datalen);

  } else {
    if (tcph->th_flags & TH_ACK)
      p = pcb->mh_ack(p, tcph, datalen);
    if (datalen > 0)
      pcb->mh_data(p, tcph, datalen);
  }

  return p;
}

void
SnoopTCP::push(int port, Packet *p)
{
  p = handle_packet(port, p);
  if (p) output(port).push(p);
}

Packet *
SnoopTCP::pull(int port)
{
  Packet *p = input(port).pull();
  if (p)
    p = handle_packet(port, p);
  return p;
}


#if 0
Packet *
SnoopTCP::PCB::add_data(Packet *p, unsigned th_seq)
{
  bool full = next_i(_head) == _tail;
  int entry = -1;

  if (SEQ_GT(th_seq, _max)) {
    // New packet with higher seqno - common case
    if (full) {
      // skip if buffer is full
      DEBUG_CHATTER("buffer full");
      return p;
    }
    _max = th_seq;
    entry = _expected_ack = _head;
    _head = next_i(_head);

  } else if (SEQ_LT(th_seq, _cache[_tail].seq)) {
    if (SEQ_LT(th_seq, _una)) {
      // already acked - don't cache
      DEBUG_CHATTER("\tway out-of-order pkt %d, lastack %d\n",
		    th_seq, _una);
      return p;
    }

    // new packet earlier than anything cached
    if (full) {
      // skip if buffer is full
      DEBUG_CHATTER("buffer full");
      return p;
    }
    _tail = prev_i(_tail);
    entry = _tail;

  } else if (_tail == _head)
    // nothing has been cached and we got a spurious packet
    return p;

  else
    // somewhere in the middle
    for (int i = _tail; i != _head; i = next_i(i)) {
      if (_cache[i].seq == th_seq) {
	// either a repeat packet or a fragment thereof
	DEBUG_CHATTER("have pkt %d at %d", th_seq, i);
	if (_cache[i].packet->length() <= p->length()) {
	  // replace fragment/packet with new one
	  _cache[i].packet->kill();
	  _cache[i].packet = p->clone();
	}
	_cache[i].num_rxmit = 0;
	_cache[i].sender_rxmit = 1;
	//microtime(&(packet->snd_time));
	return p;

      } else if (SEQ_GT(_cache[i].seq, th_seq)) {
	// insert new packet in the middle
	if (full) {
	  // skip if buffer is full
	  DEBUG_CHATTER("buffer full");
	  return p;
	}
	for (int j = _tail; j != i; j = next_i(j))
	  _cache[prev_i(j)] = _cache[j];
	entry = i;
	_tail = prev_i(_tail);
	DEBUG_CHATTER("\tcache reorg; pkt %d, head %d, tail %d",
		      th_seq, _head, _tail);
	break;
      }
    }

  // Cache packet at `_cache[entry]'
  //microtime(&(packet->snd_time));
  assert(entry >= 0);
  p->use();
  _cache[entry].packet = p;
  _cache[entry].seq = th_seq;
  _cache[entry].num_rxmit = 0;
  _cache[entry].sender_rxmit = 0;
  DEBUG_CHATTER("\t%d at %d\n", th_seq, entry);

  if (SEQ_LT(th_seq, _max)) {
    // out-of-order
    DEBUG_CHATTER("\tpkt %x out of order, last %x\n", th_seq, _max);
    if (_tail == entry) {
      _cache[entry].sender_rxmit = 1;
      _cache[entry].num_rxmit = 0;
    }
    _expected_ack = _tail;
  }

  return p;
}
#endif

#if 0
Packet *
SnoopTCP::PCB::add_ack(Packet *p, unsigned th_ack, int data_len,
		       unsigned short win, SnoopTCP *snp)
{

  DEBUG_CHATTER("ack %d, expect %d, dacks %d, seen %d dups\tcached %d-%d",
		th_ack, _expected_ack, _expected_dup_acks,
		_dup_acks, _cache[_tail].seq, _cache[prev_i(_head)].seq);

  if (SEQ_GT(th_ack, _una)) {
    // new ack
    DEBUG_CHATTER("new ack %d", th_ack);
    // XXX update RTT ??
    int i = _tail;
    while (i != _head && SEQ_LT(_cache[i].seq, th_ack)) {
      _cache[i].packet->kill();
      i = next_i(i);
    }
    _tail = i;
    _una = th_ack;
    _dup_acks = 0;
    _last_win = win;
    return p;

  } else if (SEQ_LT(th_ack, _una)) {
    // out-of-order ack; just forward it on
    DEBUG_CHATTER("spurious ack %d", th_ack);
    return p;

  } else {

    if (_last_win != win || data_len > 0) {
      // not a duplicate ack -- window change ad or piggyback ACK
      DEBUG_CHATTER("data/window ad");
      _last_win = win;
      return p;
    }
    // duplicate ack
    // first look for the appropriate cache entry
    int entry = -1;
    for (int i = _tail; i != _head; i = next_i(i))
      if (_cache[i].seq == th_ack) {
	entry = i;
	break;
      }
    // just forward the duplicate ack if there is no such packet, or the
    // sender has already retransmitted it
    if (entry < 0 || _cache[entry].sender_rxmit)
      return p;
    // otherwise, check if we're expecting it
    if (_expected_dup_acks > 0) {
      --_expected_dup_acks;
      DEBUG_CHATTER("expected, discarding");
      // discard if not piggybacked
      if (data_len)
	return p;
      else
	return 0;
    } else if (!_expected_dup_acks) {
      // Compute number of expected dups
      _expected_dup_acks = _head - _expected_ack;
      if (_expected_dup_acks < 0)
	_expected_dup_acks += SNOOP_MAX_BUF;
      _expected_dup_acks--;
      _expected_ack = next_i(_tail);

      DEBUG_CHATTER(" ack %d expect %d more\n",
		    th_ack, _expected_dup_acks);

      // Retransmit
      _dup_acks++;
      _cache[entry].num_rxmit++;
      click_chatter("dup ack %d, retransmitting", _una);
      snp->output(2).push(_cache[entry].packet);

      // Squelch packet if no data content
      if (data_len)
	return p;
      else
	return 0;
    } else {
      DEBUG_CHATTER("Help! Inconsistent state");
      return p;
    }
  }
}
#endif

CLICK_ENDDECLS
ELEMENT_REQUIRES(false)
EXPORT_ELEMENT(SnoopTCP)
