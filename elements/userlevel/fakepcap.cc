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
#include <clicknet/ip.h>
#include <clicknet/ip6.h>
#include <clicknet/ether.h>
#include <clicknet/fddi.h>
#include <clicknet/rfc1483.h>
#include <clicknet/wifi.h>
#include <clicknet/llc.h>
#include <clicknet/ppp.h>
#include <click/args.hh>
CLICK_DECLS

static const struct dlt_name {
    const char* name;
    int dlt;
} dlt_names[] = {
    { "NULL", FAKE_DLT_NULL },
    { "IP", FAKE_DLT_RAW },
    { "ETHER", FAKE_DLT_EN10MB },
    { "FDDI", FAKE_DLT_FDDI },
    { "ATM", FAKE_DLT_ATM_RFC1483 },
    { "RFC1483", FAKE_DLT_ATM_RFC1483 },
    { "ATM_RFC1483", FAKE_DLT_ATM_RFC1483 },
    { "802_11", FAKE_DLT_IEEE802_11 },
    { "802.11", FAKE_DLT_IEEE802_11 },
    { "802_11_RADIO", FAKE_DLT_IEEE802_11_RADIO },
    { "802.11_RADIO", FAKE_DLT_IEEE802_11_RADIO },
    { "SLL", FAKE_DLT_LINUX_SLL },
    { "AIRONET", FAKE_DLT_AIRONET_HEADER },
    { "HDLC", FAKE_DLT_C_HDLC },
    { "PPP_HDLC", FAKE_DLT_PPP_HDLC },
    { "PPP", FAKE_DLT_PPP },
    { "SUNATM", FAKE_DLT_SUNATM },
    { "PRISM", FAKE_DLT_PRISM_HEADER }
};

int
fake_pcap_parse_dlt(const String &str)
{
    for (const dlt_name* d = dlt_names; d < dlt_names + (sizeof(dlt_names) / sizeof(dlt_names[0])); d++)
	if (str == d->name)
	    return d->dlt;
    uint32_t dlt = 0;
    if (str.length() >= 2 && str[0] == '#' && IntArg().parse(str.substring(1), dlt) && dlt < 0x7FFFFFFF)
	return dlt;
    else
	return -1;
}

String
fake_pcap_unparse_dlt(int dlt)
{
    for (const dlt_name* d = dlt_names; d < dlt_names + (sizeof(dlt_names) / sizeof(dlt_names[0])); d++)
	if (dlt == d->dlt)
	    return String::make_stable(d->name);
    if (dlt < 0)
	return String::make_stable("<none>");
    return "#" + String(dlt);
}

// Handling FORCE_IP.

bool
fake_pcap_dlt_force_ipable(int dlt)
{
    return (dlt == FAKE_DLT_RAW || dlt == FAKE_DLT_HOST_RAW
	    || dlt == FAKE_DLT_EN10MB || dlt == FAKE_DLT_SUNATM
	    || dlt == FAKE_DLT_FDDI || dlt == FAKE_DLT_ATM_RFC1483
	    || dlt == FAKE_DLT_LINUX_SLL || dlt == FAKE_DLT_C_HDLC
	    || dlt == FAKE_DLT_IEEE802_11 || dlt == FAKE_DLT_PRISM_HEADER
	    || dlt == FAKE_DLT_PPP_HDLC || dlt == FAKE_DLT_PPP
	    || dlt == FAKE_DLT_NULL || dlt == FAKE_DLT_IEEE802_11_RADIO);
}

int
fake_pcap_canonical_dlt(int dlt, bool)
{
    if (dlt == FAKE_DLT_HOST_RAW)
	return FAKE_DLT_RAW;
    else
	return dlt;
}

#if HAVE_INDIFFERENT_ALIGNMENT
#define unaligned_net_short(v) (ntohs(*reinterpret_cast<const uint16_t*>(v)))
#define UNALIGNED_NET_SHORT_EQ(x, y) ((x) == htons((y)))
#else
static inline uint16_t
unaligned_net_short(const void *v)
{
    const uint8_t *d = reinterpret_cast<const uint8_t *>(v);
    return (d[0] << 8) | d[1];
}
#define UNALIGNED_NET_SHORT_EQ(x, y) (unaligned_net_short(&(x)) == (y))
#endif

#define IP_ETHERTYPE(et)	(UNALIGNED_NET_SHORT_EQ((et), ETHERTYPE_IP) || UNALIGNED_NET_SHORT_EQ((et), ETHERTYPE_IP6))


// NB: May change 'p', but will never free it.
bool
fake_pcap_force_ip(Packet *&p, int dlt)
{
    const click_ip *iph = 0;
    const uint8_t *data = p->data();
    const uint8_t *end_data = p->end_data();

    switch (dlt) {

      case FAKE_DLT_RAW:
      case FAKE_DLT_HOST_RAW: {
	  iph = reinterpret_cast<const click_ip*>(data);
	  break;
      }

      ethernet:
      case FAKE_DLT_EN10MB: {
	  const click_ether* ethh = reinterpret_cast<const click_ether*>(data);
	  if (data + sizeof(click_ether) <= end_data) {
	      if (IP_ETHERTYPE(ethh->ether_type))
		  iph = reinterpret_cast<const click_ip*>(ethh + 1);
	      else if (UNALIGNED_NET_SHORT_EQ(ethh->ether_type, ETHERTYPE_8021Q)
		       && data + sizeof(click_ether_vlan) <= end_data) {
		  // XXX don't handle 802.1Q-in-802.1Q
		  const click_ether_vlan* ethvh = reinterpret_cast<const click_ether_vlan*>(ethh);
		  if (IP_ETHERTYPE(ethvh->ether_vlan_encap_proto))
		      iph = reinterpret_cast<const click_ip*>(ethvh + 1);
	      }
	  }
	  break;
      }

      fddi:
      case FAKE_DLT_FDDI: {
	  const click_fddi* fh = reinterpret_cast<const click_fddi*>(data);
	  if (data + sizeof(click_fddi_snap) > end_data
	      || (fh->fc & FDDI_FC_LLCMASK) != FDDI_FC_LLC_ASYNC)
	      break;
	  data = reinterpret_cast<const uint8_t*>(fh + 1);
	  goto rfc1483;
      }

      case FAKE_DLT_SUNATM:
	data += 4;
	goto rfc1483;

      rfc1483:
      case FAKE_DLT_ATM_RFC1483: {
	  const click_rfc1483* rh = reinterpret_cast<const click_rfc1483*>(data);
	  if (data + sizeof(click_rfc1483) <= end_data
	      && memcmp(&rh->dsap, RFC1483_SNAP_IP_EXPECTED, RFC1483_SNAP_IP_EXPECTED_LEN) == 0
	      && IP_ETHERTYPE(rh->ether_type))
	      iph = reinterpret_cast<const click_ip*>(rh + 1);
	  else if (data + 4 <= end_data
		   && rh->dsap == LLC_IP_LSAP && rh->ssap == LLC_IP_LSAP)
	      iph = reinterpret_cast<const click_ip*>(data + 4);
	  else if (data + sizeof(click_rfc1483) <= end_data
		   && rh->dsap == LLC_SNAP_LSAP && rh->ssap == LLC_SNAP_LSAP) {
#define	OUI_ENCAP_ETHER	0x000000	/* encapsulated Ethernet */
#define	OUI_CISCO_90	0x0000f8	/* Cisco bridging */
#define OUI_RFC2684	0x0080c2	/* RFC 2684 bridged Ethernet */
#define PID_RFC2684_ETH_FCS	0x0001	/* Ethernet, with FCS */
#define PID_RFC2684_ETH_NOFCS	0x0007	/* Ethernet, without FCS */
#define PID_RFC2684_FDDI_FCS	0x0004	/* FDDI, with FCS */
#define PID_RFC2684_FDDI_NOFCS	0x000a	/* FDDI, without FCS */
	      uint32_t orgcode = (rh->orgcode[0]<<16) + (rh->orgcode[1]<<8) + rh->orgcode[2];
	      if (orgcode == OUI_ENCAP_ETHER || orgcode == OUI_CISCO_90) {
		  data = reinterpret_cast<const uint8_t*>(&rh->ether_type) - 12;
		  goto ethernet;
	      } else if (orgcode == OUI_RFC2684) {
		  uint32_t ethertype = unaligned_net_short(&rh->ether_type);
		  if (ethertype == PID_RFC2684_ETH_FCS || ethertype == PID_RFC2684_ETH_NOFCS) {
		      data = reinterpret_cast<const uint8_t*>(rh + 1);
		      goto ethernet;
		  } else if (ethertype == PID_RFC2684_FDDI_FCS || ethertype == PID_RFC2684_FDDI_NOFCS) {
		      data = reinterpret_cast<const uint8_t*>(rh + 1) + 1;
		      goto fddi;
		  }
	      }
	  }
	  break;
      }

      case FAKE_DLT_LINUX_SLL: {
	  struct click_linux_sll {
	      uint16_t sll_pkttype;
	      uint16_t sll_hatype;
	      uint16_t sll_halen;
	      uint8_t sll_addr[8];
	      uint16_t sll_protocol;
	  } CLICK_SIZE_PACKED_ATTRIBUTE;
	  const click_linux_sll* sllh = reinterpret_cast<const click_linux_sll*>(data);
	  if (data + sizeof(click_linux_sll) <= end_data &&
	      IP_ETHERTYPE(sllh->sll_protocol))
	      iph = reinterpret_cast<const click_ip*>(sllh + 1);
	  break;
      }

      c_hdlc:
      case FAKE_DLT_C_HDLC: {
	  struct click_pcap_hdlc {
	      uint16_t hdlc_address;
	      uint16_t hdlc_protocol;
	  };
	  const click_pcap_hdlc* hdlch = reinterpret_cast<const click_pcap_hdlc*>(data);
	  if (data + sizeof(click_pcap_hdlc) <= end_data
	      && IP_ETHERTYPE(hdlch->hdlc_protocol))
	      iph = reinterpret_cast<const click_ip*>(hdlch + 1);
	  break;
      }

      case FAKE_DLT_PPP_HDLC: {
	  if (data + 4 > end_data)
	      /* nada */;
	  else if (data[0] == PPP_ADDRESS) {
	      if (data[2] == 0 && (data[3] == PPP_IP || data[3] == PPP_IPV6))
		  iph = reinterpret_cast<const click_ip*>(data + 4);
	  } else if (data[0] == 0x0F /* CHDLC_UNICAST */
		     || data[0] == 0x8F /* CHDLC_BCAST */)
	      goto c_hdlc;
	  break;
      }

      case FAKE_DLT_PPP: {
	  if (data + 2 <= end_data && data[0] == PPP_ADDRESS && data[1] == PPP_CONTROL)
	      data += 2;
	  if (data + 2 > end_data)
	      /* nada */;
	  else if (data[0] == PPP_IP || data[0] == PPP_IPV6)
	      iph = reinterpret_cast<const click_ip*>(data + 1);
	  else if (data[0] == 0 && (data[1] == PPP_IP || data[1] == PPP_IPV6))
	      iph = reinterpret_cast<const click_ip*>(data + 2);
	  break;
      }

      case FAKE_DLT_PRISM_HEADER:
	data += 144;
	goto ieee802_11;

      ieee802_11:
      case FAKE_DLT_IEEE802_11:
	if (data + 24 <= end_data
	    && (data[0] & WIFI_FC0_TYPE_MASK) == WIFI_FC0_TYPE_DATA) {
	    data += ((data[1] & 0x03) == 0x03 ? 30 : 24);
	    goto rfc1483;
	}
	break;

      case FAKE_DLT_IEEE802_11_RADIO: {
	  uint16_t len;
	  if (data + 4 <= end_data
	      && (len = (data[3] << 8) | (data[2])) >= 8) {
	      data += len;
	      goto ieee802_11;
	  }
	  break;
      }

      case FAKE_DLT_NULL: {
	  if (data + 4 > end_data)
	      break;
	  int family = data[0] | (data[1] << 8);
	  if (family == 0)
	      family = (data[2] << 8) | data[3];
	  if (family == 2 /* BSD_AF_INET */
	      || family == 24 /* BSD_AF_INET6_BSD */
	      || family == 28 /* BSD_AF_INET6_FREEBSD */
	      || family == 30 /* BSD_AF_INET6_DARWIN */)
	      iph = reinterpret_cast<const click_ip*>(data + 4);
	  break;
      }

      default:
	break;

    }

    if (!iph)
	return false;

#if !HAVE_INDIFFERENT_ALIGNMENT
    // Machine may crash if we try to access 'iph'. Align it on a word
    // boundary.
    uintptr_t header_ptr = reinterpret_cast<uintptr_t>(iph);
    if (header_ptr & 3) {
	int header_off = header_ptr - reinterpret_cast<uintptr_t>(p->data());
	if (Packet *q = p->shift_data(-(header_ptr & 3), false)) {
	    p = q;
	    iph = reinterpret_cast<const click_ip *>(q->data() + header_off);
	    end_data = p->end_data();
	} else			// cannot align; return it as a non-IP packet
	    return false;
    }
#endif

    if (iph->ip_v == 4) {
	if (iph->ip_hl >= 5
	    && reinterpret_cast<const uint8_t*>(iph) + (iph->ip_hl << 2) <= end_data) {
	    p->set_ip_header(iph, iph->ip_hl << 2);
	    p->set_dst_ip_anno(iph->ip_dst);
	    return true;
	}
    } else if (iph->ip_v == 6) {
	if (reinterpret_cast<const uint8_t*>(iph) + sizeof(click_ip6) <= end_data) {
	    p->set_ip6_header(reinterpret_cast<const click_ip6*>(iph));
	    return true;
	}
    }

    return false;
}

CLICK_ENDDECLS
ELEMENT_REQUIRES(userlevel|ns)
ELEMENT_PROVIDES(FakePcap)
