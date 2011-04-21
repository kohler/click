/*
 * WebGen.{cc,hh} -- toy TCP implementation
 * Robert Morris
 *
 * Copyright (c) 1999-2001 Massachusetts Institute of Technology
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
#include "webgen.hh"
#include <clicknet/tcp.h>
#include <clicknet/ip.h>
#include <click/ipaddress.hh>
#include <click/args.hh>
#include <click/error.hh>
#include <click/glue.hh>
CLICK_DECLS

static int
timestamp_diff(const Timestamp &t1, const Timestamp &t2)
{
    return (t1.sec() - t2.sec()) * 1000000 + (t1.usec() - t2.usec());
}

WebGen::WebGen()
  : _timer(this)
{

  cbfree = NULL;
  rexmit_head = NULL;
  rexmit_tail = NULL;

  memset (cbhash, 0, sizeof (cbhash));
  memset (&perfcnt, 0, sizeof (perfcnt));
}

WebGen::~WebGen()
{
}

int
WebGen::configure (Vector<String> &conf, ErrorHandler *errh)
{
  int ret;
  int cps;

  ret = Args(conf, this, errh)
      .read_mp("PREFIX", IPPrefixArg(true), _src_prefix, _mask)
      .read_mp("DST", _dst)
      .read_mp("RATE", cps)
      .complete();

  start_interval = 1000000 / cps;
  return ret;
}

IPAddress
WebGen::pick_src ()
{
  uint32_t x;
  uint32_t mask = (uint32_t) _mask;

  x = (click_random() & ~mask) | ((uint32_t) _src_prefix & mask);

  return IPAddress (x);
}

int
WebGen::connhash (unsigned src, unsigned short sport)
{
  return (src ^ sport) & htmask;
}

int
WebGen::initialize (ErrorHandler *)
{
  _timer.initialize (this);
  _timer.schedule_now ();

  int ncbs = 2 * (resend_dt / start_interval) * resend_max;
  for (int i = 0; i < ncbs; i++) {
    CB *cb = new CB;
    if (!cb) {
      click_chatter ("Not enough memory for CBs\n");
      return ENOMEM;
    }
    cb->add_to_list (&cbfree);
  }
  click_chatter ("Allocated %d CBs\n", ncbs);

  Timestamp now = Timestamp::now();
  perf_tv = now;
  start_tv = now;

  rexmit_head = new CB;
  rexmit_tail = new CB;
  if (!rexmit_head || !rexmit_tail) {
    click_chatter ("Not enough memory for dummy elements\n");
    return ENOMEM;
  }
  rexmit_head->rexmit_next = rexmit_tail;
  rexmit_head->rexmit_prev = NULL;
  rexmit_tail->rexmit_next = NULL;
  rexmit_tail->rexmit_prev = rexmit_head;

  return 0;
}

void
WebGen::cleanup (CleanupStage stage)
{
  int i = 0;
  CB *c = cbfree;

  do {
    while (c) {
      CB *tc = c;
      c = tc->next;
      delete tc;
    }

    c = cbhash[i++];
  } while (i <= htsize);

  delete rexmit_head;
  delete rexmit_tail;

  if (stage >= CLEANUP_INITIALIZED)
    do_perf_stats ();
}

void
WebGen::recycle (CB *cb)
{
  cb->rexmit_unlink ();
  cb->remove_from_list ();
  cb->add_to_list (&cbfree);
}

void
WebGen::do_perf_stats ()
{
  Timestamp now = Timestamp::now();

  //double td = ((double) perf_diff) / 1000000.0;
  //double ips = perfcnt.initiated / td;
  //double cps = perfcnt.completed / td;
  //double tps = perfcnt.timeout / td;
  //double rps = perfcnt.reset / td;
  //click_chatter ("Init: %5d  Comp: %5d  Tmo: %5d  RST: %5d\n",
  //               (int) ips, (int) cps, (int) tps, (int) rps);
  click_chatter ("init: %d  comp: %5d  tmo: %5d  rst: %5d\n",
                 perfcnt.initiated, perfcnt.completed,
                 perfcnt.timeout, perfcnt.reset);
  perf_tv = now;
  memset (&perfcnt, 0, sizeof (perfcnt));
}

void
WebGen::run_timer (Timer *)
{
  CB *cb;
  Timestamp now = Timestamp::now();

  while (timestamp_diff(now, start_tv) > start_interval) {
    start_tv += Timestamp::make_usec(start_interval);
    cb = cbfree;

    if (cb) {
      cb->remove_from_list ();
      cb->reset (pick_src ());

      int hv = connhash (cb->_src, cb->_sport);
      cb->add_to_list (&cbhash[hv]);
      tcp_send(cb, 0);
      perfcnt.initiated++;
    } else {
      click_chatter ("out of available CBs\n");
    }
  }

  CB *lrxcb = rexmit_tail->rexmit_prev;
  do {
    cb = rexmit_head->rexmit_next;

    if (cb == rexmit_tail)
      break;

    if (timestamp_diff(now, cb->last_send) > resend_dt) {
      if (cb->_resends++ > resend_max) {
	perfcnt.timeout++;
	recycle (cb);
      } else {
	tcp_send (cb, 0);
      }
    } else {
      break;
    }
  } while (cb != lrxcb);

  if (timestamp_diff(now, perf_tv) > perf_dt)
    do_perf_stats ();

  _timer.schedule_after_msec(1);
}

WebGen::CB *
WebGen::find_cb (unsigned src, unsigned short sport, unsigned short dport)
{
  int hv = connhash (src, sport);
  CB *cb = cbhash[hv];

  while (cb) {
    if (sport == cb->_sport &&
	dport == cb->_dport &&
	src == (uint32_t) cb->_src)
      return cb;
    cb = cb->next;
  }
  return NULL;
}

Packet *
WebGen::simple_action (Packet *p)
{
  tcp_input (p);
  return NULL;
}

void
WebGen::tcp_input (Packet *p)
{
  unsigned seq, ack;
  unsigned plen = p->length ();

  if (plen < sizeof(click_ip) + sizeof(click_tcp))
    return;

  click_ip *ip = (click_ip *) p->data();
  unsigned iplen = ntohs(ip->ip_len);
  unsigned hlen = ip->ip_hl << 2;
  if (hlen < sizeof(click_ip) || hlen > iplen || iplen > plen) {
    p->kill();
    return;
  }

  click_tcp *th = (click_tcp *) (((char *)ip) + hlen);
  unsigned off = th->th_off << 2;
  int dlen = iplen - hlen - off;

  CB *cb = find_cb(ip->ip_dst.s_addr, th->th_dport, th->th_sport);
  if (cb == 0) {
    unsigned plen = sizeof (click_ip) + sizeof (click_tcp);

    WritablePacket *wp = fixup_packet (p, plen);
    tcp_output (wp,
		ip->ip_dst, th->th_dport,
		ip->ip_src, th->th_sport,
		th->th_ack, th->th_seq, TH_RST,
		NULL, 0);
    return;
  }

  seq = ntohl(th->th_seq);
  ack = ntohl(th->th_ack);

  if ((th->th_flags & (TH_ACK|TH_RST)) == TH_ACK &&
     ack == cb->_iss + 1 &&
     cb->_connected == 0) {
    cb->_snd_nxt = cb->_iss + 1;
    cb->_snd_una = cb->_snd_nxt;
    cb->_irs = seq;
    cb->_rcv_nxt = cb->_irs + 1;
    cb->_connected = 1;
    cb->_do_send = 1;
    //click_chatter("WebGen connected %d %d",
    //              ntohs(cb->_sport),
    //              ntohs(cb->_dport));
  } else if (dlen > 0) {
    cb->_do_send = 1;
    if (seq + dlen > cb->_rcv_nxt) {
      //click_chatter("_rcv_nxt %d + %d -> %d\n", cb->_rcv_nxt, dlen, seq+dlen);
      cb->_rcv_nxt = seq + dlen;
    }
  }

  if (th->th_flags & TH_ACK) {
    if (ack > cb->_snd_una) {
      cb->_snd_una = ack;
    }
  }

  if ((th->th_flags & TH_FIN) &&
      seq + dlen == cb->_rcv_nxt &&
      cb->_got_fin == 0) {
    cb->_got_fin = 1;
    cb->_rcv_nxt += 1;
    cb->_do_send = 1;
  }

  if (th->th_flags & TH_RST) {
    // click_chatter("RST %d %d", ntohs (th->th_sport), ntohs (th->th_dport));
    p->kill ();
    recycle (cb);
    perfcnt.reset++;
    return;
  }

  tcp_send (cb, p);

  if (cb->_closed)
    recycle (cb);
}

WritablePacket *
WebGen::fixup_packet (Packet *xp, unsigned plen)
{
  unsigned int headroom = 34;
  WritablePacket *p;

  if (xp == 0 ||
      xp->shared () ||
      xp->headroom () < headroom ||
      xp->length () + xp->tailroom() < plen) {
    if (xp)
      xp->kill ();
    p = Packet::make (headroom, NULL, plen, 0);
  } else {
    p = xp->uniqueify ();
    if (p->length () < plen)
      p = p->put (plen - p->length ());
    else if (p->length () > plen)
      p->take (p->length () - plen);
  }

  return p;
}

// Send a suitable TCP packet.
// xp is a candidate packet buffer, to be re-used or freed.
void
WebGen::tcp_send (CB *cb, Packet *xp)
{
  int paylen;
  unsigned int plen;
  unsigned int seq;
  WritablePacket *p = 0;

//click_chatter ("connected %d snd_una %d iss %d sndlen %d\n",
//	cb->_connected, cb->_snd_una, cb->_iss, cb->sndlen);
  if (cb->_connected && cb->_snd_una - cb->_iss - 1 < cb->sndlen) {
    paylen = cb->sndlen;
    seq = cb->_iss + 1;
    cb->_snd_nxt = seq + paylen;
  } else {
    paylen = 0;
    seq = cb->_snd_nxt + cb->_sent_fin;
  }
  plen = sizeof(click_ip) + sizeof(click_tcp) + paylen;

  cb->rexmit_update (rexmit_tail);
//click_chatter ("dosend %d paylen %d snd_nxt %d seq %d sfin %d\n", cb->_do_send, paylen, plen, cb->_snd_nxt, seq, cb->_sent_fin);
  if (cb->_connected == 1 && cb->_do_send == 0 && paylen == 0) {
    if (xp)
      xp->kill ();
    return;
  }
  cb->_do_send = 0;

  p = fixup_packet (xp, plen);

  char flags = 0;
  int ack = 0;

  if (cb->_connected == 0) {
    flags = TH_SYN;
  } else {
    flags = TH_ACK;
    if (paylen)
      flags |= TH_PUSH | TH_FIN;
    if (cb->_got_fin)
      flags |= TH_FIN;
    ack = cb->_rcv_nxt;
  }

  if (flags & TH_FIN)
    cb->_sent_fin = 1;

  if (cb->_sent_fin && cb->_got_fin) {
    // Other side has sent the FIN too -- we ack and close.
    cb->_closed = 1;
    perfcnt.completed++;
  }

  tcp_output (p, cb->_src, cb->_sport, _dst, cb->_dport,
	      htonl (seq), htonl (ack), flags,
	      cb->sndbuf, paylen);
}

void
WebGen::tcp_output (WritablePacket *p,
	IPAddress src, unsigned short sport,
	IPAddress dst, unsigned short dport,
	int seq, int ack, char tcpflags,
	char *payload, int paylen)
{
  unsigned plen = p->length ();

  click_ip *ip = (click_ip *) p->data ();
  ip->ip_v = 4;
  ip->ip_hl = sizeof (click_ip) >> 2;
  ip->ip_id = htons (_id.fetch_and_add (1));
  ip->ip_p = 6;
  ip->ip_src = src;
  ip->ip_dst = dst;
  ip->ip_tos = 0;
  ip->ip_off = 0;
  ip->ip_ttl = 250;
  p->set_dst_ip_anno (IPAddress (ip->ip_dst));
  p->set_ip_header (ip, sizeof (click_ip));

  click_tcp *th = (click_tcp *) (ip + 1);

  memset (th, '\0', sizeof(*th));

  if (paylen > 0)
    memcpy (th + 1, payload, paylen);

  th->th_sport = sport;
  th->th_dport = dport;
  th->th_seq = seq;
  th->th_ack = ack;
  th->th_off = sizeof (click_tcp) >> 2;
  th->th_flags = tcpflags;
  th->th_win = htons (60*1024);

  char itmp[9];
  memcpy (itmp, ip, 9);
  memset (ip, '\0', 9);
  ip->ip_sum = 0;
  ip->ip_len = htons (plen - 20);

  th->th_sum = 0;
  th->th_sum = click_in_cksum ((unsigned char *) ip, plen);

  memcpy (ip, itmp, 9);
  ip->ip_len = htons (plen);

  ip->ip_sum = 0;
  ip->ip_sum = click_in_cksum ((unsigned char *) ip, sizeof (click_ip));

  output (0).push (p);
}

WebGen::CB::CB ()
{
  next = NULL;
  pprev = NULL;

  rexmit_next = NULL;
  rexmit_prev = NULL;

  last_send.assign_now();
}

void
WebGen::CB::reset (IPAddress src)
{
  _src = src;
  _dport = htons (80);
  _iss = click_random() & 0x0fffffff;
  _irs = 0;
  _snd_nxt = _iss;
  _snd_una = _iss;
  _sport = htons (1024 + (click_random() % 60000));
  _do_send = 0;
  _connected = 0;
  _got_fin = 0;
  _sent_fin = 0;
  _closed = 0;
  _resends = 0;

  int dir = click_random(0, 9);
  int file = click_random(0, 8);	// 0 .. 8 exist
  int c = click_random(0, 2);		// 0 .. 3 exist
  sprintf (sndbuf, "GET /spec/%d/%d-%d-%d HTTP/1.0\r\n\r\n",
           dir, dir, c, file);
  sndlen = strlen (sndbuf);
}

void
WebGen::CB::remove_from_list ()
{
  if (next)
    next->pprev = pprev;
  if (pprev)
    *pprev = next;

  next = NULL;
  pprev = NULL;
}

void
WebGen::CB::add_to_list (CB **phead)
{
  assert (!next && !pprev);

  next = *phead;
  if (next)
    next->pprev = &next;

  pprev = phead;
  *phead = this;
}

void
WebGen::CB::rexmit_unlink ()
{
  if (rexmit_next)
    rexmit_next->rexmit_prev = rexmit_prev;
  if (rexmit_prev)
    rexmit_prev->rexmit_next = rexmit_next;

  rexmit_next = NULL;
  rexmit_prev = NULL;
}

void
WebGen::CB::rexmit_update (CB *tail)
{
  last_send.assign_now();

  rexmit_unlink ();

  rexmit_next = tail;
  rexmit_prev = tail->rexmit_prev;

  rexmit_prev->rexmit_next = this;
  rexmit_next->rexmit_prev = this;
}

CLICK_ENDDECLS
EXPORT_ELEMENT(WebGen)
