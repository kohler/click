/*
 * lispencap.{cc,hh} -- element encapsulates IP packets in LISP/UDP/IP (RFC 9300)
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

/** @file lispencap.hh
 * @brief Element encapsulates IP packets in LISP/UDP/IP (RFC 9300). */

#ifndef CLICK_LISPENCAP_HH
#define CLICK_LISPENCAP_HH

#include <click/element.hh>
#include <click/atomic.hh>
#include <click/glue.hh>
#include <clicknet/ip.h>

#include "lispcommon.hh"
CLICK_DECLS

/*
=c

LISPEncap(PROTO, SRC, DST, I<keywords> NONCE, ECHO_NONCE, LSB, IID, UDP_CHECKSUM, TTL, TOS, DSCP, DF)

=s lisp

encapsulates IP packets in LISP/UDP/IP (RFC 9300)

=d

Prepends an outer IPv4 header, a UDP header (destination port 4341, source
port derived from the inner flow) and the 8-byte LISP data header.  With
DST_ANNO, the outer destination is the packet's destination IP annotation,
which LISPMapRequester sets to the RLOC.

PROTO, SRC and DST describe the IP header of the packets it sends: PROTO is
accepted for compatibility and ignored (LISP is always UDP), SRC is the
source address, DST the destination or C<DST_ANNO>.  Keywords TOS, TTL,
DSCP, ECT, CE and DF are as for IPEncap.

=over 8

=item NONCE

Boolean.  Set the N bit and a random 24-bit nonce.  Default true.  (Old
name: UseNce.)  When false, the V bit is set with map-versions 0.

=item ECHO_NONCE

Boolean.  Set the E bit (echo-nonce request).  Default false.  (SNRqst.)

=item LSB

Boolean.  Set the L bit; locator 0 is reported up.  Default false.  (useLSB.)

=item IID

Integer, 24 bits.  Set the I bit with this instance ID.  (useIID true sets
the I bit with instance ID 0.)

=item UDP_CHECKSUM

Boolean.  Compute the UDP checksum; otherwise it is 0, which RFC 9300 allows
for IPv4.  Default false.  (SetUDPSum.)

=back

=h count read-only

Packets encapsulated.

=a LISPDecap, LISPMapRequester, IPEncap
*/
/** @brief Encapsulates IP packets in LISP/UDP/IP (RFC 9300) (Click element LISPEncap).
 *
 * Arguments, ports and handlers: see the element documentation comment
 * above, also available as the LISPEncap(7click) man page. */
class LISPEncap : public Element {
  public:
    LISPEncap();
    ~LISPEncap();
    /// Click Element interface
    const char *class_name() const { return "LISPEncap"; }
    /// Click Element interface
    const char *port_count() const { return PORTS_1_1; }
    /// Click Element interface
    void push(int, Packet *);
    /// Click Element interface
    int configure(Vector<String> &, ErrorHandler *);
    /** @brief Encapsulate one packet and push it on output 0. */
    void encapsulation(Packet *);
    /// Click Element interface
    int initialize(ErrorHandler *);
    /// Click Element interface
    void add_handlers();

  private:
    LISPIPHeader _hdr; ///< outer IP/UDP header of the packets sent
    bool _use_nonce;
    bool _solicit_rqst;
    bool _loc_sb;
    bool _use_ins_id;
    uint32_t _ins_id; ///< instance ID (24 bits) when _use_ins_id
    bool _set_udp_sum;
    atomic_uint32_t _packet_count;
};

CLICK_ENDDECLS

#endif
