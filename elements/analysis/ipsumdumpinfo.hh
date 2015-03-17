// -*- mode: c++; c-basic-offset: 4 -*-
#ifndef CLICK_IPSUMDUMPINFO_HH
#define CLICK_IPSUMDUMPINFO_HH
#include <click/string.hh>
#include <click/straccum.hh>
#include <click/packet.hh>
CLICK_DECLS
class Element;
class IPFlowID;

namespace IPSummaryDump {

enum { MAJOR_VERSION = 1, MINOR_VERSION = 3 };
// MINOR_VERSION 0 has W_IP_FRAGOFF fields in multiples of 8 bytes.
// MINOR_VERSION 1 uses 'W' for TCP flag CWR (now we use 'C'), and often
// uses ':' in sack blocks.
// MINOR_VERSION 2 can have incorrect payload MD5 checksums for some packets
// (usually short packets with link headers).


struct PacketDesc {
    const Packet *p;
    const click_ip *iph;
    const click_udp *udph;
    const click_tcp *tcph;
    const click_icmp *icmph;
    int tailpad;		// # bytes extraneous data at end of packet

    union {
	uint32_t v;
	uint32_t u32[2];
	uint8_t u8[8];
	const uint8_t *vptr[2];
    };

    StringAccum* sa;
    StringAccum* bad_sa;
    bool careful_trunc;
    bool force_extra_length;
    const Element *e;

    inline PacketDesc(const Element *e, Packet *p, StringAccum* sa, StringAccum* bad_sa, bool careful_trunc, bool force_extra_length);
    void clear_values()			{ vptr[0] = vptr[1] = 0; }

    // These accessors reduce the Packet's length by network-level padding.
    // (The Packet's length may include extraneous data at the end; e.g. a
    // 54-byte IP packet with 6 padding bytes.  'tailpad' records the amount
    // of this data.  We do not modify the packet itself.)
    const unsigned char *end_data() const {
	return p->end_data() - tailpad;
    }
    uint32_t length() const {
	return p->length() - tailpad;
    }
    uint32_t network_length() const {
	return p->network_length() - tailpad;
    }
    uint32_t transport_length() const {
	return p->transport_length() - tailpad;
    }
};

struct PacketOdesc {
    WritablePacket* p;
    bool is_ip;
    bool have_icmp_type : 1;
    bool have_icmp_code : 1;
    bool have_ip_hl : 1;
    bool have_tcp_hl : 1;

    union {
	uint32_t v;
	uint32_t u32[2];
	uint8_t u8[8];
	const uint8_t *vptr[2];
    };

    StringAccum sa;
    const Element *e;
    int default_ip_p;
    const IPFlowID *default_ip_flowid;
    int minor_version;
    uint32_t want_len;

    inline PacketOdesc(const Element *e, WritablePacket *p, int default_ip_p, const IPFlowID *default_ip_flowid, int minor_version);
    void clear_values()			{ vptr[0] = vptr[1] = 0; }
    bool make_ip(int ip_p);
    bool make_transp();
  private:
    bool hard_make_ip();
    bool hard_make_transp();
};


enum {
    B_0 = 0,
    B_1 = 1,
    B_2 = 2,
    B_4 = 4,
    B_6PTR = 6,
    B_8 = 8,
    B_16 = 16,
    B_4NET = 4 + 256,
    B_SPECIAL = 4 + 512,
    B_NOTALLOWED = -1
};

struct FieldWriter {
    const char *name;		// must come first
    int type;
    int user_data;
    void (*prepare)(PacketDesc &, const FieldWriter *);
    bool (*extract)(PacketDesc &, const FieldWriter *);
    void (*outa)(const PacketDesc &, const FieldWriter *);
    void (*outb)(const PacketDesc &, bool ok, const FieldWriter *);

    static void add(const FieldWriter *);
    static void remove(const FieldWriter *);
    static const FieldWriter *find(const String &name);

    static int binary_size(int type) {
	if (type < 0)
	    return -1;
	else
	    return type & 256;
    }
    inline int binary_size() const {
	return binary_size(type);
    }
};

struct FieldReader {
    const char *name;		// must come first
    int type;
    int user_data;
    int order;
    bool (*ina)(PacketOdesc &, const String &, const FieldReader *);
    const uint8_t *(*inb)(PacketOdesc &, const uint8_t *, const uint8_t *,
			  const FieldReader *);
    void (*inject)(PacketOdesc &, const FieldReader *);

    static void add(const FieldReader *);
    static void remove(const FieldReader *);
    static const FieldReader *find(const String &name);

    inline int binary_size() const {
	return FieldWriter::binary_size(type);
    }
};

struct FieldSynonym {
    const char *name;		// must come first
    const char *synonym;
    static void add(const FieldSynonym *);
    static void remove(const FieldSynonym *);
};

extern const FieldReader null_reader;
extern const FieldWriter null_writer;

enum {
    order_anno = 100,
    order_link = 200,
    order_net = 300,
    order_transp = 400,
    order_payload = 500,
    order_offset = 50
};

void num_outa(const PacketDesc&, const FieldWriter *);
void outb(const PacketDesc&, bool ok, const FieldWriter *);

bool num_ina(PacketOdesc&, const String &, const FieldReader *);
const uint8_t *inb(PacketOdesc&, const uint8_t*, const uint8_t*, const FieldReader *);

enum { MISSING_IP = 0,
       MISSING_ETHERNET = 260 };
inline bool field_missing(const PacketDesc &d, int proto, int l);
bool hard_field_missing(const PacketDesc &d, int proto, int l);

// particular parsers
void ip_prepare(PacketDesc &, const FieldWriter *);

enum { DO_IPOPT_PADDING = 1,
       DO_IPOPT_ROUTE = 2,
       DO_IPOPT_TS = 4,
       DO_IPOPT_UNKNOWN = 32,
       DO_IPOPT_ALL = 0xFFFFFFFFU,
       DO_IPOPT_ALL_NOPAD = 0xFFFFFFFEU };
void unparse_ip_opt(StringAccum&, const uint8_t*, int olen, int mask);
void unparse_ip_opt(StringAccum&, const click_ip*, int mask);
void unparse_ip_opt_binary(StringAccum&, const uint8_t*, int olen, int mask);
void unparse_ip_opt_binary(StringAccum&, const click_ip*, int mask);

extern const char tcp_flags_word[];
extern const uint8_t tcp_flag_mapping[256];

enum { DO_TCPOPT_PADDING = 1,
       DO_TCPOPT_MSS = 2,
       DO_TCPOPT_WSCALE = 4,
       DO_TCPOPT_SACK = 8,
       DO_TCPOPT_TIMESTAMP = 16,
       DO_TCPOPT_UNKNOWN = 32,
       DO_TCPOPT_ALL = 0xFFFFFFFFU,
       DO_TCPOPT_ALL_NOPAD = 0xFFFFFFFEU,
       DO_TCPOPT_NTALL = 0xFFFFFFEEU };
void unparse_tcp_opt(StringAccum&, const uint8_t*, int olen, int mask);
void unparse_tcp_opt(StringAccum&, const click_tcp*, int mask);
void unparse_tcp_opt_binary(StringAccum&, const uint8_t*, int olen, int mask);
void unparse_tcp_opt_binary(StringAccum&, const click_tcp*, int mask);

inline PacketDesc::PacketDesc(const Element *e_, Packet* p_, StringAccum* sa_, StringAccum* bad_sa_, bool careful_trunc_, bool force_extra_length_)
    : p(p_), iph(0), udph(0), tcph(0), tailpad(0), sa(sa_), bad_sa(bad_sa_),
      careful_trunc(careful_trunc_), force_extra_length(force_extra_length_),
      e(e_)
{
}

inline PacketOdesc::PacketOdesc(const Element *e_, WritablePacket* p_, int default_ip_p_, const IPFlowID *default_ip_flowid_, int minor_version_)
    : p(p_), is_ip(true), have_icmp_type(false), have_icmp_code(false),
      have_ip_hl(false), have_tcp_hl(false),
      e(e_), default_ip_p(default_ip_p_), default_ip_flowid(default_ip_flowid_),
      minor_version(minor_version_), want_len(0)
{
}

inline bool PacketOdesc::make_ip(int ip_p)
{
    if ((!is_ip || !p->has_network_header()
	 || p->network_length() < (int) sizeof(click_ip))
	&& !hard_make_ip())
	return false;
    return !ip_p || !p->ip_header()->ip_p || p->ip_header()->ip_p == ip_p;
}

inline bool PacketOdesc::make_transp()
{
    // assumes make_ip()
    assert(is_ip && p->network_header());
    if (!IP_FIRSTFRAG(p->ip_header()))
	return false;
    if (p->transport_length() < 8)
	return hard_make_transp();
    else
	return true;
}

inline bool field_missing(const PacketDesc &d, int proto, int l)
{
    return (d.bad_sa ? hard_field_missing(d, proto, l) : false);
}

}

class IPSummaryDumpInfo { public:
    static void static_cleanup();
};

CLICK_ENDDECLS
#endif
