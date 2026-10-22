/*
 * rqstresolver.{cc,hh} -- ETR element answering Map-Requests (LISPMapReplier)
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

/** @file rqstresolver.hh
 * @brief ETR element answering Map-Requests (LISPMapReplier). */

#ifndef CLICK_RSQTRESOLVER_HH
#define CLICK_RSQTRESOLVER_HH

#include <click/element.hh>
#include <click/atomic.hh>
#include <click/ipaddress.hh>
#include <click/sync.hh>
#include <clicknet/ip.h>
#include "etrbase.hh"

#include "lispcommon.hh"
CLICK_DECLS

/*
=c

LISPMapReplier(BASE, PROTO, SRC, DST, I<keywords>)

=s lisp

ETR: answers Map-Requests with Map-Replies

=d

Input: Map-Requests.  If BASE (a LISPEtrDatabase) has a prefix that covers the
requested EID (longest-prefix match), a Map-Reply is sent on the output to
the request's ITR-RLOC and source port.  The ITR is remembered for SMRs.

PROTO, SRC and DST describe the IP header of the packets it sends: PROTO is
accepted for compatibility and ignored (LISP is always UDP), SRC is the
source address, DST the destination or C<DST_ANNO>.  Keywords TOS, TTL,
DSCP, ECT, CE and DF are as for IPEncap.

=h requests, replies read-only

Counters.

=a LISPEtrDatabase, LISPMapRequester
*/
/** @brief ETR: answers Map-Requests with Map-Replies (Click element LISPMapReplier).
 *
 * Arguments, ports and handlers: see the element documentation comment
 * above, also available as the LISPMapReplier(7click) man page. */
class LISPMapReplier : public Element {
  public:
    LISPMapReplier();
    ~LISPMapReplier();

    /// Click Element interface
    const char *class_name() const { return "LISPMapReplier"; }
    /// Click Element interface
    const char *port_count() const { return PORTS_1_1; }

    /// Click Element interface
    int configure(Vector<String> &, ErrorHandler *);
    /// Click Element interface
    void push(int, Packet *);
    /// Click Element interface
    int initialize(ErrorHandler *);
    /** @brief Answer a Map-Request with a Map-Reply if a local prefix covers the EID. */
    void handle_request(Packet *);
    /// Click Element interface
    void add_handlers();

  private:
    LISPIPHeader _hdr; ///< outer IP/UDP header of the packets sent
    LISPEtrDatabase *_base;

    //Statistics
    atomic_uint32_t _rqst_received;
    atomic_uint32_t _rply_sent;
};

CLICK_ENDDECLS
#endif
