/*
 * ip6helpers{cc,hh} -- A file with IPv6 helper functions
 * Glenn Minne
 *
 * Copyright (c) 2000-2007 Mazu Networks, Inc.
 * Copyright (c) 2010 Meraki, Inc.
 * Copyright (c) 2004-2011 Regents of the University of California
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
#include <clicknet/tcp.h>
#include <clicknet/ip6.h>
#include "ip6helpers.hh"

CLICK_DECLS
namespace ip6 {

/** @brief Starts at the packet's network header pointer and searches for a fragmentation extension header.
  * @return true on found, false on not found
  */
bool has_fragmentation_extension_header(Packet *packet) {
    uint8_t nxt = ((click_ip6*) packet->network_header())->ip6_nxt;     // the next header number
    void *nxt_header = ((click_ip6*) packet->network_header() + 1);     // a pointer to the next header
    while (true) {
        if (nxt == 44) {
            return true;        // we have found a header
        } else if (nxt == 0) {
            nxt = ((click_ip6_hbh*) nxt_header)->ip6h_nxt;      
            nxt_header = (click_ip6_hbh*) nxt_header + ((click_ip6_hbh*) nxt_header)->ip6h_len + 1;            
        } else if (nxt == 43) {
            nxt = ((click_ip6_rthdr *) nxt_header)->ip6r_nxt;
            nxt_header = (click_ip6_rthdr *) nxt_header + ((click_ip6_rthdr *) nxt_header)->ip6r_len + 1;
        } else if (nxt == 60) {
            nxt = ((click_ip6_dest*) nxt_header)->ip6d_nxt;      
            nxt_header = (click_ip6_dest*) nxt_header + ((click_ip6_dest*) nxt_header)->ip6d_len + 1;
        } else {
            return false;
        }
    }
}

/** @brief Returns the higher layer protocol of this IPv6 packet.
  * In case extension headers are present, follow the extension
  * header chain to return the higher layer protocol
  *
  * Currently supports hop by hop, destination options, routing and
  * fragment header extension headers.
  */
uint8_t get_higher_layer_protocol(Packet *packet) {
    click_ip6 *network_header_of_this_packet = (click_ip6*) packet->network_header();
    void* header = network_header_of_this_packet;
    uint8_t nxt = network_header_of_this_packet->ip6_ctlun.ip6_un1.ip6_un1_nxt;
    
    if (nxt == 0) {      // An Hop By Hop extension MAY ONLY occur at the start
        click_ip6_hbh* hop_by_hop_header = (click_ip6_hbh*) header;
        hop_by_hop_header = hop_by_hop_header + hop_by_hop_header->ip6h_nxt + 1;
        header = (void*) hop_by_hop_header;     // Now we start at this header; which is the packet after the Hop By Hop Header
    }
    
    // If it is not an extension header give back the protocol immediatly
    if (!(nxt == 60    ||       // destination options header
          nxt == 43    ||       // routing header
          nxt == 44   ))    {   // fragment header
        return network_header_of_this_packet->ip6_ctlun.ip6_un1.ip6_un1_nxt;      // give it back immediatly
    }
        
    Vector<uint8_t> nxt_seen_list;
    while (nxt == 60 || nxt == 43 || nxt == 44) {
        if (nxt == 43) {
            click_ip6_rthdr *routing_header = (click_ip6_rthdr*) header;

            nxt = routing_header->ip6r_nxt;
            if (!list_contains_value(nxt_seen_list,nxt)) {
                nxt_seen_list.push_back(nxt);
            } else {
                throw String("An error: we have seen the routing header twice");
            }
            routing_header = routing_header + routing_header->ip6r_len + 1;
            header = (void*) routing_header;
        } else if (nxt == 44) {
            click_ip6_fragment *fragment_header = (click_ip6_fragment*) header;

            nxt = fragment_header->ip6_frag_nxt;
            if (!list_contains_value(nxt_seen_list,nxt)) {
                nxt_seen_list.push_back(nxt);
            } else {
                throw String("An error: we have seen the fragment header twice");
            }
            fragment_header++;
            header = (void*) fragment_header;
        } else if (nxt == 60) {
            click_ip6_dest *destination_header = (click_ip6_dest*) header;
            // TODO set the destination header pointer in the packet
            nxt = destination_header->ip6d_nxt;
            if (!list_contains_value(nxt_seen_list,nxt)) {
                nxt_seen_list.push_back(nxt);
            } else {
                throw String("An error: we have seen the routing header twice");
            }
            destination_header = destination_header + destination_header->ip6d_len + 1;
            header = (void*) destination_header;
        } else {
            if (nxt == 0) {
                throw String("We enountered a a Hop by Hop header but it was not the first header, this is not allowed");
            }
            click_chatter("we gaan %u returnen", nxt);
            return nxt; // nothing went wrong, send the thing back
        }
    }
}
    
// This function checks whether the next value list, contains the given next value
uint8_t list_contains_value(Vector<uint8_t> list, uint8_t nxt) {
    for(int i = 0; i < list.size(); i++) {
        if (list[i] == nxt) {
            return true;
        }
    }
    return false;
}    
    
};

CLICK_ENDDECLS
ELEMENT_PROVIDES(IP6Helpers)
