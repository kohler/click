/*
 * wifiencap.{cc,hh} -- encapsultates 802.11 packets
 * John Bicket
 *
 * Copyright (c) 2004 Massachusetts Institute of Technology
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
#include "checkfragment.hh"
#include <click/etheraddress.hh>
#include <click/confparse.hh>
#include <click/error.hh>
#include <click/glue.hh>
#include <clicknet/wifi.h>
#include <clicknet/llc.h>
#include <click/packet_anno.hh>
#include <click/straccum.hh>
#include "frag.hh"
CLICK_DECLS


CheckFragment::CheckFragment()
  : Element(1, 1)
{
}

CheckFragment::~CheckFragment()
{
}

void
CheckFragment::notify_noutputs(int n)
{
  set_noutputs(n < 3 ? n : 1);
}

int
CheckFragment::configure(Vector<String> &conf, ErrorHandler *errh)
{

  _header_only = false;
  if (cp_va_parse(conf, this, errh,
		  cpOptional,
		  cpKeywords,
		  "HEADER_ONLY", cpBool, "", &_header_only,
		  cpEnd) < 0)
    return -1;
  return 0;
}

Packet *
CheckFragment::simple_action(Packet *p)
{

  if (!p) {
    return p;
  }
  unsigned expected_length = 0;
  struct frag_header *fh = (struct frag_header *)p->data();
  if (p->length() < sizeof(struct frag_header)) {
    click_chatter("%{element} packet too small: got %d needed %d\n",
		  this,
		  p->length(),
		  sizeof(struct frag_header));
    goto drop;
  }


  if (!fh->valid_checksum()) {
    if (0) {
      click_chatter("%{element}: header failed checksum\n",
		    this);
    }
    goto drop;
  }
  

  fh = (struct frag_header *) p->data();
  expected_length = fh->packet_size(fh->num_frags, fh->frag_size);
  if (expected_length != p->length()) {
    if (0) {
      click_chatter("%{element}: bad length got %d expected %d frag_size %d num_frags %d\n",
		    this,
		    p->length(),
		    expected_length,
		    fh->frag_size,
		    fh->num_frags);
    }
    goto drop1;
  }

  for (int x = 0; x < fh->num_frags; x++) {
    struct frag *f = fh->get_frag(x);
    if (!f->valid_checksum(fh->frag_size)) {
      if (0) {
	click_chatter("%{element}: frag %d [ %d %d ] failed checksum\n",
		      this,
		      x,
		      f->packet_num,
		      f->frag_num);
      }
      goto drop1;
    }
   
  } 
    
  return p;

 drop1:
  if (_header_only) {
    return p;
  }

 drop:
  if (noutputs() > 1) {
    output(1).push(p);
    return 0;
  }
  p->kill();
  return 0;
}



 
void
CheckFragment::add_handlers()
{
  add_default_handlers(true);
}

EXPORT_ELEMENT(CheckFragment)
CLICK_ENDDECLS

