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

enum { MAJOR_VERSION = 1, MINOR_VERSION = 2 };
// MINOR_VERSION 0 has W_IP_FRAGOFF fields in multiples of 8 bytes.
// MINOR_VERSION 1 uses 'W' for TCP flag CWR (now we use 'C'), and often
// uses ':' in sack blocks.
    

struct PacketDesc {
    Packet* p;
    const click_ip* iph;
    const click_udp* udph;
    const click_tcp* tcph;
    const click_icmp *icmph;

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
};

struct PacketOdesc {
    WritablePacket* p;
    bool is_ip;
    bool have_icmp_type : 1;
    bool have_icmp_code : 1;

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
    
    inline PacketOdesc(const Element *e, WritablePacket *p, int default_ip_p, const IPFlowID *default_ip_flowid);
    void clear_values()			{ vptr[0] = vptr[1] = 0; }
    bool make_ip(int ip_p);
    bool make_transp();
  private:
    bool hard_make_ip();
    bool hard_make_transp();
};

struct Field {
    const char* name;
    int thunk;
    int order;
    void (*prepare)(PacketDesc&);
    bool (*extract)(PacketDesc&, int);
    void (*inject)(PacketOdesc&, int);
    void (*outa)(const PacketDesc&, int);
    bool (*ina)(PacketOdesc&, const String&, int);
    void (*outb)(const PacketDesc&, bool ok, int);
    const uint8_t *(*inb)(PacketOdesc&, const uint8_t*, const uint8_t*, int);
    Field* synonym;
    Field* next;
    int binary_size() const;
};

extern const Field null_field;
const Field* find_field(const String&, bool likely_synonyms = true);

enum {
    order_anno = 100,
    order_link = 200,
    order_net = 300,
    order_transp = 400,
    order_payload = 500,
    order_offset = 50
};

int register_field(const char* name, int thunk,
		   void (*prepare)(PacketDesc&),
		   int order,
		   bool (*extract)(PacketDesc&, int),
		   void (*inject)(PacketOdesc&, int),
		   void (*outa)(const PacketDesc&, int),
		   bool (*ina)(PacketOdesc&, const String&, int),
		   void (*outb)(const PacketDesc&, bool, int),
		   const uint8_t *(*inb)(PacketOdesc&, const uint8_t*, const uint8_t*, int));
int register_synonym(const char* name, const char* synonym);
void static_cleanup();

void num_outa(const PacketDesc&, int);
bool num_ina(PacketOdesc&, const String &, int);

enum {
    B_TYPEMASK = 0x7F000000,
    B_0 = 0x00000000,
    B_1 = 0x01000000,
    B_2 = 0x02000000,
    B_4 = 0x03000000,
    B_8 = 0x04000000,
    B_16 = 0x05000000,
    B_4NET = 0x06000000,
    B_SPECIAL = 0x07000000,
    B_NOTALLOWED = 0x08000000,
    B_6PTR = 0x09000000
};
void outb(const PacketDesc&, bool ok, int);
const uint8_t *inb(PacketOdesc&, const uint8_t*, const uint8_t*, int);

enum { MISSING_IP = 0,
       MISSING_ETHERNET = 260 };
inline bool field_missing(const PacketDesc &d, int proto, int l);
bool hard_field_missing(const PacketDesc &d, int proto, int l);

// particular parsers
void ip_prepare(PacketDesc&);

void anno_register_unparsers();
void link_register_unparsers();
void ip_register_unparsers();
void tcp_register_unparsers();
void udp_register_unparsers();
void icmp_register_unparsers();
void payload_register_unparsers();

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
    : p(p_), iph(0), udph(0), tcph(0), sa(sa_), bad_sa(bad_sa_),
      careful_trunc(careful_trunc_), force_extra_length(force_extra_length_),
      e(e_)
{
}

inline PacketOdesc::PacketOdesc(const Element *e_, WritablePacket* p_, int default_ip_p_, const IPFlowID *default_ip_flowid_)
    : p(p_), is_ip(true), have_icmp_type(false), have_icmp_code(false),
      e(e_), default_ip_p(default_ip_p_), default_ip_flowid(default_ip_flowid_)
{
}

inline bool PacketOdesc::make_ip(int ip_p)
{
    if ((!is_ip || !p->network_header() || p->network_length() < (int) sizeof(click_ip))
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
    
enum Content {
    W_NONE, W_TIMESTAMP, W_TIMESTAMP_SEC, W_TIMESTAMP_USEC, W_IP_SRC,
    W_IP_DST, W_IP_LEN, W_IP_PROTO, W_IP_ID, W_SPORT,
    W_DPORT, W_TCP_SEQ, W_TCP_ACK, W_TCP_FLAGS, W_PAYLOAD_LEN,
    W_COUNT, W_IP_FRAG, W_IP_FRAGOFF, W_PAYLOAD, W_LINK,
    W_AGGREGATE, W_TCP_SACK, W_TCP_OPT, W_TCP_NTOPT, W_FIRST_TIMESTAMP,
    W_TCP_WINDOW, W_IP_OPT, W_IP_TOS, W_IP_TTL, W_TIMESTAMP_USEC1,
    W_IP_CAPTURE_LEN, W_TCP_URP, W_NTIMESTAMP, W_FIRST_NTIMESTAMP, W_PAYLOAD_MD5,
    W_IP_HL, W_TCP_OFF, W_ICMP_TYPE, W_ICMP_CODE, W_LAST
};
static int parse_content(const String &);
static int content_binary_size(int);

};

CLICK_ENDDECLS
#endif
