/*
 * rqstsender1.{cc,hh} -- ITR element: map-cache lookups, Map-Request, Map-Reply
 * and Solicit-Map-Request handling (LISPMapRequester)
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

/** @file rqstsender1.hh
 * @brief ITR element: map-cache lookups, Map-Request, Map-Reply and Solicit-Map-Request handling (LISPMapRequester). */

#ifndef CLICK_RSQTSENDER1_HH
#define CLICK_RSQTSENDER1_HH
#include <click/element.hh>
#include <click/ipaddress.hh>
#include <click/sync.hh>
#include <click/timer.hh>
#include "itrcache.hh"
#include <clicknet/ip.h>

#include "lispcommon.hh"
CLICK_DECLS

/*
=c

LISPMapRequester(CACHE, PROTO, SRC, DST, I<keywords> MASK_LEN, AUTHORITATIVE, MAP_DATA, PITR, SMR_INVOKED, ECM, TTL, TOS, DSCP, DF)

=s lisp

ITR: map-cache lookups, Map-Request, Map-Reply and SMR handling

=d

Input 0: IP packets from the site.  A packet whose destination has a mapping
in CACHE (a LISPMapCache) leaves on output 0 with its destination annotation
set to the chosen RLOC (connect LISPEncap(..., DST_ANNO)).  Otherwise the
packet is queued and a Map-Request is sent on output 1 to DST (the
Map-Resolver).  Input 1: Map-Replies, checked (lengths, nonce, port) and
stored; queued packets are released.  Input 2: Solicit-Map-Requests; an SMR
from a locator of the cached mapping triggers a Map-Request with the
SMR-invoked bit, the old mapping stays in use meanwhile.  Output 2
(optional): packets to EIDs with a negative mapping, forwarded natively.

PROTO, SRC and DST describe the IP header of the packets it sends: PROTO is
accepted for compatibility and ignored (LISP is always UDP), SRC is the
source address, DST the destination or C<DST_ANNO>.  Keywords TOS, TTL,
DSCP, ECT, CE and DF are as for IPEncap.

MASK_LEN is the EID mask length in requests (default 24).  AUTHORITATIVE,
MAP_DATA, PITR and SMR_INVOKED set the A, M, p and s bits.  ECM (boolean)
sends Map-Requests inside an Encapsulated Control Message (RFC 9301).

=h stats read-only

Lookup hits and misses, requests sent, replies received and valid.

=h table, count, length read-only

The cache's mappings, queued packets and entries.

=h smrs, smrs_ignored, native_forwarded read-only

Counters.

=a LISPMapCache, LISPEncap, LISPMapReplier, LISPMapResolver
*/
/** @brief ITR: map-cache lookups, Map-Request, Map-Reply and SMR handling (Click element LISPMapRequester).
 *
 * Arguments, ports and handlers: see the element documentation comment
 * above, also available as the LISPMapRequester(7click) man page. */
class LISPMapRequester : public Element {
  public:
    LISPMapRequester();
    ~LISPMapRequester();

    /// Click Element interface
    const char *class_name() const { return "LISPMapRequester"; }
    /// out 2: natively forwarded (negative mapping)
    const char *port_count() const { return "3/0-3"; }
    const char *processing() const { return PUSH; } ///< Click Element interface

    /// Click Element interface
    int configure(Vector<String> &, ErrorHandler *);
    /// Click Element interface
    void add_handlers();
    /// Click Element interface
    int initialize(ErrorHandler *);
    /// Click Element interface
    void push(int port, Packet *p);
    /** @brief Data packet from the site: send it to its RLOC if mapped,
     * otherwise queue it and send a Map-Request. */
    void handle_data(Packet *p);

    /** @brief Map-Reply: validate, store the mapping, release queued packets. */
    void handle_response(Packet *p);
    /** @brief Validate a Map-Reply: lengths, nonce, ports, TTL.
     * @return the record that answers a pending request, or null */
    const Record *check_reply(Packet *p);

    /** @brief Build a Map-Request for @a dst_eid (inside an ECM if ECM is set).
     * @param dst_eid requested EID
     * @param src_eid Source-EID field (0.0.0.0 when unknown)
     * @param[out] udpsport,nonce1,nonce2 values a valid reply must match
     * @param smr_invoked set the s bit */
    Packet *make_request(IPAddress dst_eid, IPAddress src_eid,
	uint16_t *udpsport, uint32_t *nonce1, uint32_t *nonce2,
	bool smr_invoked = false);

    /** @brief SMR: refresh the mapping if the SMR comes from one of its locators. */
    void handle_smr(Packet *p);
    /// Click Element interface
    static String read_handler(Element *, void *);

  private:
    LISPIPHeader _hdr; ///< outer IP/UDP header of the packets sent

    LISPMapCache *_cache;

    uint16_t _eid_mask_len;
    bool _aut_bit_set, _is_pitr, _reply_rec_includ, _smr_rply;

    // statistics
    atomic_uint32_t _lookup_hit;
    atomic_uint32_t _lookup_miss;
    atomic_uint32_t _all_resp;
    atomic_uint32_t _valid_resp;
    atomic_uint32_t _rqst_sent;
    atomic_uint32_t _smr_count;
    atomic_uint32_t _smr_ignored;

    enum {
	NOT_EXIST,
	VALID_REPLY,
	NOT_VALID,
	VALID_REQUEST,
	CAN_REQUEST,
	WITHOUT_RESPONSE,
	NATIVE_FWD
    };
    atomic_uint32_t _native_fwd;
    enum {
	h_cache,
	h_stats,
	h_insert,
	h_clear,
	h_insert_rep,
	h_count,
	h_length
    };

    uint32_t _version;
    bool _ecm; ///< ECM: send Map-Requests inside an Encapsulated Control Message
};

CLICK_ENDDECLS
#endif
