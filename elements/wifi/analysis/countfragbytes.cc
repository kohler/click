/*
 * CountFragBytes.{cc,hh} -- sets wifi txrate annotation on a packet
 * John Bicket
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
#include <click/error.hh>
#include <click/glue.hh>
#include <click/packet_anno.hh>
#include <click/straccum.hh>
#include <clicknet/ether.h>
#include "countfragbytes.hh"
CLICK_DECLS

CountFragBytes::CountFragBytes()
  : Element(1,1)
{
  MOD_INC_USE_COUNT;
  static unsigned char bcast_addr[] = { 0xff, 0xff, 0xff, 0xff, 0xff, 0xff };
  _bcast = EtherAddress(bcast_addr);
}

CountFragBytes::~CountFragBytes()
{
  MOD_DEC_USE_COUNT;
}

int
CountFragBytes::configure(Vector<String> &conf, ErrorHandler *errh)
{
  _et = 0x7FFA;
  _length = 0;
  _overhead = 4;
  if (cp_va_parse(conf, this, errh,
		  cpKeywords, 
		  "LENGTH", cpUnsigned, "", &_length,
		  "FRAG_SIZE", cpUnsigned, "", &_frag_size,
		  "OVERHEAD", cpUnsigned, "", &_overhead,
		  cpEnd) < 0) {
    return -1;
  }

  return 0;
}

void
CountFragBytes::push (int port, Packet *p_in)
{
  unsigned const char *ptr = p_in->data();

  int ok_frame = true;
  int packet_ok_bytes = 0;
  StringAccum sa;
  sa << "[";
  for (unsigned int x = 0; x < _length; x++) {
    if (x == 23 || x == 24) {
      /* 802.11 sequence bytes */
      continue;
    }
    bool error = false;
    if (x < p_in->length()) {
      error = (ptr[x] != 0xff);
    } else {
      error = true;
    }

    if (error) {
      ok_frame = false;
    }

    if (x % _frag_size == 0) {
      if (ok_frame) {
	packet_ok_bytes += _frag_size - _overhead;
      }
      ok_frame = true;
    }
  }

  if (ok_frame && p_in->length() == _length) {
    packet_ok_bytes += p_in->length() % _frag_size;
  }
  
  click_chatter("packet_frag_bytes %d %d\n",
		_frag_size,
		packet_ok_bytes);

  _bytes += packet_ok_bytes;

  output(port).push(p_in);
  return;
}

enum {H_BYTES};

static String
CountFragBytes_read_param(Element *e, void *thunk)
{
  CountFragBytes *td = (CountFragBytes *)e;
  switch ((uintptr_t) thunk) {
  case H_BYTES: return String(td->_bytes) + "\n";
  default:
    return String();
  }
  
}  
	  
void
CountFragBytes::add_handlers()
{
  add_default_handlers(true);
  add_read_handler("byte_count", CountFragBytes_read_param, (void *) H_BYTES);

}
CLICK_ENDDECLS
EXPORT_ELEMENT(CountFragBytes)

