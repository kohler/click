/*
 * register.{cc,hh} -- ETR element sending Map-Register and checking Map-Notify
 * (LISPMapRegistrar)
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

/** @file register.hh
 * @brief ETR element sending Map-Register and checking Map-Notify (LISPMapRegistrar). */

#ifndef CLICK_REGISTER_HH
#define CLICK_REGISTER_HH
#include <click/element.hh>
#include <click/atomic.hh>
#include <click/sync.hh>
#include <click/timer.hh>
#include "lispcommon.hh"
#include "etrbase.hh"
CLICK_DECLS

/*
=c

LISPMapRegistrar(BASE, PROTO, SRC, DST, I<keywords> KEY, INTERVAL, RETRY)

=s lisp

ETR: periodic Map-Register, Map-Notify check

=d

Sends a Map-Register with the prefixes of BASE (a LISPEtrDatabase) to DST (the
Map-Server) at start-up, then every RETRY seconds (default 20) until a
valid Map-Notify arrives on the input, then every INTERVAL seconds
(default 60).  With KEY, messages are authenticated with HMAC-SHA-256-128
(Key ID 2) and a Map-Notify must carry a valid HMAC; in all cases it must
echo the nonce of the last Map-Register.

PROTO, SRC and DST describe the IP header of the packets it sends: PROTO is
accepted for compatibility and ignored (LISP is always UDP), SRC is the
source address, DST the destination or C<DST_ANNO>.  Keywords TOS, TTL,
DSCP, ECT, CE and DF are as for IPEncap.

=h registers, notifies, notifies_valid read-only

Counters.

=h register write-only

Send a Map-Register now.

=a LISPEtrDatabase, LISPMapServer
*/
/** @brief ETR: periodic Map-Register, Map-Notify check (Click element LISPMapRegistrar).
 *
 * Arguments, ports and handlers: see the element documentation comment
 * above, also available as the LISPMapRegistrar(7click) man page. */
class LISPMapRegistrar : public Element {
  public:
    LISPMapRegistrar();
    ~LISPMapRegistrar();

    /// Click Element interface
    const char *class_name() const { return "LISPMapRegistrar"; }
    /// Click Element interface
    const char *port_count() const { return PORTS_1_1; }

    /// Click Element interface
    int configure(Vector<String> &, ErrorHandler *);
    /// Click Element interface
    void run_timer(Timer *);
    /** @brief Send a Map-Register now with the ETR database's prefixes. */
    void send_register();
    /// Click Element interface
    int initialize(ErrorHandler *errh);
    /** @brief Check a Map-Notify (nonce, HMAC) and confirm the registration. */
    void handle_notify(Packet *p);
    /// Click Element interface
    void push(int port, Packet *p);

    /// Click Element interface
    void add_handlers();

  private:
    LISPIPHeader _hdr; ///< outer IP/UDP header of the packets sent
    LISPEtrDatabase *_base;

    atomic_uint32_t _notif_received;
    atomic_uint32_t _register_sent;
    String
	_key; ///< shared secret with the Map-Server (KEY); empty: no authentication
    uint32_t
	_last_nonce1; ///< nonce of the last Map-Register, echoed by Map-Notify
    uint32_t _last_nonce2;
    atomic_uint32_t _notif_valid;

    Timer _reg_timer;
    uint32_t
	_interval; ///< seconds between Map-Registers once confirmed (INTERVAL)
    uint32_t _retry; ///< seconds between Map-Registers until a Map-Notify (RETRY)
    bool _confirmed; ///< a valid Map-Notify answered the last Map-Register
    /// Click Element interface
    static int write_handler(const String &, Element *, void *, ErrorHandler *);
};

CLICK_ENDDECLS
#endif
