// -*- mode: c++; c-basic-offset: 4 -*-
#ifndef CLICK_IPSUMDUMPINFO_HH
#define CLICK_IPSUMDUMPINFO_HH
#include <click/string.hh>
#include <click/straccum.hh>
class Packet;
struct click_ip;
struct click_udp;
struct click_tcp;
CLICK_DECLS

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
    
    uint32_t v;
    uint32_t v2;
    
    StringAccum* sa;
    StringAccum* bad_sa;
    bool careful_trunc;
    
    inline PacketDesc(Packet*, StringAccum* sa, StringAccum* bad_sa, bool careful_trunc);
    void clear_values()			{ v = v2 = 0; }
};

struct Field {
    const char* name;
    int thunk;
    void (*prepare)(PacketDesc&);
    bool (*extract)(PacketDesc&, int);
    void (*outa)(const PacketDesc&, int);
    void (*outb)(const PacketDesc&, bool ok, int);
    Field* synonym;
    Field* next;
    int binary_size() const;
};

extern const Field null_field;
const Field* find_field(const String&, bool likely_synonyms = true);

int register_unparser(const char* name, int thunk, void (*prepare)(PacketDesc&), bool (*extract)(PacketDesc&, int), void (*outa)(const PacketDesc&, int), void (*outb)(const PacketDesc&, bool, int));
int register_synonym(const char* name, const char* synonym);

void num_outa(const PacketDesc&, int);

enum { B_TYPEMASK = 0x70000000,
       B_0 = 0x00000000,
       B_1 = 0x10000000,
       B_2 = 0x20000000,
       B_4 = 0x30000000,
       B_8 = 0x40000000,
       B_4NET = 0x50000000,
       B_SPECIAL = 0x60000000,
       B_NOTALLOWED = 0x70000000 };
void outb(const PacketDesc&, bool ok, int);

enum { MISSING_IP = 0,
       MISSING_IP_TRANSPORT = 1 };
bool field_missing(const PacketDesc&, int what, const char* header_name, int l);

// particular parsers
void ip_prepare(PacketDesc&);

void anno_register_unparsers();
void ip_register_unparsers();
void tcp_register_unparsers();

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

extern const char* const tcp_flags_word;
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

inline PacketDesc::PacketDesc(Packet* p_, StringAccum* sa_, StringAccum* bad_sa_, bool careful_trunc_)
    : p(p_), iph(0), udph(0), tcph(0), sa(sa_), bad_sa(bad_sa_),
      careful_trunc(careful_trunc_)
{
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
    W_IP_CAPTURE_LEN, W_TCP_URP, W_NTIMESTAMP, W_FIRST_NTIMESTAMP, W_LAST
};
static int parse_content(const String &);
static int content_binary_size(int);

};

CLICK_ENDDECLS
#endif
