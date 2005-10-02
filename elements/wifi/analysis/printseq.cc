/*
 * printseq.{cc,hh} -- print Wifi packets, for debugging.
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
#include "printseq.hh"
CLICK_DECLS

PrintSeq::PrintSeq()
  : _print_anno(false),
    _print_checksum(false)
{
  _label = "";
}

PrintSeq::~PrintSeq()
{
}

int
PrintSeq::configure(Vector<String> &conf, ErrorHandler* errh)
{
  int ret;
  _offset = 0;
  _bytes = 4;
  ret = cp_va_parse(conf, this, errh,
		    cpOptional,
		    cpString, "label", &_label,
		    cpKeywords,
		    "OFFSET", cpUnsigned, "offset", &_offset,
		    "BYTES", cpUnsigned, "bytes", &_bytes,
		    cpEnd);
  return ret;
}

Packet *
PrintSeq::simple_action(Packet *p)
{

  if (!p) {
    return p;
  }
  uint8_t *data = (uint8_t *) p->data() + _offset;

  u_int32_t seq = 0;
  if (_bytes == 2) {
    seq = le16_to_cpu(*(uint16_t *) data);
  } else {
    seq = le32_to_cpu(*(uint32_t *) data);
  }
  
  StringAccum sa;
  if (_label[0] != 0) {
    sa << _label.c_str() << ": ";
  }
  struct click_wifi_extra *ceh = (struct click_wifi_extra *) p->all_user_anno();  
  sa << p->timestamp_anno();
  sa << " seq " << seq;
  sa << " rate " << (int) ceh->rate;
  sa << " rssi " << (int) ceh->rssi;
  sa << " noise " << (int) ceh->silence;

  click_chatter("%s\n", sa.take_string().c_str());

    
  return p;
}

CLICK_ENDDECLS
EXPORT_ELEMENT(PrintSeq)
