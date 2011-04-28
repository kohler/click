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
#include <click/error.hh>
#include <click/args.hh>
#include "incseqno.hh"
CLICK_DECLS

IncrementSeqNo::IncrementSeqNo()
  : _seqno(0), _offset(0)
{
}

IncrementSeqNo::~IncrementSeqNo()
{
}

Packet *
IncrementSeqNo::simple_action(Packet *p)
{
  if (p->length() < _offset + 4) {
    click_chatter("IncrementSeqNo %s: packet of %d bytes is too small to hold sequence number at offset %u",
		  name().c_str(), p->length(), _offset);
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
  return Args(conf, this, errh)
      .read("OFFSET", _offset)
      .read("FIRST", _seqno)
      .read("NET_BYTE_ORDER", _use_net_byteorder)
      .complete();
}

int
IncrementSeqNo::initialize(ErrorHandler *)
{
  return 0;
}

void
IncrementSeqNo::add_handlers() {
  add_read_handler("seq", next_seq, 0);
  add_write_handler("seq", write_seq, 0);
}

int
IncrementSeqNo::write_seq(const String &in_s, Element *e, void *,
			  ErrorHandler *errh)
{
    IncrementSeqNo *e2 = (IncrementSeqNo *) e;

    unsigned i;
    if (!IntArg().parse(in_s, i)) {
      return errh->error("seq must be unsigned");
    }
    e2->_seqno = i;
    return 0;
}

String
IncrementSeqNo::next_seq(Element *e, void *)
{
  IncrementSeqNo *e2 = (IncrementSeqNo *) e;
  return String(e2->_seqno);
}

CLICK_ENDDECLS
EXPORT_ELEMENT(IncrementSeqNo)

