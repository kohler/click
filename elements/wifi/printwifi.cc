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
  _timestamp = false;
  ret = cp_va_parse(conf, this, errh,
                  cpOptional,
		  cpString, "label", &_label,
		  cpKeywords,
		    "TIMESTAMP", cpBool, "print timestamp", &_timestamp,
		    cpEnd);
  return ret;
}

Packet *
PrintWifi::simple_action(Packet *p)
{
  struct click_wifi *wh = (struct click_wifi *) p->data();
  struct click_wifi_extra *ceh = (struct click_wifi_extra *) p->all_user_anno();  
  int type = wh->i_fc[0] & WIFI_FC0_TYPE_MASK;
  int subtype = wh->i_fc[0] & WIFI_FC0_SUBTYPE_MASK;

  EtherAddress src;
  EtherAddress dst;
  EtherAddress bssid;




  StringAccum sa;
  if (_label[0] != 0) {
    sa << _label.cc() << ": ";
  }
  if (_timestamp)
    sa << p->timestamp_anno() << ": ";

  int len;
  len = sprintf(sa.reserve(9), "%4d | ", p->length());
  sa.forward(len);

  if (ceh->rate == 11) {
    sa << " 5.5";
  } else {
    len = sprintf(sa.reserve(2), "%2d", ceh->rate/2);
    sa.forward(len);
  }
  sa << "Mb ";
  
  len = sprintf(sa.reserve(9), "+%2d/", ceh->rssi);
  sa.forward(len);

  len = sprintf(sa.reserve(9), "%2d | ", ceh->silence);
  sa.forward(len);



  switch (wh->i_fc[1] & WIFI_FC1_DIR_MASK) {
  case WIFI_FC1_DIR_NODS:
    dst = EtherAddress(wh->i_addr1);
    src = EtherAddress(wh->i_addr2);
    bssid = EtherAddress(wh->i_addr3);
    break;
  case WIFI_FC1_DIR_TODS:
    bssid = EtherAddress(wh->i_addr1);
    src = EtherAddress(wh->i_addr2);
    dst = EtherAddress(wh->i_addr3);
    break;
  case WIFI_FC1_DIR_FROMDS:
    dst = EtherAddress(wh->i_addr1);
    bssid = EtherAddress(wh->i_addr2);
    src = EtherAddress(wh->i_addr3);
    break;
  case WIFI_FC1_DIR_DSTODS:
    dst = EtherAddress(wh->i_addr1);
    src = EtherAddress(wh->i_addr2);
    bssid = EtherAddress(wh->i_addr3);
    break;
  default:
    sa << "??? ";
  }


  switch (type) {
  case WIFI_FC0_TYPE_MGT:
    sa << "mgmt ";

    switch (subtype) {
    case WIFI_FC0_SUBTYPE_ASSOC_REQ:      sa << "assoc_req "; break;
    case WIFI_FC0_SUBTYPE_ASSOC_RESP:     sa << "assoc_resp "; break;
    case WIFI_FC0_SUBTYPE_REASSOC_REQ:    sa << "reassoc_req "; break;
    case WIFI_FC0_SUBTYPE_REASSOC_RESP:   sa << "reassoc_resp "; break;
    case WIFI_FC0_SUBTYPE_PROBE_REQ:      sa << "probe_req "; break;
    case WIFI_FC0_SUBTYPE_PROBE_RESP:     sa << "probe_resp "; break;
    case WIFI_FC0_SUBTYPE_BEACON:         sa << "beacon "; break;
    case WIFI_FC0_SUBTYPE_ATIM:           sa << "atim "; break;
    case WIFI_FC0_SUBTYPE_DISASSOC:       sa << "disassco "; break;
    case WIFI_FC0_SUBTYPE_AUTH:           sa << "auth "; break;
    case WIFI_FC0_SUBTYPE_DEAUTH:         sa << "deauth "; break;
    default:
      sa << "unknown-subtype-" << (int) (wh->i_fc[0] & WIFI_FC0_SUBTYPE_MASK) << " ";
      break;
    }
    break;
  case WIFI_FC0_TYPE_CTL:
    sa << "cntl ";
    switch (wh->i_fc[0] & WIFI_FC0_SUBTYPE_MASK) {
    case WIFI_FC0_SUBTYPE_PS_POLL:    sa << "psp  "; break;
    case WIFI_FC0_SUBTYPE_RTS:        sa << "rts  "; break;
    case WIFI_FC0_SUBTYPE_CTS:	      sa << "cts  "; break;
    case WIFI_FC0_SUBTYPE_ACK:	      sa << "ack  "; break;
    case WIFI_FC0_SUBTYPE_CF_END:     sa << "cfe  "; break;
    case WIFI_FC0_SUBTYPE_CF_END_ACK: sa << "cfea "; break;
    default:
    sa << "unknown-subtype-" << (int) (wh->i_fc[0] & WIFI_FC0_SUBTYPE_MASK) << " ";
    }
    break;
  case WIFI_FC0_TYPE_DATA:
    sa << "data ";
    switch (wh->i_fc[1] & WIFI_FC1_DIR_MASK) {
    case WIFI_FC1_DIR_NODS:   sa << "nods "; break;
    case WIFI_FC1_DIR_TODS:   sa << "tods "; break;
    case WIFI_FC1_DIR_FROMDS: sa << "frds "; break;
    case WIFI_FC1_DIR_DSTODS: sa << "dsds "; break;
    }
    break;
  default:
    sa << "unknown-type-" << (int) (wh->i_fc[0] & WIFI_FC0_TYPE_MASK) << " ";
  }


  sa << EtherAddress(wh->i_addr1);
  if (p->length() >= 16) {
    sa << " " << EtherAddress(wh->i_addr2);
  }
  if (p->length() > 22) {
    sa << " " << EtherAddress(wh->i_addr3);
  }
  sa << " ";

  if (p->length() >= sizeof(click_wifi)) {
    uint16_t seq = le16_to_cpu(*(u_int16_t *)wh->i_seq) >> WIFI_SEQ_SEQ_SHIFT;
    uint8_t frag = le16_to_cpu(*(u_int16_t *)wh->i_seq) & WIFI_SEQ_FRAG_MASK;
    sa << "seq " << (int) seq;
    if (frag || wh->i_fc[1] & WIFI_FC1_MORE_FRAG) {
      sa << " frag " << (int) frag;
    }
    sa << " ";
  }

  sa << "[";
  if (ceh->flags & WIFI_EXTRA_TX) {
    sa << " TX ";
  }
  if (ceh->flags & WIFI_EXTRA_TX_FAIL) {
    sa << " TX_FAIL ";
  }
  if (ceh->flags & WIFI_EXTRA_TX_USED_ALT_RATE) {
    sa << " TX_ALT_RATE ";
  }
  if (ceh->flags & WIFI_EXTRA_RX_ERR) {
    sa << " RX_ERR ";
  }
  if (ceh->flags & WIFI_EXTRA_RX_MORE) {
    sa << " RX_MORE ";
  }
  if (wh->i_fc[1] & WIFI_FC1_RETRY) {
    sa << " RETRY ";
  }
  sa << "] ";

  uint8_t *ptr;
  ptr = (uint8_t *) p->data() + sizeof(struct click_wifi);

  if (subtype == WIFI_FC0_SUBTYPE_AUTH) {
    uint16_t algo = le16_to_cpu(*(uint16_t *) ptr);
    ptr += 2;
    
    uint16_t seq = le16_to_cpu(*(uint16_t *) ptr);
    ptr += 2;
    
    uint16_t status =le16_to_cpu(*(uint16_t *) ptr);
    ptr += 2;
    sa << "alg " << (int)  algo << " seq " << (int) seq << " status " << status;
  }
  click_chatter("%s\n", sa.cc());

  return p;
}

CLICK_ENDDECLS
EXPORT_ELEMENT(PrintWifi)
