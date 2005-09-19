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
#include "printfragment.hh"
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


PrintFragment::PrintFragment()
{
}

PrintFragment::~PrintFragment()
{
}

int
PrintFragment::configure(Vector<String> &conf, ErrorHandler *errh)
{

  if (cp_va_parse(conf, this, errh,
		  cpOptional,
		  cpString, "label", &_label,
		  cpKeywords,
		  cpEnd) < 0)
    return -1;
  return 0;
}

Packet *
PrintFragment::simple_action(Packet *p)
{

  struct frag_header *fh = (struct frag_header *) p->data();


  
  StringAccum sa;
  if (_label[0] != 0) {
    sa << _label << ": ";
  } else {
    sa << "PrintFragment: ";
  }
  sa << "flags " << fh->flags;
  sa << " num_frags " << (int) fh->num_frags;
  sa << " num_frags_packet " << (int) fh->num_frags_packet;
  sa << " frag_size " << (int) fh->frag_size;


  int min_frag = 65535;
  int max_frag = -1;
  int print_foo = 1;
  if (print_foo) {
      sa << " [";
  }
  for (int x = 0; x < fh->num_frags; x++) {
    struct frag *f = (struct frag *) (p->data() + sizeof(struct frag_header) + x*(sizeof(frag) + fh->frag_size));    

    if ((const unsigned char *)f > p->data() + p->length()) {
      click_chatter("%{element} bad frag\n",
		    this);
      return 0;
    }

    min_frag = MIN(min_frag, f->packet_num);
    max_frag = MAX(max_frag, f->packet_num);
    if (!print_foo) {
    sa << " " << (int) f->packet_num;
    sa << " " << (int) f->frag_num;
    if (x != fh->num_frags - 1) {
      sa << " |";
    }
    }
  }

  if (print_foo) {
    sa << " [ " << min_frag << " " << max_frag << " ]";
  }
  sa << "\n";

  click_chatter("%s", sa.take_string().c_str());
  return p;
}



#include <click/hashmap.cc>
#if EXPLICIT_TEMPLATE_INSTANCES
template class HashMap<int, PrintFragment::PacketInfo>;
#endif
EXPORT_ELEMENT(PrintFragment)
CLICK_ENDDECLS

