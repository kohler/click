/*
 * ErrorThreshold.{cc,hh} -- sets wifi txrate annotation on a packet
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
#include "errorthreshold.hh"
CLICK_DECLS

ErrorThreshold::ErrorThreshold()
{
}

ErrorThreshold::~ErrorThreshold()
{
}

int
ErrorThreshold::configure(Vector<String> &conf, ErrorHandler *errh)
{

  _correct_threshold = 0;

  if (cp_va_parse(conf, this, errh,
		  cpKeywords, 
		  "LENGTH", cpUnsigned, "", &_length,
		  "CORRECT_THRESH", cpUnsigned, "", &_correct_threshold,
		  cpEnd) < 0) {
    return -1;
  }

  return 0;
}

void
ErrorThreshold::push (int, Packet *p_in)
{
  unsigned const char *ptr = p_in->data();
  unsigned ok = 0;


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
    if (!error) {
      ok++;
    }
  }


  if (_correct_threshold && 
      ok * 100 < _length * _correct_threshold) {
    if (noutputs() == 2) {
      output(1).push(p_in);
    } else {
      p_in->kill();
    }
    return;
  }
  output(0).push(p_in);
  return;
}

CLICK_ENDDECLS
EXPORT_ELEMENT(ErrorThreshold)

