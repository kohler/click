// -*- mode: c++; c-basic-offset: 4 -*-
/*
 * fakepcap.{cc,hh} -- a faked-up pcap-like interface
 * Eddie Kohler
 *
 * Copyright (c) 2001 International Computer Science Institute
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
#include "fakepcap.hh"
#include <click/click_ip.h>
#include <click/click_ether.h>
#include <click/click_fddi.h>

int
fake_pcap_parse_dlt(const String &str)
{
    if (str == "IP")
	return FAKE_DLT_RAW;
    else if (str == "ETHER")
	return FAKE_DLT_EN10MB;
    else if (str == "FDDI")
	return FAKE_DLT_FDDI;
    else
	return -1;
}

const char *
fake_pcap_unparse_dlt(int dlt)
{
    switch (dlt) {
      case FAKE_DLT_RAW:
	return "IP";
      case FAKE_DLT_EN10MB:
	return "ETHER";
      case FAKE_DLT_FDDI:
	return "FDDI";
      default:
	return "??";
    }
}

// Handling FORCE_IP.
bool
fake_pcap_force_ip(Packet *p, int dlt)
{
    const click_ip *iph = 0;
    switch (dlt) {
	/* XXX IP6? */

      case FAKE_DLT_RAW: {
	  iph = (const click_ip *)p->data();
	  if (p->length() < sizeof(click_ip)
	      || (int)p->length() < (iph->ip_hl << 2))
	      iph = 0;
	  break;
      }

      case FAKE_DLT_EN10MB: {
	  const click_ether *ethh = (const click_ether *)p->data();
	  iph = (const click_ip *)(ethh + 1);
	  if (p->length() < sizeof(click_ether) + sizeof(click_ip)
	      || ethh->ether_type != htons(ETHERTYPE_IP)
	      || p->length() < sizeof(click_ether) + (iph->ip_hl << 2))
	      iph = 0;
	  break;
      }

      case FAKE_DLT_FDDI: {
	  const click_fddi *fh = (const click_fddi *)p->data();
	  if (p->length() < sizeof(click_fddi)
	      || (fh->fc & FDDI_FC_LLCMASK) != FDDI_FC_LLC_ASYNC)
	      break;
	  const click_fddi_snap *fsh = (const click_fddi_snap *)fh;
	  if (p->length() < sizeof(*fsh) + sizeof(click_ip)
	      || memcmp(&fsh->dsap, FDDI_SNAP_EXPECTED, FDDI_SNAP_EXPECTED_LEN) != 0
	      || fsh->ether_type != htons(ETHERTYPE_IP))
	      break;
	  iph = (const click_ip *)(fsh + 1);
	  if (p->length() < sizeof(*fsh) + (iph->ip_hl << 2))
	      iph = 0;
	  break;
      }
      
      default:
	break;

    }

    if (iph) {
	p->set_ip_header(iph, iph->ip_hl << 2);
	return true;
    } else
	return false;
}

ELEMENT_REQUIRES(userlevel)
ELEMENT_PROVIDES(FakePcap)
