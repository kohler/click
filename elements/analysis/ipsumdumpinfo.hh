// -*- mode: c++; c-basic-offset: 4 -*-
#ifndef CLICK_IPSUMDUMPINFO_HH
#define CLICK_IPSUMDUMPINFO_HH
#include <click/string.hh>
CLICK_DECLS

class IPSummaryDumpInfo { public:

    enum { MAJOR_VERSION = 1, MINOR_VERSION = 1 };
    
    enum Content {	// must agree with ToIPSummaryDump
	W_NONE, W_TIMESTAMP, W_TIMESTAMP_SEC, W_TIMESTAMP_USEC,	W_SRC,
	W_DST, W_LENGTH, W_PROTO, W_IPID, W_SPORT,
	W_DPORT, W_TCP_SEQ, W_TCP_ACK, W_TCP_FLAGS, W_PAYLOAD_LENGTH,
	W_COUNT, W_FRAG, W_FRAGOFF, W_PAYLOAD, W_LINK,
	W_AGGREGATE, W_TCP_SACK, W_TCP_OPT,
	W_LAST
    };
    static int parse_content(const String &);
    static const char *unparse_content(int);
    static int content_binary_size(int);

    static const char * const tcp_flags_word;
    static const uint8_t tcp_flag_mapping[256];

};

CLICK_ENDDECLS
#endif
