#ifndef CLICK_IPSUMDUMP_PAYLOAD_HH
#define CLICK_IPSUMDUMP_PAYLOAD_HH
#include "ipsumdumpinfo.hh"
CLICK_DECLS

class IPSummaryDump_Payload { public:
    static void static_initialize();
    static void static_cleanup();
};

CLICK_ENDDECLS
#endif
