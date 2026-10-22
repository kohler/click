/*
 * updater.{cc,hh} -- xTR element applying FMCC Map-Migrate orders
 * (LISPMigrationUpdater)
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

/** @file updater.hh
 * @brief XTR element applying FMCC Map-Migrate orders (LISPMigrationUpdater). */

#ifndef CLICK_UPDATER_HH
#define CLICK_UPDATER_HH
#include <click/element.hh>
#include <click/atomic.hh>
#include <click/ipaddress.hh>
#include "lispcommon.hh"
#include <click/timer.hh>
#include "register.hh"
#include "etrbase.hh"
#include "transmitter.hh"
CLICK_DECLS

/*
=c

LISPMigrationUpdater(RLOC, BASE, TRANSMITTER, REGISTER, I<keywords> KEY)

=s lisp

xTR: applies FMCC Map-Migrate orders

=d

Input: Map-Migrate messages.  If the new data center is this one (RLOC),
the VM's EID is added to BASE (a LISPEtrDatabase) and TRANSMITTER;
otherwise it is removed and an SMR is sent on the output to every ITR
that asked for it.
REGISTER then sends a Map-Register.  With KEY, only Map-Migrate messages
with a valid HMAC-SHA-256-128 are obeyed.

=h vms_in, vms_out, smrs, auth_failures, last_dc read-only

Counters and the last destination data center.

=a LISPEtrDatabase, LISPMapRegistrar, LISPVMCounter, FMCCMigrator
*/
/** @brief XTR: applies FMCC Map-Migrate orders (Click element LISPMigrationUpdater).
 *
 * Arguments, ports and handlers: see the element documentation comment
 * above, also available as the LISPMigrationUpdater(7click) man page. */
class LISPMigrationUpdater : public Element {
  public:
    LISPMigrationUpdater();
    ~LISPMigrationUpdater();

    /// Click Element interface
    const char *class_name() const { return "LISPMigrationUpdater"; }
    /// Click Element interface
    const char *port_count() const { return PORTS_1_1; }

    /// Click Element interface
    int configure(Vector<String> &, ErrorHandler *);
    /// Click Element interface
    void add_handlers();
    /// Click Element interface
    void push(int port, Packet *p);

    /** @brief Build an SMR for an EID, addressed to an ITR. */
    Packet *make_smr(IPAddress itr_addr, uint16_t mask, IPAddress eid);
    /** @brief Send an SMR for an EID to each ITR. */
    void send_smrs(uint16_t mask, IPAddress eid, const Vector<IPAddress> &itrs);

  private:
    LISPIPHeader _hdr; ///< outer IP/UDP header of the packets sent

    LISPEtrDatabase *_base;
    LISPMapRegistrar *_register;
    LISPVMCounter *_transmit;
    IPAddress _my_addr, _ip;
    uint32_t _ttl;
    atomic_uint32_t _mv_in;
    atomic_uint32_t _mv_out;
    atomic_uint32_t _smr_sent;
    atomic_uint32_t _auth_failures;
    String _key; ///< KEY: HMAC key shared with the FMCC FMCCMigrator
};

CLICK_ENDDECLS
#endif
