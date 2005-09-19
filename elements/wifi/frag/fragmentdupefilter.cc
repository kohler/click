/*
 * wifiencap.{cc,hh} -- encapsultates 802.11 pdupefilterets
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
#include "fragmentdupefilter.hh"
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


FragmentDupeFilter::FragmentDupeFilter()
{
}

FragmentDupeFilter::~FragmentDupeFilter()
{
}

int
FragmentDupeFilter::configure(Vector<String> &conf, ErrorHandler *errh)
{

  _window_size = 500;
  _debug = false;
  if (cp_va_parse(conf, this, errh,
		  cpOptional,
		  cpKeywords,
		  "WINDOW", cpUnsigned, "", &_window_size,
		  "DEBUG", cpBool, "", &_debug,
		  cpEnd) < 0)
    return -1;
  return 0;
}



Packet *
FragmentDupeFilter::simple_action(Packet *p)
{

  struct frag_header *fh = (struct frag_header *) p->data();
  EtherAddress src = EtherAddress(fh->src);
  DstInfo *nfo = _frags.findp(src);

  if (!nfo) {
    _frags.insert(src, DstInfo(src));
    nfo = _frags.findp(src);
  }

  for (int x = 0; x < fh->num_frags; x++) {
    struct frag *f = fh->get_frag(x);
    struct fragid fid = fragid(f->packet_num, f->frag_num);
    for (int y = 0; y < nfo->frags.size(); y++) {
      if (nfo->frags[y] == fid) {
	if (_debug) {
	  click_chatter("%{element} dupe [ %d %d ]\n",
			this,
			f->packet_num,
			f->frag_num);
	}
	p->kill();
	return 0;
      }
    }  
    nfo->frags.push_back(fid);
  }


  /* clear space for new seq */
  while( (unsigned) nfo->frags.size() > _window_size) {
    nfo->frags.pop_front();
  }
  
  return p;
  
}


#include <click/vector.cc>
#include <click/dequeue.cc>
#include <click/hashmap.cc>
#if EXPLICIT_TEMPLATE_INSTANCES
template class HashMap<EtherAddress, FragmentDupeFilter::DstInfo>;
#endif
EXPORT_ELEMENT(FragmentDupeFilter)
CLICK_ENDDECLS

