/*
 * autotxpower.{cc,hh} -- sets wifi txpower annotation on a packet
 * John Bicket
 *
 * Copyright (c) 2003 Massachusetts Institute of Technology
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
#include <click/packet_anno.hh>
#include <click/straccum.hh>
#include <clicknet/ether.h>
#include "autotxpower.hh"
CLICK_DECLS

static inline int next_lower_power(int power) {
  return 0;
}


static inline int next_higher_power(int power) {
  return 0;
}
  

AutoTXPower::AutoTXPower()
  : Element(1, 1)
{
  MOD_INC_USE_COUNT;

  _max_probation_count = 10;
  /* bleh */
  static unsigned char bcast_addr[] = { 0xff, 0xff, 0xff, 0xff, 0xff, 0xff };
  _bcast = EtherAddress(bcast_addr);
}

AutoTXPower::~AutoTXPower()
{
  MOD_DEC_USE_COUNT;
}

AutoTXPower *
AutoTXPower::clone() const
{
  return new AutoTXPower;
}

int
AutoTXPower::configure(Vector<String> &conf, ErrorHandler *errh)
{
  if (cp_va_parse(conf, this, errh,
		  cpKeywords, 
		  0) < 0) {
    return -1;
  }

  return 0;
}

/* returns 0 if we haven't gotten feedback for a dst */
int 
AutoTXPower::get_tx_power(EtherAddress dst)
{
  if (dst == _bcast) {
    return 1;
  }
  DstInfo *nfo = _neighbors.findp(dst);
  if (!nfo) {
    return 0;
  }
  return nfo->_power;

}
Packet *
AutoTXPower::simple_action(Packet *p_in)
{
  click_ether *eh = (click_ether *) p_in->data();
  EtherAddress dst = EtherAddress(eh->ether_dhost);
  int success = WIFI_TX_SUCCESS_ANNO(p_in);
  int power = WIFI_TX_POWER_ANNO(p_in);
  int retries = WIFI_RETRIES_ANNO(p_in);
  if (dst == _bcast) {
    /* don't record info for bcast packets */
    p_in->kill();
    return 0;
  }

  if (0 == power) {
    /* power wasn't set */
    p_in->kill();
    return 0;
  }


  DstInfo *nfo = _neighbors.findp(dst);
  if (!nfo) {
    DstInfo foo = DstInfo(dst);
    _neighbors.insert(dst, foo);
    nfo = _neighbors.findp(dst);
  }

  if (success) {

    if (!nfo->_power || nfo->_power == power) {
      click_chatter("AutoTXPower: packet to %s succeeded, %d retries, %d power", 
		    dst.s().cc(), retries, power);
      click_gettimeofday(&nfo->_last_success);
      nfo->_packets++;
      nfo->_power = power;
      if (nfo->_packets > _max_probation_count) {
	int next_power = next_higher_power(nfo->_power);
	if (next_power == nfo->_power) {
	  nfo->_packets++;
	} else {
	  nfo->_power = next_power;
	  nfo->_packets = 0;
	}

      }
    }
    p_in->kill();
    return 0;
  }

  /* the packet has failed */
  if (WIFI_RETRIES_ANNO(p_in) > 4) {
    click_chatter("packet to %s failed!", dst.s().cc());
    p_in->kill();
    return 0;
  }

  if (retries > 0 && power > 1) {
    /* try twice at a speed before bumping down */
    nfo->_power = next_lower_power(nfo->_power);
    click_chatter("AutoTXPower: packet to %s failed twice, bumping down to %d\n", 
		  dst.s().cc(), nfo->_power);
    
    SET_WIFI_TX_POWER_ANNO(p_in, nfo->_power);
    SET_WIFI_RETRIES_ANNO(p_in, 0);
    nfo->_packets = 0;
  }
  SET_WIFI_RETRIES_ANNO(p_in, retries+1);


  return p_in;
}
String
AutoTXPower::static_print_stats(Element *e, void *)
{
  AutoTXPower *n = (AutoTXPower *) e;
  return n->print_stats();
}

String
AutoTXPower::print_stats() 
{
  struct timeval now;
  click_gettimeofday(&now);
  
  StringAccum sa;
  for (NIter iter = _neighbors.begin(); iter; iter++) {
    DstInfo n = iter.value();
    struct timeval age = now - n._last_success;
    sa << n._eth.s().cc() << " ";
    sa << "power: " << n._power << " ";
    sa << "packets: " << n._packets << " ";
    sa << "last_success: " << age << "\n";
  }
  return sa.take_string();
}
void
AutoTXPower::add_handlers()
{
  add_default_handlers(true);
  add_read_handler("stats", static_print_stats, 0);

}
// genepower Vector template instance
#include <click/bighashmap.cc>
#if EXPLICIT_TEMPLATE_INSTANCES
template class HashMap<EtherAddress, AutoTXPower::DstInfo>;
#endif
CLICK_ENDDECLS
EXPORT_ELEMENT(AutoTXPower)

