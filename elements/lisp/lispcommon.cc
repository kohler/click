/*
 * lispcommon.{cc,hh} -- code shared by the LISP elements: IP/UDP header builder,
 * prefix tables, locator choice, SHA-256 and HMAC, locking
 * Franck Messaoudi
 *
 * Copyright (c) 2014-2026 Franck Messaoudi
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
#include "lispcommon.hh"
#include <click/args.hh>
#include <click/nameinfo.hh>
#include <click/error.hh>
CLICK_DECLS

namespace {

const uint32_t K[64] = {0x428a2f98, 0x71374491, 0xb5c0fbcf, 0xe9b5dba5,
    0x3956c25b, 0x59f111f1, 0x923f82a4, 0xab1c5ed5, 0xd807aa98, 0x12835b01,
    0x243185be, 0x550c7dc3, 0x72be5d74, 0x80deb1fe, 0x9bdc06a7, 0xc19bf174,
    0xe49b69c1, 0xefbe4786, 0x0fc19dc6, 0x240ca1cc, 0x2de92c6f, 0x4a7484aa,
    0x5cb0a9dc, 0x76f988da, 0x983e5152, 0xa831c66d, 0xb00327c8, 0xbf597fc7,
    0xc6e00bf3, 0xd5a79147, 0x06ca6351, 0x14292967, 0x27b70a85, 0x2e1b2138,
    0x4d2c6dfc, 0x53380d13, 0x650a7354, 0x766a0abb, 0x81c2c92e, 0x92722c85,
    0xa2bfe8a1, 0xa81a664b, 0xc24b8b70, 0xc76c51a3, 0xd192e819, 0xd6990624,
    0xf40e3585, 0x106aa070, 0x19a4c116, 0x1e376c08, 0x2748774c, 0x34b0bcb5,
    0x391c0cb3, 0x4ed8aa4a, 0x5b9cca4f, 0x682e6ff3, 0x748f82ee, 0x78a5636f,
    0x84c87814, 0x8cc70208, 0x90befffa, 0xa4506ceb, 0xbef9a3f7, 0xc67178f2};

inline uint32_t
ror(uint32_t x, int n)
{
    return (x >> n) | (x << (32 - n));
}

struct Sha256 {
    uint32_t h[8];
    uint8_t buf[64];
    uint64_t total;
    unsigned used;

    Sha256()
      : total(0), used(0)
    {
	static const uint32_t init[8] = {0x6a09e667, 0xbb67ae85, 0x3c6ef372,
	    0xa54ff53a, 0x510e527f, 0x9b05688c, 0x1f83d9ab, 0x5be0cd19};
	memcpy(h, init, sizeof(h));
    }
    void block(const uint8_t *p)
    {
	uint32_t w[64];
	for (int i = 0; i < 16; ++i)
	    w[i] = (p[4 * i] << 24) | (p[4 * i + 1] << 16) | (p[4 * i + 2] << 8)
		| p[4 * i + 3];
	for (int i = 16; i < 64; ++i) {
	    uint32_t s0
		= ror(w[i - 15], 7) ^ ror(w[i - 15], 18) ^ (w[i - 15] >> 3);
	    uint32_t s1
		= ror(w[i - 2], 17) ^ ror(w[i - 2], 19) ^ (w[i - 2] >> 10);
	    w[i] = w[i - 16] + s0 + w[i - 7] + s1;
	}
	uint32_t a = h[0], b = h[1], c = h[2], d = h[3], e = h[4], f = h[5],
		 g = h[6], hh = h[7];
	for (int i = 0; i < 64; ++i) {
	    uint32_t t1 = hh + (ror(e, 6) ^ ror(e, 11) ^ ror(e, 25))
		+ ((e & f) ^ (~e & g)) + K[i] + w[i];
	    uint32_t t2 = (ror(a, 2) ^ ror(a, 13) ^ ror(a, 22))
		+ ((a & b) ^ (a & c) ^ (b & c));
	    hh = g;
	    g = f;
	    f = e;
	    e = d + t1;
	    d = c;
	    c = b;
	    b = a;
	    a = t1 + t2;
	}
	h[0] += a;
	h[1] += b;
	h[2] += c;
	h[3] += d;
	h[4] += e;
	h[5] += f;
	h[6] += g;
	h[7] += hh;
    }
    void update(const uint8_t *p, size_t n)
    {
	total += n;
	while (n) {
	    size_t k = 64 - used < n ? 64 - used : n;
	    memcpy(buf + used, p, k);
	    used += k;
	    p += k;
	    n -= k;
	    if (used == 64) {
		block(buf);
		used = 0;
	    }
	}
    }
    void final(uint8_t out[32])
    {
	uint64_t bits = total * 8;
	uint8_t pad = 0x80, zero = 0;
	update(&pad, 1);
	while (used != 56)
	    update(&zero, 1);
	uint8_t len[8];
	for (int i = 0; i < 8; ++i)
	    len[i] = bits >> (56 - 8 * i);
	update(len, 8);
	for (int i = 0; i < 8; ++i) {
	    out[4 * i] = h[i] >> 24;
	    out[4 * i + 1] = h[i] >> 16;
	    out[4 * i + 2] = h[i] >> 8;
	    out[4 * i + 3] = h[i];
	}
    }
};

} // namespace

void
lisp_sha256(const uint8_t *data, size_t len, uint8_t out[32])
{
    Sha256 s;
    s.update(data, len);
    s.final(out);
}

void
lisp_hmac_sha256(const uint8_t *key, size_t keylen, const uint8_t *data,
    size_t len, uint8_t out[32])
{
    uint8_t k[64], pad[64], inner[32];
    memset(k, 0, sizeof(k));
    if (keylen > 64)
	lisp_sha256(key, keylen, k);
    else
	memcpy(k, key, keylen);
    Sha256 in;
    for (int i = 0; i < 64; ++i)
	pad[i] = k[i] ^ 0x36;
    in.update(pad, 64);
    in.update(data, len);
    in.final(inner);
    Sha256 outer;
    for (int i = 0; i < 64; ++i)
	pad[i] = k[i] ^ 0x5c;
    outer.update(pad, 64);
    outer.update(inner, 32);
    outer.final(out);
}

void
lisp_auth_sign(uint8_t *msg, size_t len, size_t auth_off, const String &key)
{
    uint8_t mac[32];
    memset(msg + auth_off, 0, LISP_AUTH_LEN_HMAC_SHA_256_128);
    lisp_hmac_sha256(reinterpret_cast<const uint8_t *>(key.data()),
	key.length(), msg, len, mac);
    memcpy(msg + auth_off, mac, LISP_AUTH_LEN_HMAC_SHA_256_128);
}

bool
lisp_auth_verify(
    const uint8_t *msg, size_t len, size_t auth_off, const String &key)
{
    if (auth_off + LISP_AUTH_LEN_HMAC_SHA_256_128 > len)
	return false;
    uint8_t copy_mac[LISP_AUTH_LEN_HMAC_SHA_256_128], mac[32];
    memcpy(copy_mac, msg + auth_off, sizeof(copy_mac));
    // compute over a copy with the authentication field set to zero
    uint8_t *tmp = new uint8_t[len];
    memcpy(tmp, msg, len);
    memset(tmp + auth_off, 0, sizeof(copy_mac));
    lisp_hmac_sha256(reinterpret_cast<const uint8_t *>(key.data()),
	key.length(), tmp, len, mac);
    delete[] tmp;
    uint8_t diff = 0; // constant-time compare
    for (unsigned i = 0; i < sizeof(copy_mac); ++i)
	diff |= copy_mac[i] ^ mac[i];
    return diff == 0;
}

/* ---- LISPIPHeader ---- */

LISPIPHeader::LISPIPHeader()
  : _use_dst_anno(false),
    _proto(IP_PROTO_UDP),
    _tos(-1),
    _dscp(-1),
    _ce(false),
    _df(false)
{
    memset(&_iph, 0, sizeof(_iph));
    _iph.ip_v = 4;
    _iph.ip_hl = sizeof(click_ip) >> 2;
    _iph.ip_ttl = 250;
    _iph.ip_p = IP_PROTO_UDP;
    _id = 0;
}

void
LISPIPHeader::read_args(Args &args)
{
    args.read_mp("PROTO", NamedIntArg(NameInfo::T_IP_PROTO), _proto)
	.read_mp("SRC", _iph.ip_src)
	.read_mp("DST", AnyArg(), _dst_str)
	.read("TOS", _tos)
	.read("TTL", _iph.ip_ttl)
	.read("DSCP", _dscp)
	.read("ECT", KeywordArg(), _ect_str)
	.read("CE", _ce)
	.read("DF", _df);
}

int
LISPIPHeader::finish_configure(Element *e, ErrorHandler *errh)
{
    if (_proto != IP_PROTO_UDP)
	errh->warning(
	    "PROTO %d ignored: LISP packets are always UDP (17)", _proto);
    _iph.ip_p = IP_PROTO_UDP;

    _use_dst_anno = _dst_str == "DST_ANNO";
    if (_use_dst_anno)
	_iph.ip_dst.s_addr = 0;
    else if (!IPAddressArg().parse(_dst_str, _iph.ip_dst, e))
	return errh->error("DST argument should be IP address or 'DST_ANNO'");

    int ect = 0;
    if (_ect_str) {
	bool x;
	if (BoolArg().parse(_ect_str, x))
	    ect = x;
	else if (_ect_str == "2")
	    ect = 2;
	else
	    return errh->error("bad ECT value '%s'", _ect_str.c_str());
    }
    if (_tos >= 0 && _dscp >= 0)
	return errh->error("cannot set both TOS and DSCP");
    else if (_tos >= 0 && (ect || _ce))
	return errh->error("cannot set both TOS and ECN bits");
    if (_tos >= 256 || _tos < -1)
	return errh->error("TOS too large; max 255");
    else if (_dscp >= 64 || _dscp < -1)
	return errh->error("DSCP too large; max 63");
    if (ect && _ce)
	return errh->error("can set at most one ECN option");
    if (_tos >= 0)
	_iph.ip_tos = _tos;
    else if (_dscp >= 0)
	_iph.ip_tos = (_dscp << 2);
    if (ect)
	_iph.ip_tos |= (ect == 1 ? IP_ECN_ECT1 : IP_ECN_ECT2);
    if (_ce)
	_iph.ip_tos |= IP_ECN_CE;
    if (_df)
	_iph.ip_off |= htons(IP_DF);
    return 0;
}

void
LISPIPHeader::set(IPAddress src, IPAddress dst, int ttl)
{
    _iph.ip_src = src.in_addr();
    _iph.ip_dst = dst.in_addr();
    _iph.ip_ttl = ttl;
    _use_dst_anno = false;
}

void
LISPIPHeader::fill(
    WritablePacket *q, IPAddress dst, uint16_t sport, uint16_t dport)
{
    click_ip *iph = reinterpret_cast<click_ip *>(q->data());
    click_udp *udph = reinterpret_cast<click_udp *>(iph + 1);
    *iph = _iph;
    if (dst)
	iph->ip_dst = dst.in_addr();
    else if (_use_dst_anno)
	iph->ip_dst = q->dst_ip_anno().in_addr();
    q->set_dst_ip_anno(IPAddress(iph->ip_dst));
    iph->ip_len = htons(q->length());
    iph->ip_id = htons(_id.fetch_and_add(1));
    iph->ip_sum = 0;
    iph->ip_sum = click_in_cksum(
	reinterpret_cast<unsigned char *>(iph), sizeof(click_ip));
    q->set_ip_header(iph, sizeof(click_ip));
    udph->uh_sport = sport;
    udph->uh_dport = dport;
    udph->uh_ulen = htons(q->length() - sizeof(click_ip));
    udph->uh_sum = 0;
}

void
LISPIPHeader::finish_udp(WritablePacket *q)
{
    click_ip *iph = q->ip_header();
    click_udp *udph = reinterpret_cast<click_udp *>(iph + 1);
    uint16_t len = ntohs(udph->uh_ulen);
    udph->uh_sum = 0;
    unsigned csum
	= click_in_cksum(reinterpret_cast<unsigned char *>(udph), len);
    udph->uh_sum = click_in_cksum_pseudohdr(csum, iph, len);
}

CLICK_ENDDECLS
ELEMENT_PROVIDES(LISPCommon)
