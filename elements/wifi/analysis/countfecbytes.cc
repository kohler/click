/*
 * CountFecBytes.{cc,hh} -- sets wifi txrate annotation on a packet
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
#include "countfecbytes.hh"
CLICK_DECLS

CountFecBytes::CountFecBytes()
  : Element(1,1)
{
  static unsigned char bcast_addr[] = { 0xff, 0xff, 0xff, 0xff, 0xff, 0xff };
  _bcast = EtherAddress(bcast_addr);
}

CountFecBytes::~CountFecBytes()
{
}

int
CountFecBytes::configure(Vector<String> &conf, ErrorHandler *errh)
{
  _et = 0x7FFA;
  _length = 0;
  _packets = 0;
  _overhead = 0;
  _tolerate = 0;
  _adaptive = false;
  if (cp_va_parse(conf, this, errh,
		  cpKeywords, 
		  "LENGTH", cpUnsigned, "", &_length,
		  "TOLERATE", cpUnsigned, "", &_tolerate,
		  "OVERHEAD", cpUnsigned, "", &_overhead,
		  "ADAPTIVE", cpBool, "", &_adaptive,
		  cpEnd) < 0) {
    return -1;
  }
  return 0;
}

void
CountFecBytes::push (int port, Packet *p_in)
{
  unsigned const char *ptr = p_in->data();

  StringAccum sa;
  sa << "[";

  _packets++;
  _packet_count++;
  int errors = 0;
  for (unsigned int x = 0; x < _length; x++) {
    bool error = false;
    if (x < p_in->length()) {
      error = (ptr[x] != 0xff);
    } else {
      error = true;
    }

    if (error) {
      errors++;
      _sum_errors++;
    }

  }


  bool  packet_ok = (errors <= _tolerate);
  if (packet_ok) {
    _bytes += _length - _overhead;
  } else {
    click_chatter("DROP %d vs %d\n", 
		  errors,
		  _tolerate);
  }
  
#define max(a, b) ((a) > (b) ? (a) : (b))
#define min(a, b) ((a) < (b) ? (a) : (b))

    int window = 10;
    if (_packet_count % window == 1) {
      _min_errors = errors;
      _max_errors = errors;
      _sum_errors = 0;
    } else {
      _min_errors = min (_min_errors, errors);
      _max_errors = max (_max_errors, errors);
    }
    
    click_chatter("errors %d\n", errors);
    
    
    if (_packet_count && _packet_count % window == 0) {
      int average_errors = _sum_errors / window;
      click_chatter("errors %d / %d / %d\n",
		    _min_errors,
		    average_errors,
		    _max_errors);
      if (_adaptive) {
	_tolerate = average_errors * 2 + 50;
	_overhead = _tolerate * 2 +100;
      }
    }


  output(port).push(p_in);
  return;
}

enum {H_BYTES};

static String
CountFecBytes_read_param(Element *e, void *thunk)
{
  CountFecBytes *td = (CountFecBytes *)e;
  switch ((uintptr_t) thunk) {
  case H_BYTES: return String(td->_bytes) + "\n";
  default:
    return String();
  }
  
}  
	  
void
CountFecBytes::add_handlers()
{
  add_default_handlers(true);
  add_read_handler("byte_count", CountFecBytes_read_param, (void *) H_BYTES);

}
CLICK_ENDDECLS
EXPORT_ELEMENT(CountFecBytes)

