/*
 * LocationHistogram.{cc,hh} -- sets wifi txrate annotation on a packet
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
#include "locationhistogram.hh"
CLICK_DECLS

LocationHistogram::LocationHistogram()
  : Element(1,1)
{
  MOD_INC_USE_COUNT;
}

LocationHistogram::~LocationHistogram()
{
  MOD_DEC_USE_COUNT;
}

int
LocationHistogram::configure(Vector<String> &conf, ErrorHandler *errh)
{
  _length = 0;
  if (cp_va_parse(conf, this, errh,
		  cpKeywords, 
		  "LENGTH", cpUnsigned, "", &_length,
		  cpEnd) < 0) {
    return -1;
  }

  while ((unsigned) _byte_errors.size() < _length) {
    _byte_errors.push_back(0);
  }

  return 0;
}

void
LocationHistogram::push (int port, Packet *p_in)
{
  unsigned const char *ptr = p_in->data();

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
      _byte_errors[x]++;
    }
  }
  output(port).push(p_in);
  return;
}

enum {H_HISTOGRAM};

static String
LocationHistogram_read_param(Element *e, void *thunk)
{
  LocationHistogram *td = (LocationHistogram *)e;
  switch ((uintptr_t) thunk) {
  case H_HISTOGRAM: {
    StringAccum sa;
    for (int x = 0; x < td->_byte_errors.size(); x++) {
      sa << "histogram " << x << " " << td->_byte_errors[x] << "\n";
    }

    return sa.take_string();
  }
  default:
    return String();
  }
  
}  
	  
void
LocationHistogram::add_handlers()
{
  add_default_handlers(true);
  add_read_handler("histogram", LocationHistogram_read_param, (void *) H_HISTOGRAM);

}
CLICK_ENDDECLS
EXPORT_ELEMENT(LocationHistogram)

