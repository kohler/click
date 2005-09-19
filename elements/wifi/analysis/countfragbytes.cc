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
{
  static unsigned char bcast_addr[] = { 0xff, 0xff, 0xff, 0xff, 0xff, 0xff };
  _bcast = EtherAddress(bcast_addr);
}

CountFragBytes::~CountFragBytes()
{
}

int
CountFragBytes::configure(Vector<String> &conf, ErrorHandler *errh)
{
  _et = 0x7FFA;
  _length = 0;
  _overhead = 4;
  _frag_size = 1600;
  _adaptive = false;
  _adaptive_window = 1;
  _adaptive_window_bytes = 0;
  _adaptive_window_packets = 0;
  _adaptive_window_larger_bytes = 0;
  _adaptive_window_smaller_bytes = 0;
  _adaptive_window_smaller_packets = 0;
  _packets = 0;

  _adaptive_window_smaller_sample = 10;

  if (cp_va_parse(conf, this, errh,
		  cpKeywords, 
		  "LENGTH", cpUnsigned, "", &_length,
		  "FRAG_SIZE", cpUnsigned, "", &_frag_size,
		  "OVERHEAD", cpUnsigned, "", &_overhead,
		  "ADAPTIVE", cpBool, "", &_adaptive,
		  "ADAPTIVE_WINDOW", cpUnsigned, "", &_adaptive_window,
		  "ADAPTIVE_SMALL_SAMPLE", cpUnsigned, "", &_adaptive_window_smaller_sample,
		  cpEnd) < 0) {
    return -1;
  }

  return 0;
}

int frag_tput (int packet_size, 
		int frag_size, 
		int overhead, 
		int wrong_blocks) {
  unsigned frags = 1;
  if (frag_size < packet_size) {
    frags = packet_size / frag_size;
    if (packet_size % frag_size != 0) {
      frags++;
    }
  }
  unsigned correct_frags = 0;
  if (frags >  wrong_blocks) {
    correct_frags = frags - wrong_blocks;
  }
  return correct_frags * (frag_size - overhead);
}
void
CountFragBytes::push (int port, Packet *p_in)
{
  unsigned const char *ptr = p_in->data();

  int ok_frame = true;
  bool last_frag = false;
  int packet_ok_bytes = 0;
  int packet_ok_larger_bytes = 0;
  StringAccum sa;
  sa << "[";
  int frag_num = 0;

  int packet_frag_size = _frag_size;
  if (_adaptive && 
      _packets && 
      packet_frag_size / 2> 4 &&
      _packets % _adaptive_window_smaller_sample == 0) {
    if (0) {
      click_chatter("using small frag size %d\n",
		    _packets);
    }
    packet_frag_size = _frag_size / 2;
  }
  _packets++;
  for (unsigned int x = 0; x < _length; x++) {
    bool error = false;
    if (x < p_in->length()) {
      error = (ptr[x] != 0xff);
    } else {
      error = true;
    }

    if (error) {
      ok_frame = false;
    }

    if (x && x % packet_frag_size == 0) {
      frag_num++;

      if (packet_frag_size == _frag_size &&
	  frag_num % 2 == 0 && 
	  ok_frame && last_frag) {
	packet_ok_larger_bytes += packet_frag_size * 2 - _overhead;
      }

      if (ok_frame) {
	packet_ok_bytes += packet_frag_size - _overhead;
    
      }
      last_frag = ok_frame;
      ok_frame = true;
    }
  }

  int leftover =  _length % packet_frag_size;
  if (leftover) {
    if (ok_frame) {
      if (packet_frag_size == _frag_size && 
	  frag_num % 2) { 
	if (last_frag) {
	  packet_ok_larger_bytes += packet_frag_size + leftover - _overhead;
	}
      } else {
	packet_ok_larger_bytes += leftover - _overhead;
      }
    }
    if (ok_frame) {
      packet_ok_bytes += leftover - _overhead;
    }
  }
  
  if (packet_ok_bytes > _length) {
    click_chatter("AAHAH ! len %d packet_ok_bytes %d frag_size %d\n",
		  _length,
		  packet_ok_bytes,
		  _frag_size);
  }
  if (1) {
    click_chatter("packet_frag_bytes %d %d\n",
		  _frag_size,
		  packet_ok_bytes);
  }

  _bytes += packet_ok_bytes;

  if (packet_frag_size == _frag_size) {
    _adaptive_window_packets++;
    _adaptive_window_bytes += packet_ok_bytes;
    _adaptive_window_larger_bytes += packet_ok_larger_bytes;
  } else {
    /* smaller frag size */
    _adaptive_window_smaller_packets++;
    _adaptive_window_smaller_bytes += packet_ok_bytes;
  }



  if (_adaptive && 
      _packets && 
      _packets % _adaptive_window == 0) {
    unsigned new_frag_size = _frag_size;
    
    if (0) {
      click_chatter("packets %d small packets %d\n",
		    _adaptive_window_packets,
		    _adaptive_window_smaller_packets);
    }
    int average = _adaptive_window_bytes / _adaptive_window_packets;
    int average_large = _adaptive_window_larger_bytes / _adaptive_window_packets;


    if (1) {
      click_chatter("running adaptive avg %d %d larger_avg %d %d",
		    _frag_size,
		    average,
		    _frag_size * 2,
		    average_large);
    }
    if (average_large > average) {
      if (_frag_size <= 800) {
	new_frag_size *= 2;
      }
    } else if (_adaptive_window_smaller_packets) {
      int average_small = _adaptive_window_smaller_bytes / _adaptive_window_smaller_packets;
    if (1) {
      click_chatter(" smaller_avg %d %d\n",
		    _frag_size / 2,
		    average_small);
    }
      if ((new_frag_size / 2 > 1) &&
	  (!average || average_small > average)) {
      new_frag_size /= 2;
      }
    }

    click_chatter("\n");
    if (1 && new_frag_size != _frag_size) {
      click_chatter("frag_size %d to %d\n",
		    _frag_size,
		    new_frag_size);
    }

    _frag_size = new_frag_size;
    _adaptive_window_bytes = 0;
    _adaptive_window_packets = 0;
    _adaptive_window_larger_bytes = 0;
    _adaptive_window_smaller_bytes = 0;
    _adaptive_window_smaller_packets = 0;
    
  }

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
  add_read_handler("byte_count", CountFragBytes_read_param, (void *) H_BYTES);

}
CLICK_ENDDECLS
EXPORT_ELEMENT(CountFragBytes)

