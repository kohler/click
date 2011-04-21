/*
 * arpprint.{cc,hh} -- element prints packet contents to system log
 * Jose Maria Gonzalez
 *
 * Shameless graft of ipprint.hh/cc and tcpdump-3.8.3/print-arp.c
 *
 * Copyright (c) 2005-2006 Regents of the University of California
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
#include "arpprint.hh"
#include <click/glue.hh>
#include <click/args.hh>
#include <click/error.hh>
#include <click/straccum.hh>
#include <click/packet_anno.hh>
#include <click/router.hh>
#include <click/nameinfo.hh>

#include <click/etheraddress.hh>
#include <clicknet/ether.h>

#if CLICK_USERLEVEL
# include <stdio.h>
#endif

CLICK_DECLS


/*
 * Address Resolution Protocol.
 *
 * See RFC 826 for protocol description.  ARP packets are variable
 * in size; the arphdr structure defines the fixed-length portion.
 * Protocol type values are the same as those for 10 Mb/s Ethernet.
 * It is followed by the variable-sized fields ar_sha, arp_spa,
 * arp_tha and arp_tpa in that order, according to the lengths
 * specified.  Field names used correspond to RFC 826.
 */



ARPPrint::ARPPrint()
    : _label(), _print_timestamp(true), _print_ether(false), _active(true)
{
#if CLICK_USERLEVEL
    _outfile = 0;
#endif
}

ARPPrint::~ARPPrint()
{
}

int
ARPPrint::configure(Vector<String> &conf, ErrorHandler *errh)
{
    String channel;

    if (Args(conf, this, errh)
	.read_p("LABEL", _label)
	.read("TIMESTAMP", _print_timestamp)
	.read("ETHER", _print_ether)
	.read("ACTIVE", _active)
#if CLICK_USERLEVEL
	.read("OUTFILE", FilenameArg(), _outfilename)
#endif
	.complete() < 0)
	return -1;

    _errh = router()->chatter_channel(channel);
    return 0;
}

int
ARPPrint::initialize(ErrorHandler *errh)
{
#if CLICK_USERLEVEL
  if (_outfilename) {
    _outfile = fopen(_outfilename.c_str(), "wb");
    if (!_outfile)
      return errh->error("%s: %s", _outfilename.c_str(), strerror(errno));
  }
#else
  (void) errh;
#endif
  return 0;
}

void
ARPPrint::cleanup(CleanupStage)
{
#if CLICK_USERLEVEL
    if (_outfile)
	fclose(_outfile);
    _outfile = 0;
#endif
}


Packet *
ARPPrint::simple_action(Packet *p)
{
    if (!_active || !p->has_network_header())
	return p;

    StringAccum sa;
    if (_label)
	sa << _label << ": ";
    if (_print_timestamp)
	sa << p->timestamp_anno() << ": ";

    if (_print_ether) {
	const unsigned char *x = p->mac_header();
	if (!x)
	    x = p->data();
	if (x + 14 <= p->network_header() && x + 14 <= p->end_data()) {
	    const click_ether *ethh = reinterpret_cast<const click_ether *>(x);
	    sa << EtherAddress(ethh->ether_shost) << " > "
	       << EtherAddress(ethh->ether_dhost) << ": ";
	}
    }

    if (p->network_length() < (int) sizeof(click_arp))
	sa << "truncated-arp (" << p->network_length() << ")";
    else {
	const click_ether_arp *ap = (const click_ether_arp *) p->network_header();
	uint16_t hrd = ntohs(ap->ea_hdr.ar_hrd);
	uint16_t pro = ntohs(ap->ea_hdr.ar_pro);
	uint8_t hln = ap->ea_hdr.ar_hln;
	uint8_t pln = ap->ea_hdr.ar_pln;
	uint16_t op = ntohs(ap->ea_hdr.ar_op);

	if ((pro != ETHERTYPE_IP && pro != ETHERTYPE_TRAIL) ||
		pln != 4 || hln == 0) {
	    sa << "arp-#" << op << " for proto #" << pro << " (" << pln << ") "
	    << "hardware #" << hrd << " (" << hln << ")";
	    return p;
	}
	const unsigned char *sha = (const unsigned char *)ap->arp_sha;
	const unsigned char *spa = (const unsigned char *)ap->arp_spa;
	const unsigned char *tha = (const unsigned char *)ap->arp_tha;
	const unsigned char *tpa = (const unsigned char *)ap->arp_tpa;

	if (pro == ETHERTYPE_TRAIL)
	    sa << "trailer-";

	switch (op) {
	case ARPOP_REQUEST:
	    {
	    const unsigned char ezero[6] = {0,0,0,0,0,0};
	    sa << "arp who-has " << IPAddress(tpa);
	    if ( memcmp (ezero, tha, hln) != 0 )
		sa << " (" << EtherAddress(tha) << ")";
	    sa << " tell " << IPAddress(spa);
	    break;
	    }

	case ARPOP_REPLY:
	    sa << "arp reply " << IPAddress(spa);
	    sa << " is-at " << EtherAddress(sha);
	    break;

	case ARPOP_REVREQUEST:
	    sa << "rarp who-is " << EtherAddress(tha) << " tell " <<
		EtherAddress(sha);
	    break;

	case ARPOP_REVREPLY:
	    sa << "rarp reply " << EtherAddress(tha) << " at " << IPAddress(tpa);
	    break;

	case ARPOP_INVREQUEST:
	    sa << "invarp who-is " << EtherAddress(tha) << " tell " <<
		EtherAddress(sha);
	    break;

	case ARPOP_INVREPLY:
	    sa << "invarp reply " << EtherAddress(tha) << " at " <<
		IPAddress(tpa);
	    break;

	default:
	    sa << "arp-#" << op;
	    // default_print((const u_char *)ap, caplen);
	}
	if (hrd != ARPHRD_ETHER)
	    sa << "hardware #" << hrd;
    }

#if CLICK_USERLEVEL
    if (_outfile) {
	sa << '\n';
	ignore_result(fwrite(sa.data(), 1, sa.length(), _outfile));
    } else
#endif
	_errh->message("%s", sa.c_str());

    return p;
}


void
ARPPrint::add_handlers()
{
    add_data_handlers("active", Handler::OP_READ | Handler::OP_WRITE, &_active);
}

CLICK_ENDDECLS
EXPORT_ELEMENT(ARPPrint)
