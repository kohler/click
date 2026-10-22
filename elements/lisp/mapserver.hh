/*
 * mapserver.{cc,hh} -- Map-Server element: Map-Register, Map-Notify (LISPMapServer)
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

/** @file mapserver.hh
 * @brief Map-Server element: Map-Register, Map-Notify (LISPMapServer). */

#ifndef CLICK_MAPSERVER_HH
#define CLICK_MAPSERVER_HH

#include <click/element.hh>
#include <click/atomic.hh>
#include <click/ipaddress.hh>
#include <clicknet/ip.h>
#include "mapbase.hh"

#include "lispcommon.hh"
CLICK_DECLS

/*
=c

LISPMapServer(MBASE, PROTO, SRC, DST, I<keywords> SITE)

=s lisp

Map-Server: accepts Map-Registers, sends Map-Notify

=d

Input: Map-Registers.  Valid registrations are stored in MBASE (a LISPMappingDB)
and, when the M bit is set, a Map-Notify (same nonce and records, signed
with the site key) is sent on the output to the ETR.  Each C<SITE "PREFIX
KEY"> keyword declares a site; when sites are declared, a Map-Register must
carry a valid HMAC-SHA-256-128 with the site key and register only prefixes
inside sites with that key.  Without SITE, every registration is accepted.

PROTO, SRC and DST describe the IP header of the packets it sends: PROTO is
accepted for compatibility and ignored (LISP is always UDP), SRC is the
source address, DST the destination or C<DST_ANNO>.  Keywords TOS, TTL,
DSCP, ECT, CE and DF are as for IPEncap.

=h registers, notifies, auth_failures read-only

Counters.

=h last_src, last_dst, last_sport, last_dport, last_type, last_ttl read-only

Fields of the last Map-Register.

=a LISPMappingDB, LISPMapRegistrar, LISPMapResolver
*/
/** @brief Map-Server: accepts Map-Registers, sends Map-Notify (Click element LISPMapServer).
 *
 * Arguments, ports and handlers: see the element documentation comment
 * above, also available as the LISPMapServer(7click) man page. */
class LISPMapServer : public Element {
  public:
    LISPMapServer();
    ~LISPMapServer();

    /// Click Element interface
    const char *class_name() const { return "LISPMapServer"; }
    /// Click Element interface
    const char *port_count() const { return PORTS_1_1; }

    /// Click Element interface
    int configure(Vector<String> &, ErrorHandler *);
    /// Click Element interface
    void push(int port, Packet *p);
    /** @brief Authenticate and store a Map-Register; answer with a Map-Notify
       * when the M bit is set. */
    void handle_register(Packet *p);
    /** @brief Map-Notify answering a Map-Register, signed with @a key if not empty. */
    Packet *make_notify(Packet *rgst_p, const String &key);
    /// Click Element interface
    void add_handlers();
    /// Click Element interface
    int initialize(ErrorHandler *);

    enum { EQL, MODIF, ERR, CRD };

  private:
    LISPIPHeader _hdr; ///< outer IP/UDP header of the packets sent
    // Sites allowed to register (SITE "PREFIX KEY").  If none is
    // configured the Map-Server accepts every registration (open mode).
    struct Site {
	LISPPrefix prefix;
	String key;
    };
    Vector<Site> _sites;
    /** @brief Most specific site that contains the prefix eid/len, or null. */
    const Site *find_site(IPAddress eid, int len) const;
    atomic_uint32_t _auth_failures;
    LISPMappingDB *_mbase;
    atomic_uint32_t _rgstmsg_received;
    atomic_uint32_t _valid_rgstmsg;
    atomic_uint32_t _notifmsg_sent;
    //stat
    IPAddress _dest, _src;
    uint32_t _dp, _sp, _type, _ttl;
};

CLICK_ENDDECLS
#endif
