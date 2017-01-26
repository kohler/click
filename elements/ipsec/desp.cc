/*
 * desp.{cc,hh} -- element implements IPsec unencapsulation (RFC 2406)
 * Alex Snoeren, Benjie Chen
 *
 * Copyright (c) 1999-2000 Massachusetts Institute of Technology
 *
 * Added Security Association Table support. Dimitris Syrivelis <jsyr@inf.uth.gr>, University of Thessaly, Hellas
 * Added Replay Check support originally written for linux ipsec-0.5 from John Ioannidis <ji@hol.gr>
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
#ifndef HAVE_IPSEC
# error "Must #define HAVE_IPSEC in config.h"
#endif
#include "esp.hh"
#include "desp.hh"
#include <click/ipaddress.hh>
#include <click/confparse.hh>
#include <click/error.hh>
#include <click/glue.hh>
#include <click/element.hh>
#include <click/packet_anno.hh>
#include "satable.hh"
#include "sadatatuple.hh"
CLICK_DECLS

IPsecESPUnencap::IPsecESPUnencap()
{
}

IPsecESPUnencap::~IPsecESPUnencap()
{
}

int
IPsecESPUnencap::checkreplaywindow(SADataTuple * sa_data,unsigned long seq)
  {
	unsigned long diff;

	if (seq == 0)
		return 0;		/* first == 0 or wrapped */
	/*This logic has been added for the time being to deal with replay rollover*/
	if((seq == sa_data->replay_start_counter) && (sa_data->lastseq!=sa_data->replay_start_counter)) { sa_data->bitmap=0;sa_data->lastseq=seq;return 1;}

	if (seq > sa_data->lastseq)	/* new larger sequence number */
	{
		diff = seq - sa_data->lastseq;
		if (diff < sa_data->ooowin) /* In win, set bit for this pkt */
			sa_data->bitmap = (sa_data->bitmap << diff) | 1;
		else
			sa_data->bitmap = 1; /* This packet has way larger */

		sa_data->lastseq = seq;
		return 1;		/* larger is good */
	}
	diff = sa_data->lastseq - seq;
	if (diff >= sa_data->ooowin) {	/* too old or wrapped */
		click_chatter("Replay protection: This packet is too old to be accepted\n");
		return 0;

        }
	if (sa_data->bitmap & (1 << diff)) { /* this packet already seen */
		click_chatter("Replay protection: This packet is already seen...\n");
		return 0;
        }
	sa_data->bitmap |= (1 << diff);	/* mark as seen */
	return 1;			/* out of order but good */
}

Packet *
IPsecESPUnencap::simple_action(Packet *p)
{
  int i, blks;
  const unsigned char * blk;
  SADataTuple * sa;

  //Check replay counter:
  struct esp_new *esp = (struct esp_new *) p->data();

  sa=(SADataTuple *)IPSEC_SA_DATA_REFERENCE_ANNO(p);

  if(sa==NULL) {click_chatter("Null reference to Security Association Table");}

  if(!checkreplaywindow(sa,(unsigned long)ntohl(esp->esp_rpl))) {
      p->kill(); //The packet failed replay check and it is therefore dropped
      return (0);
  }

  // rip off ESP header
  p->pull(sizeof(esp_new));
  // verify padding
  blks = p->length();
  blk = p->data();

  if((blk[blks - 2] != blk[blks - 3]) && (blk[blks -2] != 0)) {
    click_chatter("Invalid padding length");
    p->kill();
    return(0);
  }
  blks = blk[blks - 2];
  blk = p->data() + p->length() - (blks + 2);
  for(i = 0; (i < blks) && (blk[i] == i + 1);)
    ++i;
  if(i<blks) {
    click_chatter("Corrupt padding");
    p->kill();
    return(0);
  }
  // chop off padding
  p->take(blks+2);
  return p;
}

CLICK_ENDDECLS
EXPORT_ELEMENT(IPsecESPUnencap)
ELEMENT_MT_SAFE(IPsecESPUnencap)
