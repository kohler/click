/*
 * CountErrors.{cc,hh} -- sets wifi txrate annotation on a packet
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
#include "counterrors.hh"
CLICK_DECLS

CountErrors::CountErrors()
  : Element(2,2)
{
  MOD_INC_USE_COUNT;
  static unsigned char bcast_addr[] = { 0xff, 0xff, 0xff, 0xff, 0xff, 0xff };
  _bcast = EtherAddress(bcast_addr);
}

CountErrors::~CountErrors()
{
  MOD_DEC_USE_COUNT;
}

int
CountErrors::configure(Vector<String> &conf, ErrorHandler *errh)
{
  _et = 0x7FFA;
  _length = 0;
  _runs = true;
  if (cp_va_parse(conf, this, errh,
		  cpKeywords, 
		  "ETHTYPE", cpUnsigned, "Ethernet encapsulation type", &_et,
		  "RUNS", cpBool, "", &_runs,
		  cpEnd) < 0) {
    return -1;
  }

  return 0;
}

void
CountErrors::push (int port, Packet *p_in)
{
  click_ether *eh = (click_ether *) p_in->data();
  EtherAddress src = EtherAddress(eh->ether_shost);
  unsigned et = ntohs(eh->ether_type);
  
  if (port == 0) {
    if (!_length) {
      _length = p_in->length();
      _et = et;
      _src = src;
    }
    output(port).push(p_in);
    return;
  }

  if (p_in->length() == _length ||
      src == _src ||
      et == _et) {
		  
  } else {
    p_in->kill();
    return;
  }


  int errors = 0;
  unsigned const char *ptr = p_in->data();
  int current_run = 0;
  int runs = 0;
  for (unsigned int x = sizeof(click_ether); x < _length; x++) {
    bool error = false;
    if (x < p_in->length()) {
      error= (ptr[x] != 0x30);
    } else {
      error = true;
    }
    if (error) {
      errors++;
      current_run++;
    } else {
      if (_runs && current_run) {
	click_chatter("%{element} run %d\n",
		      this,
		      current_run);
      }
      current_run = 0;
      runs++;
    }
  }

  if (_runs && current_run) {
    click_chatter("%{element} run %d\n",
		  this,
		  current_run);
    runs++;
  }

  if (_length) {
    click_chatter("%{element} errors %d\n",
		  this,
		  errors);
  }
  if (_length) {
    click_chatter("%{element} runs %d\n",
		  this,
		  runs);
  }
  output(port).push(p_in);
  return;
}

enum {H_STATS, H_SIGNAL, H_NOISE};

static String
CountErrors_read_param(Element *e, void *thunk)
{
  CountErrors *td = (CountErrors *)e;
  switch ((uintptr_t) thunk) {
  default:
    return String();
  }
  
}  
	  
void
CountErrors::add_handlers()
{
  add_default_handlers(true);


}
CLICK_ENDDECLS
EXPORT_ELEMENT(CountErrors)

