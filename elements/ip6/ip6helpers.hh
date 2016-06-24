#ifndef CLICK_IP6HELPERS_HH
#define CLICK_IP6HELPERS_HH
#include <click/packet.hh>
#include <click/vector.hh>
CLICK_DECLS

namespace ip6 {
    // public functions
    bool has_fragmentation_extension_header(Packet *packet);
    uint8_t get_higher_layer_protocol(Packet *packet);
    
    // functions that should only be accessible from functions in the ip6 namespace
    uint8_t list_contains_value(Vector<uint8_t> list, uint8_t nxt);
};

CLICK_DECLS
#endif /* CLICK_IP6HELPERS_HH */
