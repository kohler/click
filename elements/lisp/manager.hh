/*
 * manager.{cc,hh} -- FMCC element deciding VM migrations (FMCCManager)
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

/** @file manager.hh
 * @brief FMCC element deciding VM migrations (FMCCManager). */

#ifndef CLICK_MANAGER_HH
#define CLICK_MANAGER_HH

#include <click/element.hh>
#include <click/ipaddress.hh>
#include <click/sync.hh>
#include <click/timer.hh>
#include <click/list.hh>
#include "lispcommon.hh"
#include "fmccbase.hh"
#include "migrator.hh"

CLICK_DECLS

/*
=c

FMCCManager(DC_SRC, DC_DST, VM, FMCCBASE, MIGRATOR, I<keywords> AUTO)

=s lisp

FMCC: decides VM migrations

=d

Input: copies of Map-Registers, stored in FMCCBASE.  With AUTO (default
true), the built-in rule migrates VM from DC_SRC to DC_DST through
MIGRATOR.  The C<migrate> handler orders a migration from a script or an
external controller.

=h registers, registers_valid, migrations read-only

Counters.

=h migrate write-only

C<VM ADDR FROM RLOC TO RLOC [MASK LEN]>: migrate a VM now.

=a FMCCDatabase, FMCCMigrator, LISPMigrationUpdater
*/
/** @brief FMCC: decides VM migrations (Click element FMCCManager).
 *
 * Arguments, ports and handlers: see the element documentation comment
 * above, also available as the FMCCManager(7click) man page. */
class FMCCManager : public Element {
  public:
    FMCCManager();
    ~FMCCManager();

    /// Click Element interface
    const char *class_name() const { return "FMCCManager"; }
    /// Click Element interface
    const char *port_count() const { return "1/0-1"; }

    /// Click Element interface
    int configure(Vector<String> &, ErrorHandler *);
    /// Click Element interface
    void add_handlers();
    /** @brief Store a Map-Register copy; with AUTO, decide and migrate. */
    void handle_register(Packet *p);
    /// Click Element interface
    void push(int port, Packet *p);
    /** @brief The built-in migration rule: the configured VM and data centers. */
    void decide(Decision *dec);
    /// Click Element interface
    static int write_handler(const String &, Element *, void *, ErrorHandler *);

    enum { EQL, MODIF, ERR, CRD };

  private:
    FMCCDatabase *_mbase;
    FMCCMigrator *_migrator;
    IPAddress _my_addr1;
    IPAddress _my_addr2;
    IPAddress _vm_addr;
    IPAddress _mask;
    EtherAddress _vm_mac;
    atomic_uint32_t _number_mig;
    atomic_uint32_t _rgstmsg_received;
    atomic_uint32_t _valid_rgstmsg;
    bool _auto; ///< AUTO: apply the built-in rule on registrations
};

CLICK_ENDDECLS
#endif
