/*
 * printsrcr.{cc,hh} -- print srcr packets, for debugging.
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
#include "printsrcr.hh"
#include <click/ipaddress.hh>
#include <click/confparse.hh>
#include <click/error.hh>
#include <click/glue.hh>
#include <elements/grid/linkstat.hh>
#include <click/straccum.hh>
#include <clicknet/ether.h>
CLICK_DECLS

PrintSRCR::PrintSRCR()
  : Element(1, 1)
{
  MOD_INC_USE_COUNT;
  _label = "";
}

PrintSRCR::~PrintSRCR()
{
  MOD_DEC_USE_COUNT;
}

PrintSRCR *
PrintSRCR::clone() const
{
  return new PrintSRCR;
}

int
PrintSRCR::configure(Vector<String> &conf, ErrorHandler* errh)
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
PrintSRCR::simple_action(Packet *p)
{
  struct sr_pkt *pk = (struct sr_pkt *) p->data();

  if(pk->ether_type != ETHERTYPE_SRCR){
    click_chatter("PrintSRCR %s%s%s: not a srcr packet",
		  id().cc(),
		  _label.cc()[0] ? " " : "",
                  _label.cc());
    return (0);
  }

  StringAccum sa;
  sa << "PrintSRCR ";
  if (_label[0] != 0) {
    sa << _label.cc() << " ";
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
  default:
    type = "UNKNOWN";
  }
  String flags = "";
  if (pk->_flags & PF_BETTER) {
    flags = "FBETTER";
  }
  sa << ": " << type << " (" << flags << ") ";

  if (pk->_type == PT_DATA) {
    sa << "len=" << pk->hlen_with_data() << " ";
  } else {
    sa << "len=" << pk->hlen_wo_data() << " ";
  }

  if (pk->_type != PT_DATA) {
    sa << "qdst=" << IPAddress(pk->_qdst) << " ";
  }

  if (pk->_type == PT_DATA) {
    sa << "dlen=" << ntohs(pk->_dlen) << " ";
  }

  sa << "nhops=" << ntohs(pk->_nhops) << " ";
  sa << "next=" << ntohs(pk->_next) << " ";

  sa << "[";
  for(int i = 0; i< ntohs(pk->_nhops); i++) {
    sa << " "<< IPAddress(pk->get_hop(i)).s().cc() << " ";
    if (i != ntohs(pk->_nhops) - 1) {
      sa << "<" << pk->get_metric(i) << ">";
    }

  }
  sa << "]";



  click_chatter("%s", sa.cc());

  return p;
}

CLICK_ENDDECLS
EXPORT_ELEMENT(PrintSRCR)
