/*
 * incseqno.{cc,hh} -- element writes an incrementing sequence number into packets
 * Douglas S. J. De Couto
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
#include "incseqno.hh"
CLICK_DECLS

IncrementSeqNo::IncrementSeqNo() 
  : _seqno(0), _offset(0)
{
  MOD_INC_USE_COUNT;
  add_input();
  add_output();
}

IncrementSeqNo::~IncrementSeqNo()
{
  MOD_DEC_USE_COUNT;
}

IncrementSeqNo *
IncrementSeqNo::clone() const
{
  return new IncrementSeqNo();
}

Packet *
IncrementSeqNo::simple_action(Packet *p)
{
  if (p->length() < _offset + 4) {
    click_chatter("IncrementSeqNo %s: packet of %d bytes is too small to hold sequence number at offset %u",
		  id().cc(), p->length(), _offset);
    return p;
  }

  WritablePacket *wp = p->uniqueify();
  uint32_t *up = (uint32_t *) (wp->data() + _offset);
  if (_use_net_byteorder)
    *up = htonl(_seqno);
  else
    *up = _seqno;
  _seqno++;
  return wp;
}

int
IncrementSeqNo::configure(Vector<String> &conf, ErrorHandler *errh)
{
  _seqno = 0;
  _offset = 0;
  _use_net_byteorder = false;
  int res = cp_va_parse(conf, this, errh,
			cpKeywords,
			"OFFSET", cpUnsigned, "offset to store sequence number at", &_offset,
			"FIRST", cpUnsigned, "first sequence number to use", &_seqno,
			"NET_BYTE_ORDER", cpBool, "use network byte order?", &_use_net_byteorder,
			cpEnd);
  return res;
}

int
IncrementSeqNo::initialize(ErrorHandler *)
{
  return 0;
}

void
IncrementSeqNo::add_handlers() {
  add_default_handlers(true);
  add_read_handler("seq", next_seq, 0);
}

String
IncrementSeqNo::next_seq(Element *e, void *)
{
  IncrementSeqNo *e2 = (IncrementSeqNo *) e;
  return String(e2->_seqno);
}

CLICK_ENDDECLS
EXPORT_ELEMENT(IncrementSeqNo)

