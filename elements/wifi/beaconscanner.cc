/*
 * beaconscanner.{cc,hh} -- decapsultates 802.11 packets
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
#include "beaconscanner.hh"
#include <click/etheraddress.hh>
#include <click/confparse.hh>
#include <click/error.hh>
#include <click/glue.hh>
#include <clicknet/wifi.h>
#include <clicknet/llc.h>
#include <click/straccum.hh>
CLICK_DECLS

BeaconScanner::BeaconScanner()
  : Element(1, 1)
{
  MOD_INC_USE_COUNT;
}

BeaconScanner::~BeaconScanner()
{
  MOD_DEC_USE_COUNT;
}

int
BeaconScanner::configure(Vector<String> &conf, ErrorHandler *errh)
{

  _debug = true;
  if (cp_va_parse(conf, this, errh,
		  /* not required */
		  cpKeywords,
		  "DEBUG", cpBool, "Debug", &_debug,
		  0) < 0)
    return -1;
  return 0;
}

Packet *
BeaconScanner::simple_action(Packet *p)
{


  uint8_t dir;
  uint8_t type;
  uint8_t subtype;


  if (p->length() < sizeof(struct click_wifi)) {
    click_chatter("%{element}: packet too small: %d vs %d\n",
		  this,
		  p->length(),
		  sizeof(struct click_wifi));

    p->kill();
    return 0;
	      
  }
  struct click_wifi *w = (struct click_wifi *) p->data();


  dir = w->i_fc[1] & WIFI_FC1_DIR_MASK;
  type = w->i_fc[0] & WIFI_FC0_TYPE_MASK;
  subtype = w->i_fc[0] & WIFI_FC0_SUBTYPE_MASK;

  if (type != WIFI_FC0_TYPE_MGT) {
    click_chatter("%{element}: received non-management packet\n",
		  this);
    p->kill();
    return 0;
  }

  if (subtype != WIFI_FC0_SUBTYPE_BEACON) {
    click_chatter("%{element}: received non-beacon packet\n",
		  this);
    p->kill();
    return 0;
  }

  uint8_t *ptr;
  
  ptr = (uint8_t *) p->data() + sizeof(struct click_wifi);

  uint8_t *ts = ptr;
  ptr += 8;

  uint8_t *beacon_interval = ptr;
  ptr += 2;

  uint8_t *cap_info = ptr;
  ptr += 2;
  
  uint8_t *end  = (uint8_t *) p->data() + p->length();

  uint8_t *ssid_l = NULL;
  uint8_t *rates_l = NULL;
  uint8_t *ds_l = NULL;
  while (ptr < end) {
    switch (*ptr) {
    case WIFI_ELEMID_SSID:
      ssid_l = ptr;
      break;
    case WIFI_ELEMID_RATES:
      rates_l = ptr;
      break;
    case WIFI_ELEMID_FHPARMS:
      break;
    case WIFI_ELEMID_DSPARMS:
      ds_l = ptr;
      break;
    case WIFI_ELEMID_IBSSPARMS:
      break;
    case WIFI_ELEMID_TIM:
      break;
    case WIFI_ELEMID_ERP:
      break;
    case WIFI_ELEMID_VENDOR:
      break;
    case 133: /* ??? */
      break;
    case 150: /* ??? */
      break;
    default:
      click_chatter("%{element}: ignored element id %u %u \n",
		    this,
		    *ptr,
		    ptr[1]);
    }
    ptr += ptr[1] + 2;

  }
  
  String ssid;
  if (ssid_l) {
    ssid = String((char *) ssid_l + 2, min((int)ssid_l[1], WIFI_NWID_MAXSIZE));
  } else {
    ssid = "(none)";
  }

  String rates;
  if (rates_l) {
    StringAccum sa;
    for (int x = 0; x < min((int)rates_l[1], WIFI_RATES_MAXSIZE); x++) {
      uint8_t rate = rates_l[x + 2];

      if (rate & WIFI_RATE_BASIC) {
	sa << "basic ";
      }
      sa << (int)(rate & WIFI_RATE_VAL) << " ";
    }
    rates = sa.take_string();
  } else {
    rates = "(none)";
  }

  String ds = "(none)";
  if (ds_l) {
    uint8_t dot11channel = ds_l[2];
    StringAccum sa;
    sa << (int) dot11channel;
    ds = sa.take_string();
  }

  StringAccum sa;

  sa << ": beacon ";
  sa << EtherAddress(w->i_addr2) << " -> ";
  sa << EtherAddress(w->i_addr1) << " ";
  sa << "(" <<EtherAddress(w->i_addr3) << ")";
  sa << " ssid " << ssid;
  sa << " channel " << ds;
  sa << " rates " << rates;
  if (_debug) {
    click_chatter("%s\n",
		  sa.take_string().cc());
  }

  return p;
}


enum {H_DEBUG};

static String 
BeaconScanner_read_param(Element *e, void *thunk)
{
  BeaconScanner *td = (BeaconScanner *)e;
    switch ((uintptr_t) thunk) {
      case H_DEBUG:
	return String(td->_debug) + "\n";
    default:
      return String();
    }
}
static int 
BeaconScanner_write_param(const String &in_s, Element *e, void *vparam,
		      ErrorHandler *errh)
{
  BeaconScanner *f = (BeaconScanner *)e;
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
BeaconScanner::add_handlers()
{
  add_default_handlers(true);

  add_read_handler("debug", BeaconScanner_read_param, (void *) H_DEBUG);

  add_write_handler("debug", BeaconScanner_write_param, (void *) H_DEBUG);
}
CLICK_ENDDECLS
EXPORT_ELEMENT(BeaconScanner)
