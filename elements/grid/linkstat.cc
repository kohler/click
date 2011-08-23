/*
 * linkstat.{cc,hh} -- track per-link delivery rates.
 * Douglas S. J. De Couto
 *
 * Copyright (c) 1999-2003 Massachusetts Institute of Technology
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
#include <click/args.hh>
#include <clicknet/ether.h>
#include <click/error.hh>
#include <click/glue.hh>
#include <click/timer.hh>
#include <click/straccum.hh>
#include <elements/grid/grid.hh>
#include <elements/grid/linkstat.hh>
#include <elements/grid/timeutils.hh>
CLICK_DECLS

LinkStat::LinkStat()
  : _window(100), _tau(10000), _period(1000),
  _probe_size(1000), _seq(0), _send_timer(0),
  _use_proto2(false)
{
}

LinkStat::~LinkStat()
{
}


int
LinkStat::configure(Vector<String> &conf, ErrorHandler *errh)
{
  int res = Args(conf, this, errh)
      .read("WINDOW", _window)
      .read("ETH", _eth)
      .read("PERIOD", _period)
      .read("TAU", _tau)
      .read("SIZE", _probe_size)
      .read("USE_SECOND_PROTO", _use_proto2)
      .complete();
  if (res < 0)
    return res;

  unsigned min_sz = sizeof(click_ether) + link_probe::size;
  if (_probe_size < min_sz)
    return errh->error("Specified packet size is less than the minimum probe size of %u",
		       min_sz);

  return res;
}

void
LinkStat::send_hook()
{
  WritablePacket *p = Packet::make(_probe_size + 2); // +2 for alignment
  if (p == 0) {
    click_chatter("LinkStat %s: cannot make packet!", name().c_str());
    return;
  }
  ASSERT_4ALIGNED(p->data());
  p->pull(2);
  memset(p->data(), 0, p->length());

  p->set_timestamp_anno(Timestamp::now());

  // fill in ethernet header
  click_ether *eh = (click_ether *) p->data();
  memset(eh->ether_dhost, 0xff, 6); // broadcast
  eh->ether_type = htons(_use_proto2 ? ETHERTYPE_LINKSTAT2 : ETHERTYPE_LINKSTAT);
  memcpy(eh->ether_shost, _eth.data(), 6);

  // calculate number of entries
  unsigned min_packet_sz = sizeof(click_ether) + link_probe::size;
  unsigned max_entries = (_probe_size - min_packet_sz) / link_entry::size;
  static bool size_warning = false;
  if (!size_warning && max_entries < (unsigned) _bcast_stats.size()) {
    size_warning = true;
    click_chatter("LinkStat %s: WARNING, probe packet is too small to contain all link stats", name().c_str());
  }
  unsigned num_entries = max_entries < (unsigned) _bcast_stats.size() ? max_entries : _bcast_stats.size();

  // build packet
  link_probe lp(_seq, _period, num_entries, _tau, _probe_size);
  _seq++;
  unsigned char *d = p->data() + sizeof(click_ether);
  d += lp.write(d);

  for (ProbeMap::const_iterator i = _bcast_stats.begin();
       i.live() && num_entries > 0;
       num_entries--, i++) {
    const probe_list_t &val = i.value();
    if (val.probes.size() == 0) {
      num_entries++;
      continue;
    }
    unsigned n = count_rx(&val);
    if (n > 0xFFff)
      click_chatter("LinkStat %s: WARNING, overflow in number of probes received from %s", name().c_str(), val.eth.unparse().c_str());
    link_entry le(val.eth, n & 0xFFff);
    d += le.write(d);
  }

  link_probe::update_cksum(p->data() + sizeof(click_ether));

  unsigned max_jitter = _period / 10;
  unsigned j = click_random(0, max_jitter * 2);
  _send_timer->reschedule_after_msec(_period + j - max_jitter);

  checked_output_push(0, p);
}

unsigned int
LinkStat::count_rx(const probe_list_t *pl)
{
  if (!pl)
    return 0;

  Timestamp earliest = Timestamp::now() - Timestamp::make_msec(_tau);

  int num = 0;
  for (int i = pl->probes.size() - 1; i >= 0; i--) {
    if (pl->probes[i].when >= earliest)
      num++;
    else
      break;
  }
  return num;
}

unsigned int
LinkStat::count_rx(const EtherAddress &eth)
{
  probe_list_t *pl = _bcast_stats.findp(eth);
  if (pl)
    return count_rx(pl);
  else
    return 0;
}

int
LinkStat::initialize(ErrorHandler *errh)
{
  if (noutputs() > 0) {
    if (!_eth)
      return errh->error("Source Ethernet address must be specified to send probes");
    _send_timer = new Timer(static_send_hook, this);
    _send_timer->initialize(this);
    _send_timer->schedule_after_sec(1);
  }
  return 0;
}



Packet *
LinkStat::simple_action(Packet *p)
{
  unsigned min_sz = sizeof(click_ether) + link_probe::size;
  if (p->length() < min_sz) {
    click_chatter("LinkStat %s: packet is too small", name().c_str());
    p->kill();
    return 0;
  }

  click_ether *eh = (click_ether *) p->data();

  if (ntohs(eh->ether_type) != (_use_proto2 ? ETHERTYPE_LINKSTAT2 : ETHERTYPE_LINKSTAT)) {
    click_chatter("LinkStat %s: got non-LinkStat packet type", name().c_str());
    p->kill();
    return 0;
  }

  link_probe lp(p->data() + sizeof(click_ether));
  if (link_probe::calc_cksum(p->data() + sizeof(click_ether)) != 0) {
    click_chatter("LinkStat %s: bad checksum from %s", name().c_str(), EtherAddress(eh->ether_shost).unparse().c_str());
    p->kill();
    return 0;
  }

  if (p->length() < lp.psz)
    click_chatter("LinkStat %s: packet is smaller (%d) than it claims (%u)",
		  name().c_str(), p->length(), lp.psz);

  add_bcast_stat(EtherAddress(eh->ether_shost), lp);

  // look in received packet for info about our outgoing link
  Timestamp now = Timestamp::now();
  unsigned int max_entries = (p->length() - sizeof(*eh) - link_probe::size) / link_entry::size;
  unsigned int num_entries = lp.num_links;
  if (num_entries > max_entries) {
    click_chatter("LinkStat %s: WARNING, probe packet from %s contains fewer link entries (at most %u) than claimed (%u)",
		  name().c_str(), EtherAddress(eh->ether_shost).unparse().c_str(), max_entries, num_entries);
    num_entries = max_entries;
  }

  const unsigned char *d = p->data() + sizeof(click_ether) + link_probe::size;
  for (unsigned i = 0; i < num_entries; i++, d += link_entry::size) {
    link_entry le(d);
    if (le.eth == _eth) {
      _rev_bcast_stats.insert(EtherAddress(eh->ether_shost), outgoing_link_entry_t(le, now, lp.tau));
      break;
    }
  }

  p->kill();
  return 0;
}

bool
LinkStat::get_forward_rate(const EtherAddress &eth, unsigned int *r,
			   unsigned int *tau, Timestamp *t)
{
  outgoing_link_entry_t *ol = _rev_bcast_stats.findp(eth);
  if (!ol)
    return false;

  if (_period == 0)
    return false;

  unsigned num_expected = ol->tau / _period;
  unsigned num_received = ol->num_rx;

  // will happen if our send period is greater than the the remote
  // host's averaging period
  if (num_expected == 0)
    return false;

  unsigned pct = 100 * num_received / num_expected;
  if (pct > 100)
    pct = 100;
  *r = pct;
  *tau = ol->tau;
  *t = ol->received_at;

  return true;
}

bool
LinkStat::get_reverse_rate(const EtherAddress &eth, unsigned int *r,
			   unsigned int *tau)
{
  probe_list_t *pl = _bcast_stats.findp(eth);
  if (!pl)
    return false;

  if (pl->period == 0)
    return false;

  unsigned num_expected = _tau / pl->period;
  unsigned num_received = count_rx(eth);

  // will happen if our averaging period is less than the remote
  // host's sending rate.
  if (num_expected == 0)
    return false;

  unsigned pct = 100 * num_received / num_expected;
  if (pct > 100)
    pct = 100;
  *r = pct;
  *tau = _tau;

  return true;
}

void
LinkStat::add_bcast_stat(const EtherAddress &eth, const link_probe &lp)
{
  Timestamp now = Timestamp::now();
  probe_t probe(now, lp.seq_no);

  unsigned int new_period = lp.period;

  probe_list_t *l = _bcast_stats.findp(eth);
  if (!l) {
    probe_list_t l2(eth, new_period, lp.tau);
    _bcast_stats.insert(eth, l2);
    l = _bcast_stats.findp(eth);
  }
  else if (l->period != new_period) {
    click_chatter("LinkStat %s: %s has changed its link probe period from %u to %u; clearing probe info\n",
		  name().c_str(), eth.unparse().c_str(), l->period, new_period);
    l->probes.clear();
    l->period = new_period;
    return;
  }

  l->probes.push_back(probe);

  /* only keep stats for last _window *unique* sequence numbers */
  while ((unsigned) l->probes.size() > _window)
    l->probes.pop_front();
}

String
LinkStat::read_window(Element *xf, void *)
{
  LinkStat *f = (LinkStat *) xf;
  return String(f->_window) + "\n";
}

String
LinkStat::read_period(Element *xf, void *)
{
  LinkStat *f = (LinkStat *) xf;
  return String(f->_period) + "\n";
}

String
LinkStat::read_tau(Element *xf, void *)
{
  LinkStat *f = (LinkStat *) xf;
  return String(f->_tau) + "\n";
}

String
LinkStat::read_bcast_stats(Element *xf, void *)
{
  LinkStat *e = (LinkStat *) xf;

  typedef HashMap<EtherAddress, bool> EthMap;
  EthMap eth_addrs;

  for (ProbeMap::const_iterator i = e->_bcast_stats.begin(); i.live(); i++)
    eth_addrs.insert(i.key(), true);
  for (ReverseProbeMap::const_iterator i = e->_rev_bcast_stats.begin(); i.live(); i++)
    eth_addrs.insert(i.key(), true);

  Timestamp now = Timestamp::now();

  StringAccum sa;
  for (EthMap::const_iterator i = eth_addrs.begin(); i.live(); i++) {
    const EtherAddress &eth = i.key();

    probe_list_t *pl = e->_bcast_stats.findp(eth);
    outgoing_link_entry_t *ol = e->_rev_bcast_stats.findp(eth);

    sa << eth << ' ';

    sa << "fwd ";
    if (ol) {
      Timestamp age = now - ol->received_at;
      sa << "age=" << age << " tau=" << ol->tau << " num_rx=" << (unsigned) ol->num_rx
	 << " period=" << e->_period << " pct=" << calc_pct(ol->tau, e->_period, ol->num_rx);
    }
    else
      sa << "age=-1 tau=-1 num_rx=-1 period=-1 pct=-1";

    sa << " -- rev ";
    if (pl) {
      unsigned num_rx = e->count_rx(pl);
      sa << "tau=" << e->_tau << " num_rx=" << num_rx << " period=" << pl->period
	 << " pct=" << calc_pct(e->_tau, pl->period, num_rx);
    }
    else
      sa << "tau=-1 num_rx=-1 period=-1 pct=-1";

    sa << '\n';
  }

  return sa.take_string();
}

unsigned int
LinkStat::calc_pct(unsigned tau, unsigned period, unsigned num_rx)
{
  if (period == 0)
    return 0;
  unsigned num_expected = tau / period;
  if (num_expected == 0)
    return 0;
  return 100 * num_rx / num_expected;
}


int
LinkStat::write_window(const String &arg, Element *el,
		       void *, ErrorHandler *errh)
{
  LinkStat *e = (LinkStat *) el;
  if (!IntArg().parse(arg, e->_window))
    return errh->error("window must be >= 0");
  return 0;
}

int
LinkStat::write_period(const String &arg, Element *el,
		       void *, ErrorHandler *errh)
{
  LinkStat *e = (LinkStat *) el;
  if (!IntArg().parse(arg, e->_period))
    return errh->error("period must be >= 0");

  return 0;
}

int
LinkStat::write_tau(const String &arg, Element *el,
		       void *, ErrorHandler *errh)
{
  LinkStat *e = (LinkStat *) el;
  if (!IntArg().parse(arg, e->_tau))
    return errh->error("tau must be >= 0");
  return 0;
}


void
LinkStat::add_handlers()
{
  add_read_handler("bcast_stats", read_bcast_stats, 0);

  add_read_handler("window", read_window, 0);
  add_read_handler("tau", read_tau, 0);
  add_read_handler("period", read_period, 0);

  add_write_handler("window", write_window, 0);
  add_write_handler("tau", write_tau, 0);
  add_write_handler("period", write_period, 0);
}


LinkStat::link_probe::link_probe(const unsigned char *d)
  : seq_no(uint_at(d + 0)), period(uint_at(d + 4)), num_links(uint_at(d + 8)),
  tau(uint_at(d + 12)), cksum(ushort_at(d + 16)), psz(ushort_at(d + 18))
{
}

int
LinkStat::link_probe::write(unsigned char *d) const
{
  write_uint_at(d + 0, seq_no);
  write_uint_at(d + 4, period);
  write_uint_at(d + 8, num_links);
  write_uint_at(d + 12, tau);
  write_ushort_at(d + 16, 0); // cksum will be filled in later -- we don't know what the link_entry data is yet
  write_ushort_at(d + 18, psz); // cksum will be filled in later -- we don't know what the link_entry data is yet
  return size;
}

void
LinkStat::link_probe::update_cksum(unsigned char *d)
{
  unsigned short cksum = calc_cksum(d);
  unsigned char *c = (unsigned char *) &cksum;
  d[cksum_offset] = c[0];
  d[cksum_offset + 1] = c[1];
}

unsigned short
LinkStat::link_probe::calc_cksum(const unsigned char *d)
{
  link_probe lp(d);
  int nbytes = link_probe::size + lp.num_links * link_entry::size;
  return click_in_cksum(d, nbytes);
}

LinkStat::link_entry::link_entry(const unsigned char *d)
  : eth(d), num_rx(ushort_at(d + 6))
{
}

int
LinkStat::link_entry::write(unsigned char *d) const
{
  memcpy(d, eth.data(), 6);
  write_ushort_at(d + 6, num_rx);
  return size;
}

EXPORT_ELEMENT(LinkStat)
CLICK_ENDDECLS
