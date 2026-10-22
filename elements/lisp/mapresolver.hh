/*
 * mapresolver.{cc,hh} -- Map-Resolver element: forwards Map-Requests, sends
 * Negative Map-Replies (LISPMapResolver)
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

/** @file mapresolver.hh
 * @brief Map-Resolver element: forwards Map-Requests, sends Negative Map-Replies (LISPMapResolver). */

#ifndef CLICK_MAPRESOLVER_HH
#define CLICK_MAPRESOLVER_HH

#include <click/element.hh>
#include <click/atomic.hh>
#include <clicknet/ip.h>
#include "mapbase.hh"
#include "lispcommon.hh"

CLICK_DECLS

/*
=c

LISPMapResolver(MBASE, PROTO, SRC, DST, I<keywords>)

=s lisp

Map-Resolver: forwards Map-Requests to the ETR, or sends a Negative Map-Reply

=d

Input: Map-Requests, plain or inside an Encapsulated Control Message.  If
MBASE (a LISPMappingDB) has a mapping that covers the EID, the request is sent on
the output to the ETR's RLOC (the ETR answers the ITR directly).
Otherwise a Negative Map-Reply (no locator, action Natively-Forward, TTL 15
minutes) is sent to the ITR.

PROTO, SRC and DST describe the IP header of the packets it sends: PROTO is
accepted for compatibility and ignored (LISP is always UDP), SRC is the
source address, DST the destination or C<DST_ANNO>.  Keywords TOS, TTL,
DSCP, ECT, CE and DF are as for IPEncap.

=h requests, found, forwarded, negative, ecm read-only

Counters.

=a LISPMappingDB, LISPMapServer, LISPMapRequester
*/
/** @brief Map-Resolver: forwards Map-Requests to the ETR, or sends a Negative Map-Reply (Click element LISPMapResolver).
 *
 * Arguments, ports and handlers: see the element documentation comment
 * above, also available as the LISPMapResolver(7click) man page. */
class LISPMapResolver : public Element {
  public:
    LISPMapResolver();
    ~LISPMapResolver();

    /// Click Element interface
    const char *class_name() const { return "LISPMapResolver"; }
    /// Click Element interface
    const char *port_count() const { return PORTS_1_1; }

    /// Click Element interface
    int configure(Vector<String> &, ErrorHandler *);
    /// Click Element interface
    void push(int port, Packet *p);
    /** @brief Negative Map-Reply for a request whose EID has no mapping. */
    Packet *negative_reply(const click_ip *iph, const click_udp *udph,
	const click_lisp_map_request *mrh);
    /// Click Element interface
    int initialize(ErrorHandler *);
    /// Click Element interface
    void add_handlers();

  private:
    LISPIPHeader _hdr; ///< outer IP/UDP header of the packets sent
    LISPMappingDB *_mbase;
    atomic_uint32_t _rqst_received;
    atomic_uint32_t _response_found;
    atomic_uint32_t _rply_sent;
    atomic_uint32_t _negative_sent;
    atomic_uint32_t _ecm_received;

    IPAddress _ip;
    uint32_t _proto;
    IPAddress _itr_src, _itr_dst;
};

CLICK_ENDDECLS
#endif
