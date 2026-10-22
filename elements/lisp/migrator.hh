/*
 * migrator.{cc,hh} -- FMCC element sending Map-Migrate orders (FMCCMigrator)
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

/** @file migrator.hh
 * @brief FMCC element sending Map-Migrate orders (FMCCMigrator). */

#ifndef CLICK_MIGRATOR_HH
#define CLICK_MIGRATOR_HH
#include <click/element.hh>
#include <click/glue.hh>
#include <click/atomic.hh>
#include <click/ipaddress.hh>
#include "lispcommon.hh"
#include <click/sync.hh>
#include <click/timer.hh>
#include <clicknet/ip.h>
#include <click/list.hh>
#include "fmccbase.hh"

CLICK_DECLS

/*
=c

FMCCMigrator(FMCCBASE, PROTO, SRC, DST, I<keywords> KEY)

=s lisp

FMCC: sends Map-Migrate orders

=d

Sends a Map-Migrate (RFC 8113 shared extension message, FMCC sub-type) on
the output to DST, the xTR of the data center the VM leaves, and records
the migration in FMCCBASE.  With KEY the message carries an
HMAC-SHA-256-128.  Input: Map-Notify messages (counted).

PROTO, SRC and DST describe the IP header of the packets it sends: PROTO is
accepted for compatibility and ignored (LISP is always UDP), SRC is the
source address, DST the destination or C<DST_ANNO>.  Keywords TOS, TTL,
DSCP, ECT, CE and DF are as for IPEncap.

=h migrations, notifies read-only

Counters.

=a FMCCManager, LISPMigrationUpdater, FMCCDatabase
*/
/** @brief FMCC: sends Map-Migrate orders (Click element FMCCMigrator).
 *
 * Arguments, ports and handlers: see the element documentation comment
 * above, also available as the FMCCMigrator(7click) man page. */
class FMCCMigrator : public Element {
  public:
    FMCCMigrator();
    ~FMCCMigrator();

    /// Click Element interface
    const char *class_name() const { return "FMCCMigrator"; }
    /// Click Element interface
    const char *port_count() const { return PORTS_1_1; }

    /// Click Element interface
    int configure(Vector<String> &, ErrorHandler *);
    /** @brief Send a Map-Migrate for a decision and record it in the FMCC database. */
    void send_migrate(Decision *dc);
    /// Click Element interface
    int initialize(ErrorHandler *errh);
    /** @brief Count a Map-Notify. */
    void handle_notify(Packet *p);
    /// Click Element interface
    void push(int port, Packet *p);
    /// Click Element interface
    void add_handlers();

  private:
    LISPIPHeader _hdr; ///< outer IP/UDP header of the packets sent
    FMCCDatabase *_mbase;
    LISPLock _lock;
    atomic_uint32_t _notif_received;
    atomic_uint32_t _migration_sent;
    String
	_key; ///< KEY: HMAC key shared with the xTRs (LISPMigrationUpdater); empty: unsigned
};

CLICK_ENDDECLS
#endif
