// -*- mode: c++; c-basic-offset: 4 -*-
/*
 * ipsumdump_payload.{cc,hh} -- IP network layer IP summary dump unparsers
 * Eddie Kohler
 *
 * Copyright (c) 2002 International Computer Science Institute
 * Copyright (c) 2004-2008 Regents of the University of California
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

#include "ipsumdump_payload.hh"
#include <click/packet.hh>
#include <click/packet_anno.hh>
#include <click/md5.h>
#include <clicknet/ip.h>
#include <clicknet/tcp.h>
#include <clicknet/udp.h>
#include <click/confparse.hh>
#include <click/ipflowid.hh>
CLICK_DECLS

namespace IPSummaryDump {

enum { T_PAYLOAD_LEN, T_PAYLOAD, T_PAYLOAD_MD5, T_PAYLOAD_MD5_HEX };

static void payload_info(const Packet *p, const click_ip *iph, int tailpad,
                         int32_t &off, uint32_t &len)
{
    // Set `off` to the most likely offset to the start of payload data (might
    // be > p->length()), and `len` to the most likely length of payload data
    if (iph) {
        len = ntohs(iph->ip_len); // IP length
        off = p->transport_header_offset();
        if (IP_FIRSTFRAG(iph))
            switch (iph->ip_p) {
            case IP_PROTO_TCP:
                if (p->transport_length() - tailpad >= 13
                    && (unsigned) (p->tcp_header()->th_off << 2) >= sizeof(click_tcp))
                    off += p->tcp_header()->th_off << 2;
                else
                    off += sizeof(click_tcp);
                break;
            case IP_PROTO_UDP:
            case IP_PROTO_UDPLITE:
                off += sizeof(click_udp);
                break;
            }
        len -= off - p->network_header_offset();
    } else {
        off = 0;
        len = p->length() - tailpad;
    }
}

static bool payload_extract(PacketDesc &d, const FieldWriter *f)
{
    payload_info(d.p, d.iph, d.tailpad, d.i32[1], d.v);
    if (f->user_data == T_PAYLOAD_LEN) {
        if (!d.iph || d.force_extra_length)
            d.v += EXTRA_LENGTH_ANNO(d.p);
        return true;
    } else
        return d.v == 0 || (unsigned) d.i32[1] < d.p->length();
}

static void account_payload_len(PacketOdesc &d, int32_t &off, uint32_t plen)
{
    if (!d.is_ip || (d.p->ip_header()->ip_len == 0 && d.want_len == 0))
        d.want_len = off + plen;
    else {
        click_ip *iph = d.p->ip_header();
        click_tcp *tcph;
        uint32_t ip_len = (iph->ip_len ? ntohs(iph->ip_len) : d.want_len)
            + d.p->network_header_offset();
        int delta = ip_len - (off + plen);

        if (delta > 0 && IP_FIRSTFRAG(iph) && iph->ip_p == IP_PROTO_TCP
            && !d.have_tcp_hl && (tcph = d.p->tcp_header())
            && tcph->th_off == (sizeof(click_tcp) >> 2)) {
            int th_delta = delta - (delta & 3);
            if (th_delta + sizeof(click_tcp) > (15 << 2))
                th_delta = (15 << 2) - sizeof(click_tcp);
            if (!(d.p = d.p->put(th_delta)))
                return;
            iph = d.p->ip_header(); // may have shifted
            unsigned char *tx = d.p->transport_header() + sizeof(click_tcp);
            memmove(tx + th_delta, tx, d.p->end_data() - (tx + th_delta));
            memset(tx, TCPOPT_EOL, th_delta);
            d.p->tcp_header()->th_off = (sizeof(click_tcp) + th_delta) >> 2;
            delta -= th_delta;
        }

        if (delta > 0 && !d.have_ip_hl && iph->ip_hl == (sizeof(click_ip) >> 2)) {
            int ip_delta = delta - (delta & 3);
            if (ip_delta + sizeof(click_ip) > (15 << 2))
                ip_delta = (15 << 2) - sizeof(click_ip);
            if (!(d.p = d.p->put(ip_delta)))
                return;
            iph = d.p->ip_header(); // may have shifted
            unsigned char *nx = d.p->network_header() + sizeof(click_ip);
            memmove(nx + ip_delta, nx, d.p->end_data() - (nx + ip_delta));
            memset(nx, IPOPT_EOL, ip_delta);
            iph->ip_hl = (sizeof(click_ip) + ip_delta) >> 2;
            d.p->set_ip_header(iph, sizeof(click_ip) + ip_delta);
        }
    }
}

static void payload_inject(PacketOdesc &d, const FieldReader *f)
{
    if (d.make_ip(0))           // add default IPFlowID and protocol if nec.
        d.make_transp();        // don't care if we fail
    if (!d.p)
        return;

    int32_t off;
    uint32_t len;
    payload_info(d.p, d.is_ip ? d.p->ip_header() : 0, 0, off, len);
    switch (f->user_data) {
    case T_PAYLOAD: {
        if (!d.vptr[0] || d.vptr[0] == d.vptr[1])
            return;
        uint32_t plen = d.vptr[1] - d.vptr[0];
        account_payload_len(d, off, plen);
        if (!d.p || (d.p->length() - off < plen
                     && !(d.p = d.p->put(plen - (d.p->length() - off)))))
            return;
        memcpy(d.p->data() + off, d.vptr[0], plen);
        break;
    }
    case T_PAYLOAD_LEN:
        account_payload_len(d, off, d.v);
        break;
    }
}

static void payload_outa(const PacketDesc& d, const FieldWriter *f)
{
    int32_t off = d.i32[1];
    uint32_t len = d.v;
    if (off + len > d.length())
        len = d.length() - off;
    if (f->user_data == T_PAYLOAD) {
        String s = String::make_stable((const char *)(d.p->data() + off), len);
        *d.sa << cp_quote(s);
    } else {
        md5_state_t pms;
        md5_init(&pms);
        md5_append(&pms, (const md5_byte_t *) (d.p->data() + off), len);

        if (f->user_data == T_PAYLOAD_MD5_HEX) {
            if (char *buf = d.sa->extend(MD5_DIGEST_SIZE * 2)) {
                md5_finish(&pms, (md5_byte_t *) buf);
                const char digits[] = "0123456789abcdef";
                for (int i = MD5_DIGEST_SIZE - 1; i >= 0; --i) {
                    int b = (unsigned char) buf[i];
                    buf[2*i + 1] = digits[b & 15];
                    buf[2*i] = digits[b >> 4];
                }
            }
        } else {
            if (char *buf = d.sa->extend(MD5_TEXT_DIGEST_SIZE))
                md5_finish_text(&pms, buf, 1);
        }

        md5_free(&pms);
    }
}

static bool payload_ina(PacketOdesc& d, const String &str, const FieldReader *f)
{
    switch (f->user_data) {
    case T_PAYLOAD: {
        String s;
        if (cp_string(str, &s)) {
            d.sa.clear();
            d.sa << s;
            d.vptr[0] = (const uint8_t *) d.sa.begin();
            d.vptr[1] = (const uint8_t *) d.sa.end();
            return true;
        }
        break;
    }
    }
    return false;
}

static void payload_outb(const PacketDesc& d, bool, const FieldWriter*)
{
    int32_t off = d.i32[1];
    uint32_t len = d.v;
    if (off + len > (uint32_t) d.length())
        len = d.length() - off;
    md5_state_t pms;
    md5_init(&pms);
    md5_append(&pms, (const md5_byte_t *) (d.p->data() + off), len);
    if (char *buf = d.sa->extend(MD5_DIGEST_SIZE))
        md5_finish(&pms, (md5_byte_t *) buf);
    md5_free(&pms);
}

static const FieldWriter payload_writers[] = {
    { "payload_len", B_4, T_PAYLOAD_LEN,
      ip_prepare, payload_extract, num_outa, outb },
    { "payload", B_NOTALLOWED, T_PAYLOAD,
      ip_prepare, payload_extract, payload_outa, 0 },
    { "payload_md5", B_16, T_PAYLOAD_MD5,
      ip_prepare, payload_extract, payload_outa, payload_outb },
    { "payload_md5_hex", B_16, T_PAYLOAD_MD5_HEX,
      ip_prepare, payload_extract, payload_outa, payload_outb }
};

static const FieldReader payload_readers[] = {
    { "payload_len", B_4, T_PAYLOAD_LEN, order_payload + 1,
      num_ina, inb, payload_inject },
    { "payload", B_NOTALLOWED, T_PAYLOAD, order_payload,
      payload_ina, 0, payload_inject }
};

static const FieldSynonym payload_synonyms[] = {
    { "payload_length", "payload_len" }
};

}

void IPSummaryDump_Payload::static_initialize()
{
    using namespace IPSummaryDump;
    for (size_t i = 0; i < sizeof(payload_writers) / sizeof(payload_writers[0]); ++i)
        FieldWriter::add(&payload_writers[i]);
    for (size_t i = 0; i < sizeof(payload_readers) / sizeof(payload_readers[0]); ++i)
        FieldReader::add(&payload_readers[i]);
    for (size_t i = 0; i < sizeof(payload_synonyms) / sizeof(payload_synonyms[0]); ++i)
        FieldSynonym::add(&payload_synonyms[i]);
}

void IPSummaryDump_Payload::static_cleanup()
{
    using namespace IPSummaryDump;
    for (size_t i = 0; i < sizeof(payload_writers) / sizeof(payload_writers[0]); ++i)
        FieldWriter::remove(&payload_writers[i]);
    for (size_t i = 0; i < sizeof(payload_readers) / sizeof(payload_readers[0]); ++i)
        FieldReader::remove(&payload_readers[i]);
    for (size_t i = 0; i < sizeof(payload_synonyms) / sizeof(payload_synonyms[0]); ++i)
        FieldSynonym::remove(&payload_synonyms[i]);
}

ELEMENT_REQUIRES(userlevel IPSummaryDump)
ELEMENT_PROVIDES(IPSummaryDump_Payload)
CLICK_ENDDECLS
