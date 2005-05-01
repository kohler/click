/*
 * checksrheader.{cc,hh} -- element checks SR header for correctness
 * (checksums, lengths)
 * John Bicket
 * apapted from checkwifiheader.{cc,hh} by Douglas S. J. De Couto
 * from checkipheader.{cc,hh} by Robert Morris
 *
 * Copyright (c) 1999-2000 Massachusetts Institute of Technology
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
#include <click/etheraddress.hh>
#include <click/glue.hh>
#include <clicknet/ether.h>
#include <clicknet/ip.h>
#include "srpacket.hh"
#include "checksrheader.hh"


CLICK_DECLS

CheckSRHeader::CheckSRHeader()
  : _drops(0)
{
  add_input();
  add_output();
}

CheckSRHeader::~CheckSRHeader()
{
}

void
CheckSRHeader::notify_noutputs(int n)
{
  set_noutputs(n < 2 ? 1 : 2);
}

void
CheckSRHeader::drop_it(Packet *p)
{
  if (_drops == 0)
    click_chatter("CheckSRHeader %s: first drop", id().cc());
  _drops++;
  
  if (noutputs() == 2)
    output(1).push(p);
  else
    p->kill();
}

Packet *
CheckSRHeader::simple_action(Packet *p)
{
  click_ether *eh = (click_ether *) p->data();
  struct srpacket *pk = (struct srpacket *) (eh+1);
  unsigned int tlen = 0;

  if (!pk)
    goto bad;

  if (p->length() < sizeof(struct srpacket)) { 
    click_chatter("%s: packet truncated", id().cc());
    goto bad;
  }

  if (pk->_type & PT_DATA) {
    tlen = pk->hlen_with_data();
  } else {
    tlen = pk->hlen_wo_data();
  }

  if (pk->_version != _sr_version) {
    static bool version_warning = false;

    _bad_table.insert(EtherAddress(eh->ether_shost), pk->_version);

    if (!version_warning) {
      version_warning = true;
      click_chatter ("%s: unknown sr version %x from %s", 
		     id().cc(), 
		     pk->_version,
		     EtherAddress(eh->ether_shost).s().cc());
    }

     
     goto bad;
  }
  
  if (tlen > p->length()) { 
    /* can only check inequality, as short packets are padded to a
       minimum frame size for wavelan and ethernet */
    click_chatter("%s: bad packet size, wanted %d, only got %d", id().cc(),
		  tlen + sizeof(click_ether), p->length());
    goto bad;
  }

  if (click_in_cksum((unsigned char *) pk, tlen) != 0) {
    click_chatter("%s: bad SR checksum", id().cc());
    click_chatter("%s: length: %d, cksum: 0x%.4x", id().cc(), tlen, (unsigned long) ntohs(pk->_cksum));
    goto bad;
  }


  if (pk->next() > pk->num_links()){
    click_chatter("%s: data with bad next hop from %s\n", 
		  id().cc(),
		  pk->get_link_node(0).s().cc());
    goto bad;
  }




  return(p);
  
 bad:
  drop_it(p);
  return 0;
}



String 
CheckSRHeader::bad_nodes() {

  StringAccum sa;
  for (BadTable::const_iterator i = _bad_table.begin(); i; i++) {
    uint8_t version = i.value();
    EtherAddress dst = i.key();
    sa << this << " eth " << dst.s().cc() << " version " << (int) version << "\n";
  }

  return sa.take_string();
}

enum {
  H_DROPS,
  H_BAD_VERSION,
};

static String 
CheckSRHeader_read_param(Element *e, void *thunk)
{
  CheckSRHeader *td = (CheckSRHeader *)e;
    switch ((uintptr_t) thunk) {
    case H_DROPS:   return String(td->drops()) + "\n";
    case H_BAD_VERSION: return td->bad_nodes();
    default:
      return String() + "\n";
    }
}
      

void
CheckSRHeader::add_handlers()
{
  add_read_handler("drops", CheckSRHeader_read_param, (void *) H_DROPS);
  add_read_handler("bad_version", CheckSRHeader_read_param, (void *) H_BAD_VERSION);
}

#include <click/hashmap.cc>
#if EXPLICIT_TEMPLATE_INSTANCES
template class HashMap<EtherAddress, uint8_t>;
#endif

EXPORT_ELEMENT(CheckSRHeader)
CLICK_ENDDECLS

