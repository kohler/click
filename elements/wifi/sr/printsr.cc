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
  : Element(1, 1),
    _print_anno(false),
    _print_checksum(false)
{
  MOD_INC_USE_COUNT;
  _label = "";
}

PrintSR::~PrintSR()
{
  MOD_DEC_USE_COUNT;
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

String 
PrintSR::sr_to_string(struct srpacket *pk) 
{
  StringAccum sa;
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
  sa << " (";
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
  sa << flags << ")";

  if (pk->_type == PT_DATA) {
    sa << " len " << pk->hlen_with_data();
  } else {
    sa << " len " << pk->hlen_wo_data();
  }

  if (pk->_type == PT_DATA) {
    sa << " dataseq " << pk->data_seq();
  }
  IPAddress qdst = IPAddress(pk->_qdst);
  if (qdst) {
    sa << " qdst " << qdst;
  }

  if (pk->_type == PT_DATA) {
    sa << " dlen=" << pk->data_len();
  }

  sa << " seq " << pk->seq();
  sa << " nhops " << pk->num_links();
  sa << " next " << pk->next();

  if (pk->get_random_from() || pk->get_random_to()) {
    sa << " [r " << pk->get_random_from();
    sa << " <" << pk->get_random_fwd_metric() << "," << pk->get_random_rev_metric() << ">";
    sa << " " << pk->get_random_to() << " r]";
  }

  sa << " [";
  for(int i = 0; i< pk->num_links(); i++) {
    sa << " "<< pk->get_link_node(i).s().cc() << " ";
    int fwd = pk->get_link_fwd(i);
    int rev = pk->get_link_rev(i);
    int seq = pk->get_link_seq(i);
    int age = pk->get_link_age(i);
    sa << "<" << fwd << " (" << seq << "," << age << ") " << rev << ">";
  }
  sa << " "<< pk->get_link_node(pk->num_links()).s().cc() << " ";
  sa << "]";

  return sa.take_string();

}
Packet *
PrintSR::simple_action(Packet *p)
{
  StringAccum sa;
  if (_label[0] != 0) {
    sa << _label.cc() << ": ";
  } else {
      sa << "PrintSR ";
  }

  click_ether *eh = (click_ether *) p->data();
  struct srpacket *pk = (struct srpacket *) (eh+1);


  sa << PrintSR::sr_to_string(pk);
  
  click_chatter("%s", sa.take_string().cc());

  return p;
}

CLICK_ENDDECLS
EXPORT_ELEMENT(PrintSR)
