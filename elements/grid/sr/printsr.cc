/*
 * printsr.{cc,hh} -- print sr packets, for debugging.
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
#include <clicknet/ether.h>
#include "srpacket.hh"
#include "printsr.hh"

CLICK_DECLS

PrintSR::PrintSR()
  : Element(1, 1)
{
  MOD_INC_USE_COUNT;
  _label = "";
}

PrintSR::~PrintSR()
{
  MOD_DEC_USE_COUNT;
}

PrintSR *
PrintSR::clone() const
{
  return new PrintSR;
}

int
PrintSR::configure(Vector<String> &conf, ErrorHandler* errh)
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
PrintSR::simple_action(Packet *p)
{
  click_ether *eh = (click_ether *) p->data();
  struct srpacket *pk = (struct srpacket *) (eh+1);

  StringAccum sa;
  if (_label[0] != 0) {
    sa << _label.cc() << ": ";
  } else {
      sa << "PrintSR ";
  }

  String type;
  switch (pk->_type) {
  case PT_QUERY:
    type = "QUERY";
    break;
  case PT_REPLY:
    type = "REPLY";
    break;
  case PT_DATA:
    type = "DATA";
    break;
  case PT_GATEWAY:
    type = "GATEWAY";
    break;
  default:
    type = "UNKNOWN";
  }
  String flags = "";
  sa << type;
  sa << " flags (";
  if (pk->flag(FLAG_ERROR)) {
    sa << " ERROR ";
  }
  if (pk->flag(FLAG_UPDATE)) {
    sa << " UPDATE ";
  }
  if (pk->flag(FLAG_SCHEDULE)) {
    sa << " SCHEDULE ";
  }
  if (pk->flag(FLAG_SCHEDULE_TOKEN)) {
    sa << " SCHEDULE_TOKEN ";
  }
  if (pk->flag(FLAG_SCHEDULE_FAKE)) {
    sa << " SCHEDULE_FAKE ";
  }

  if (pk->flag(FLAG_ECN)) {
    sa << " ECN ";
  }
  sa << flags << ") ";

  if (pk->_type == PT_DATA) {
    sa << " len " << pk->hlen_with_data();
  } else {
    sa << " len " << pk->hlen_wo_data();
  }
  
  sa << " cksum " << (unsigned long) ntohs(pk->_cksum);
  int failures = WIFI_NUM_FAILURES(p);
  sa << " failures " << failures;
  int success = WIFI_TX_SUCCESS_ANNO(p);
  sa << " success " << success;
  int rate = WIFI_RATE_ANNO(p);
  sa << " rate " << rate;

  if (pk->_type == PT_DATA) {
    sa << " dataseq " << pk->data_seq();
  } else {
    sa << " qdst " << IPAddress(pk->_qdst);
    sa << " seq " << pk->_seq;
  }

  if (pk->_type == PT_DATA) {
    sa << " dlen=" << ntohs(pk->_dlen);
  }

  sa << " seq " << pk->_seq;
  sa << " nhops " << pk->num_hops();
  sa << " next " << pk->next();

  sa << " [";
  for(int i = 0; i< pk->num_hops(); i++) {
    sa << " "<< pk->get_hop(i).s().cc() << " ";
    if (i != pk->num_hops() - 1) {
      sa << "<" << pk->get_metric(i) << ">";
    }

  }
  sa << "]";

  
  click_chatter("%s", sa.cc());

  return p;
}

CLICK_ENDDECLS
EXPORT_ELEMENT(PrintSR)
