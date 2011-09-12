/*
 * checkarpheader.{cc,hh} -- element checks ARP header for correctness
 * Jose Maria Gonzalez
 *
 * Copyright (c) 1999-2000 Massachusetts Institute of Technology
 * Copyright (c) 2003 International Computer Science Institute
 * Copyright (c) 2006 Regents of the University of California
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
#include "checkarpheader.hh"
#include <clicknet/ether.h>
#include <click/glue.hh>
#include <click/args.hh>
#include <click/straccum.hh>
#include <click/error.hh>
#include <click/standard/alignmentinfo.hh>
CLICK_DECLS


const char * const CheckARPHeader::reason_texts[NREASONS] = {
    "tiny packet", "too small for addresses", "bad hardware type/length",
    "bad protocol type/length"
};

CheckARPHeader::CheckARPHeader()
    : _reason_drops(0)
{
    _drops = 0;
}

CheckARPHeader::~CheckARPHeader()
{
  delete[] _reason_drops;
}

int
CheckARPHeader::configure(Vector<String> &conf, ErrorHandler *errh)
{
    _offset = 0;
    bool verbose = false;
    bool details = false;

    if (Args(conf, this, errh)
	.read_p("OFFSET", _offset)
	.read("VERBOSE", verbose)
	.read("DETAILS", details)
	.complete() < 0)
	return -1;

    _verbose = verbose;
    if (details)
	_reason_drops = new atomic_uint32_t[NREASONS];

    return 0;
}

Packet *
CheckARPHeader::drop(Reason reason, Packet *p)
{
  if (_drops == 0 || _verbose)
    click_chatter("ARP header check failed: %s", reason_texts[reason]);
  _drops++;

  if (_reason_drops)
    _reason_drops[reason]++;

  checked_output_push(1, p);
  return 0;
}

Packet *
CheckARPHeader::simple_action(Packet *p)
{
  const click_arp *ap = reinterpret_cast<const click_arp *>(p->data() + _offset);
  unsigned plen = p->length() - _offset;
  unsigned hlen;

  // cast to int so very large plen is interpreted as negative
  if ((int) plen < (int) sizeof(click_arp))
      return drop(MINISCULE_PACKET, p);

  hlen = (int) sizeof(click_arp) + 2*ap->ar_hln + 2*ap->ar_pln;
  if ((int) plen < (int) hlen)
      return drop(BAD_LENGTH, p);
  else if (ap->ar_hrd == htons(ARPHRD_ETHER) && ap->ar_hln != 6)
      return drop(BAD_HRD, p);
  else if ((ap->ar_pro == htons(ETHERTYPE_IP) && ap->ar_pln != 4)
	   || (ap->ar_pro == htons(ETHERTYPE_IP6) && ap->ar_pln != 16))
      return drop(BAD_PRO, p);

  p->set_network_header((const unsigned char *) ap, hlen);
  return p;
}

String
CheckARPHeader::read_handler(Element *e, void *)
{
  CheckARPHeader *c = reinterpret_cast<CheckARPHeader *>(e);
  StringAccum sa;
  for (int i = 0; i < NREASONS; i++)
      sa << c->_reason_drops[i] << '\t' << reason_texts[i] << '\n';
  return sa.take_string();
}

void
CheckARPHeader::add_handlers()
{
    add_data_handlers("drops", Handler::OP_READ, &_drops);
    if (_reason_drops)
	add_read_handler("drop_details", read_handler, 1);
}

CLICK_ENDDECLS
EXPORT_ELEMENT(CheckARPHeader)
