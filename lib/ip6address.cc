// -*- c-basic-offset: 4; related-file-name: "../include/click/ip6address.hh" -*-
/*
 * ip6address.{cc,hh} -- an IP6 address class
 * Peilei Fan, Eddie Kohler
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
#include <click/glue.hh>
#include <click/ip6address.hh>
#include <click/ipaddress.hh>
#include <click/straccum.hh>
#include <click/confparse.hh>
#include <click/args.hh>
#if !CLICK_TOOL
# include <click/standard/addressinfo.hh>
#endif
CLICK_DECLS

IP6Address::IP6Address(const String &str)
{
    static_assert(sizeof(*this) == 16, "IPAddress has the wrong size.");
    static_assert(sizeof(struct in6_addr) == 16, "in6_addr has the wrong size.");
    static_assert(sizeof(struct click_ip6) == 40, "click_ip6 has the wrong size.");
    if (!IP6AddressArg::parse(str, *this))
	memset(&_addr, 0, sizeof(_addr));
}

IP6Address
IP6Address::make_prefix(int prefix_len)
{
    assert(prefix_len >= 0 && prefix_len <= 128);
    IP6Address a = IP6Address::uninitialized_t();
    int i;
    for (i = 0; i < 4 && prefix_len >= 32; ++i, prefix_len -= 32)
	a.data32()[i] = 0xFFFFFFFFU;
    if (i < 4 && prefix_len > 0) {
	a.data32()[i] = htonl(0xFFFFFFFFU << (32 - prefix_len));
	++i;
    }
    for (; i < 4; ++i)
	a.data32()[i] = 0;
    return a;
}

IP6Address
IP6Address::make_inverted_prefix(int prefix_len)
{
    assert(prefix_len >= 0 && prefix_len <= 128);
    IP6Address a = IP6Address::uninitialized_t();
    int i;
    for (i = 0; i < 4 && prefix_len >= 32; ++i, prefix_len -= 32)
	a.data32()[i] = 0;
    if (i < 4 && prefix_len > 0) {
	a.data32()[i] = htonl(0xFFFFFFFFU >> prefix_len);
	++i;
    }
    for (; i < 4; ++i)
	a.data32()[i] = 0xFFFFFFFFU;
    return a;
}

int
IP6Address::mask_to_prefix_len() const
{
    // check that prefix is 0xFFFFFFFF
    int word = 0;
    while (word < 4 && data32()[word] == 0xFFFFFFFFU)
	word++;
    if (word == 4)
	return 128;

    // check that suffix is zeros
    for (int zero_word = word + 1; zero_word < 4; ++zero_word)
	if (data32()[zero_word] != 0)
	    return -1;

    // check swing word
    int prefix = IPAddress(data32()[word]).mask_to_prefix_len();
    return prefix + (prefix >= 0 ? word * 32 : 0);
}

bool
IP6Address::ether_address(EtherAddress &mac) const
{
    if (has_ether_address()) {
	unsigned char *d = mac.data();
	d[0] = _addr.s6_addr[8];
	d[1] = _addr.s6_addr[9];
	d[2] = _addr.s6_addr[10];
	d[3] = _addr.s6_addr[13];
	d[4] = _addr.s6_addr[14];
	d[5] = _addr.s6_addr[15];
	return true;
    } else
	return false;
}

IPAddress
IP6Address::ip4_address() const
{
    if (is_ip4_mapped()) {
	return IPAddress(data32()[3]);
    } else
	return IPAddress();
}

void
IP6Address::unparse(StringAccum &sa) const
{
    // Unparse according to RFC 5952
    const uint32_t *a32 = data32();
    const uint16_t *a16 = data16();
    const uint8_t *a8 = data();

    // :: and IPv4 mapped addresses
    if (a32[0] == 0 && a32[1] == 0) {
	if (a32[2] == 0 && a32[3] == 0) {
	    sa.append("::", 2);
	    return;
	} else if (a32[2] == htonl(0x0000FFFFU)) {
	    sa.snprintf(23, "::ffff:%d.%d.%d.%d", a8[12], a8[13], a8[14], a8[15]);
	    return;
	}
    }

    // find the longest sequences of zero fields; if two sequences have equal
    // length, choose the first
    int zp = 0, zl = 0, lzp = 0;
    for (int p = 0; p < 8; ++p)
	if (a16[p] != 0)
	    lzp = p + 1;
	else if (p + 1 - lzp > zl) {
	    zp = lzp;
	    zl = p + 1 - lzp;
	}

    for (int p = 0; p < 8; ++p)
	if (p == zp && zl > 1) {
	    p += zl - 1;
	    sa.append("::", p == 7 ? 2 : 1);
	} else
	    sa.snprintf(5, p ? ":%x" : "%x", ntohs(a16[p]));
}

String
IP6Address::unparse() const
{
    const uint32_t *a32 = data32();
    if (a32[0] == 0 && a32[1] == 0 && a32[2] == 0 && a32[3] == 0)
	return String::make_stable("::", 2);
    else {
	StringAccum sa;
	unparse(sa);
	return sa.take_string();
    }
}

String
IP6Address::unparse_expanded() const
{
    const uint16_t *a16 = data16();
    char buf[48];
    sprintf(buf, "%x:%x:%x:%x:%x:%x:%x:%x",
	    ntohs(a16[0]), ntohs(a16[1]), ntohs(a16[2]), ntohs(a16[3]),
	    ntohs(a16[4]), ntohs(a16[5]), ntohs(a16[6]), ntohs(a16[7]));
    return String(buf);
}


// those two  methods will calculate the checksum field of ICMP6 Message.

// The checksum is the 16-bit one's complement
// of the one's complement sum of the entire ICMPv6 message starting with the
// ICMPv6 message type field, prepended with a "pseudo-header" of IPv6 header
// fields, as specified in [IPv6, section 8.1]. The Next Header value used in
// the pseudo-header is 58 (i.e. 0x3a). (NOTE: the inclusion of a pseudo-header in the
// ICMPv6 checksum is a change from IPv4; see [RFC 2460] for the rationale
// for this change.)
// A "pseudo-header" include src and dst address of ipv6 header, packet length,
// protocal field (for ICMP, it is 58) field. Packet length is the
// payloadlength from the IPv6 header, minus the length of any extension
// header present between the IPv6 header and the upper-layer header.

// The following methods only differ at how it deal with ip6 address, i.e. add 32 bit
// a time or 16 bits a time.


uint16_t
in6_fast_cksum(const struct in6_addr *saddr,
               const struct in6_addr *daddr,
               uint16_t len,
               uint8_t proto,
               uint16_t ori_csum,
               const unsigned char *addr,
               uint16_t len2)
{
	uint16_t ulen;
	uint16_t uproto;
	uint16_t answer = 0;
	uint32_t csum =0;
	uint32_t carry;
        const uint32_t *saddr32 = (const uint32_t *)&saddr->s6_addr[0];
        const uint32_t *daddr32 = (const uint32_t *)&daddr->s6_addr[0];

	//get the sum of source and destination address
	for (int i=0; i<4; i++) {

	  csum += ntohl(saddr32[i]);
	  carry = (csum < ntohl(saddr32[i]));
	  csum += carry;
	}

	for (int i=0; i<4; i++) {

	   csum += ntohl(daddr32[i]);
	   carry = (csum < ntohl(daddr32[i]));
	   csum += carry;
	}

	//get the sum of other fields:  packet length, protocal
	ulen = ntohs(len);
	csum += ulen;

	uproto = proto;
	csum += uproto;

	//get the sum of the ICMP6 package
	uint16_t nleft = ntohs(len2);
	const uint16_t *w = (const uint16_t *)addr;
	while (nleft > 1)  {
	    uint16_t w2=*w++;
	    csum += ntohs(w2);
	    nleft -=2;
	 }

	 //mop up an odd byte, if necessary
	  if (nleft == 1) {
	    *(unsigned char *)(&answer) = *(const unsigned char *)w ;
	    csum += ntohs(answer);
	  }
	  csum -= ntohs(ori_csum); //get rid of the effect of ori_csum in the calculation

	  // fold >=32-bit csum to 16-bits
	  while (csum>>16) {
	    csum = (csum & 0xffff) + (csum >> 16);
	  }

	  answer = ~csum;          // truncate to 16 bits
	  return answer;
}


//This is the slow way for in6_cksum
unsigned short
in6_cksum(const struct in6_addr *saddr,
	  const struct in6_addr *daddr,
	  uint16_t len,
	  uint8_t proto,
	  uint16_t ori_csum,
	  unsigned char *addr,
	  uint16_t len2)
{
	uint16_t ulen;
	uint16_t uproto;
	uint16_t answer = 0;
	uint32_t csum =0;
        const uint16_t *saddr16 = (const uint16_t *)&saddr->s6_addr[0];
        const uint16_t *daddr16 = (const uint16_t *)&daddr->s6_addr[0];

	//get the sum of source and destination address
	for (int i=0; i<8; i++) {
	  csum += ntohs(saddr16[i]);
	}

	for (int i=0; i<8; i++) {
	   csum += ntohs(daddr16[i]);
	}

	//get the sum of other fields:  packet length, protocal
	ulen = ntohs(len);
	csum += ulen;

	uproto = proto;
	csum += uproto;

	//get the sum of the ICMP6 package
	uint16_t nleft = ntohs(len2);
	const uint16_t *w = (const uint16_t *)addr;
	while (nleft > 1)  {
	    uint16_t w2=*w++;
	    csum += ntohs(w2);
	    nleft -=2;
	 }

	 //mop up an odd byte, if necessary
	  if (nleft == 1) {
	    *(unsigned char *)(&answer) = *(const unsigned char *)w ;
	    csum += ntohs(answer);
	  }
	  csum -= ntohs(ori_csum); //get rid of the effect of ori_csum in the calculation

	  // fold >=32-bit csum to 16-bits
	  while (csum>>16) {
	    csum = (csum & 0xffff) + (csum >> 16);
	  }

	  answer = ~csum;          // truncate to 16 bits
	  return answer;
}


const char *
IP6AddressArg::basic_parse(const String &str, IP6Address &result, const ArgContext &args)
{
    uint16_t *parts = result.data16();
    memset(parts, 255, 16);
    int d = 0, p = 0, coloncolon = -1;

    const char *begin = str.begin(), *end = str.end(), *s;
    for (s = begin; s != end; ++s) {
	int digit;
	if (*s >= '0' && *s <= '9')
	    digit = *s - '0';
	else if (*s >= 'a' && *s <= 'f')
	    digit = *s - 'a' + 10;
	else if (*s >= 'A' && *s <= 'F')
	    digit = *s - 'A' + 10;
	else if (*s == ':' && s + 1 != end && d != 7 && (p != 0 || d == 0)) {
	    if (s[1] == ':' && coloncolon < 0) {
		coloncolon = d = (p ? d + 1 : d);
		parts[d] = 0;
		++s;
	    } else if (!isxdigit((unsigned char) s[1]) || p == 0) {
		d++;
		break;
	    }
	    p = 0;
	    ++d;
	    continue;
	} else
	    break;
	if (p == 4)
	    break;
	parts[d] = (p != 0 ? parts[d] << 4 : 0) + digit;
	++p;
    }

    if (p == 0)
	--d;

    // check for IPv4 address suffix
    if ((d == 6 || (d < 6 && coloncolon >= 0)) && s != end) {
	const char *t = s;
	while (t != begin && *t != ':')
	    --t;
	IPAddress ipv4;
	if (t != begin
	    && IPAddressArg::parse(str.substring(t + 1, end), ipv4, args)) {
	    int dd = d == coloncolon ? d + 1 : d;
	    parts[dd] = (ipv4.data()[0] << 8) + ipv4.data()[1];
	    parts[dd + 1] = (ipv4.data()[2] << 8) + ipv4.data()[3];
	    d = dd + 1;
	    s = end;
	}
    }

    if (d != 7 && coloncolon < 0)
	return begin;
    for (int i = 0; i <= d; ++i)
	parts[i] = htons(parts[i]);
    if (coloncolon >= 0 && d != 7) {
	int numextra = 7 - d;
	memmove(&parts[coloncolon + 1 + numextra], &parts[coloncolon + 1], sizeof(uint16_t) * (d - coloncolon));
	memset(&parts[coloncolon + 1], 0, sizeof(uint16_t) * numextra);
    }

    return s;
}

bool
IP6AddressArg::parse(const String &str, IP6Address &result, const ArgContext &args)
{
    IP6Address a = IP6Address::uninitialized_t();
    if (basic_parse(str, a, args) == str.end()) {
	result = a;
	return true;
    }
#if !CLICK_TOOL
    return AddressInfo::query_ip6(str, result.data(), args.context());
#else
    return false;
#endif
}

bool
IP6PrefixArg::parse(const String &str,
		    IP6Address &result_addr, int &result_prefix_len,
		    const ArgContext &args) const
{
    const char *begin = str.begin(), *slash = str.end(), *end = str.end();
    while (slash != begin)
	if (*--slash == '/')
	    break;

    if (slash != begin && slash + 1 != end) {
	IP6Address a = IP6Address::uninitialized_t();
	if (IP6AddressArg::parse(str.substring(begin, slash), a, args)) {
	    int l = -1;
	    IP6Address m = IP6Address::uninitialized_t();
	    if ((IntArg(10).parse(str.substring(slash + 1, end), l)
		 && l >= 0 && l <= 128)
		|| (IP6AddressArg::parse(str.substring(slash + 1, end), m, args)
		    && (l = m.mask_to_prefix_len()) >= 0)) {
		result_addr = a;
		result_prefix_len = l;
		return true;
	    } else
		return false;
	}
    }

    if (allow_bare_address && IP6AddressArg::parse(str, result_addr, args)) {
	result_prefix_len = 128;
	return true;
    }

#if !CLICK_TOOL
    return AddressInfo::query_ip6_prefix(str, result_addr.data(),
					 &result_prefix_len, args.context());
#else
    return false;
#endif
}

bool
IP6PrefixArg::parse(const String &str,
		    IP6Address &result_addr, IP6Address &result_prefix,
		    const ArgContext &args) const
{
    int prefix_len;
    if (parse(str, result_addr, prefix_len, args)) {
	result_prefix = IP6Address::make_prefix(prefix_len);
	return true;
    } else
	return false;
}

CLICK_ENDDECLS
