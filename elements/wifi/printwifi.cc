/*
 * printWifi.{cc,hh} -- print Wifi packets, for debugging.
 * John Bicket
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
#include <click/ipaddress.hh>
#include <click/confparse.hh>
#include <click/error.hh>
#include <click/glue.hh>
#include <click/straccum.hh>
#include <click/packet_anno.hh>
#include <clicknet/wifi.h>
#include <click/etheraddress.hh>
#include "printwifi.hh"
CLICK_DECLS

PrintWifi::PrintWifi()
  : Element(1, 1),
    _print_anno(false),
    _print_checksum(false)
{
  MOD_INC_USE_COUNT;
  _label = "";
}

PrintWifi::~PrintWifi()
{
  MOD_DEC_USE_COUNT;
}

int
PrintWifi::configure(Vector<String> &conf, ErrorHandler* errh)
{
  int ret;
  ret = cp_va_parse(conf, this, errh,
                  cpOptional,
		  cpString, "label", &_label,
		  cpKeywords,
		    cpEnd);
  return ret;
}

Packet *
PrintWifi::simple_action(Packet *p)
{
  struct click_wifi *wh = (struct click_wifi *) p->data();

  StringAccum sa;
  if (_label[0] != 0) {
    sa << _label.cc() << ": ";
  } else {
      sa << "PrintWifi ";
  }

  switch (wh->i_fc[1] & WIFI_FC1_DIR_MASK) {
  case WIFI_FC1_DIR_NODS:
    sa << "NODS ";
    break;
  case WIFI_FC1_DIR_TODS:
    sa << "TODS ";
    break;
  case WIFI_FC1_DIR_FROMDS:
    sa << "FROMDS ";
    break;
  case WIFI_FC1_DIR_DSTODS:
    sa << "DSTODS ";
    break;
  default:
    sa << "??? ";
  }



  switch (wh->i_fc[0] & WIFI_FC0_TYPE_MASK) {
  case WIFI_FC0_TYPE_MGT:
    sa << "mgt ";

    switch (wh->i_fc[0] & WIFI_FC0_SUBTYPE_MASK) {
    case WIFI_FC0_SUBTYPE_ASSOC_REQ:
      sa << "assoc_req ";
      break;
    case WIFI_FC0_SUBTYPE_ASSOC_RESP:
      sa << "assoc_resp ";
      break;
    case WIFI_FC0_SUBTYPE_REASSOC_REQ:
      sa << "reassoc_req ";
      break;
    case WIFI_FC0_SUBTYPE_REASSOC_RESP:
      sa << "reassoc_resp ";
      break;
    case WIFI_FC0_SUBTYPE_PROBE_REQ:
      sa << "probe_req ";
      break;
    case WIFI_FC0_SUBTYPE_PROBE_RESP:
      sa << "probe_resp ";
      break;
    case WIFI_FC0_SUBTYPE_BEACON:
      sa << "beacon ";
      break;
    case WIFI_FC0_SUBTYPE_ATIM:
      sa << "atim ";
      break;
    case WIFI_FC0_SUBTYPE_DISASSOC:
      sa << "disassco ";
      break;
    case WIFI_FC0_SUBTYPE_AUTH:
      sa << "auth ";
      break;
    case WIFI_FC0_SUBTYPE_DEAUTH:
      sa << "deauth ";
      break;
    default:
      sa << "??unknown subtype";
      break;
    }
    break;
  case WIFI_FC0_TYPE_CTL:
    sa << "ctl ";
    break;
  case WIFI_FC0_TYPE_DATA:
    sa << "data ";
    break;
  default:
    sa << "??? ";
  }


  sa << EtherAddress(wh->i_addr2);
  sa << " -> ";
  sa << " " << EtherAddress(wh->i_addr1);
  sa << " (" <<EtherAddress(wh->i_addr3) << ") ";
  
  sa << (int) WIFI_RATE_ANNO(p) << " Mbps ";

  sa << "+" << (int) WIFI_SIGNAL_ANNO(p) << "/" <<  (int) WIFI_NOISE_ANNO(p);

  click_chatter("%s\n", sa.cc());

  return p;
}

CLICK_ENDDECLS
EXPORT_ELEMENT(PrintWifi)
