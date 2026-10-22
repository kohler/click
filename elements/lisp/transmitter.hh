/*
 * transmitter.{cc,hh} -- element counting decapsulated packets per VM (LISPVMCounter)
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

/** @file transmitter.hh
 * @brief Element counting decapsulated packets per VM (LISPVMCounter). */

#ifndef TRANSMITTER_HH
#define TRANSMITTER_HH

#include <click/element.hh>
#include <click/atomic.hh>
#include <click/sync.hh>
#include <click/etheraddress.hh>
#include <click/vector.hh>
#include "lispcommon.hh"

CLICK_DECLS

/*
=c

LISPVMCounter([VM1, VM2, VM3, VM4], I<keywords> VM, USE_MAC)

=s lisp

counts decapsulated packets per VM (stands in for the hypervisor)

=d

Counts the packets for each VM address (or MAC address with USE_MAC).
With output 0 connected, packets are passed on after counting; otherwise
they are dropped.  LISPMigrationUpdater adds and removes VMs when they migrate.

=h table read-only

Packets per VM.

=h received, counted read-only

Counters.

=a LISPMigrationUpdater, LISPDecap
*/
/** @brief Counts decapsulated packets per VM (stands in for the hypervisor) (Click element LISPVMCounter).
 *
 * Arguments, ports and handlers: see the element documentation comment
 * above, also available as the LISPVMCounter(7click) man page. */
class LISPVMCounter : public Element {
  public:
    LISPVMCounter();
    ~LISPVMCounter();

    /// Click Element interface
    const char *class_name() const { return "LISPVMCounter"; }
    // Stands in for the hypervisor: counts decapsulated packets per VM.
    // With output 0 connected, packets are passed on after counting
    // (for example to a ToDevice towards the VMs); otherwise they are dropped.
    /// Click Element interface
    const char *port_count() const { return "1/0-1"; }
    /// Click Element interface
    int configure(Vector<String> &, ErrorHandler *);
    /// Click Element interface
    int initialize(ErrorHandler *errh);
    /** @brief Index of the VM with this address, or -1. */
    int iplookup(IPAddress ip);
    /** @brief Index of the VM with this MAC address, or -1. */
    int ethlookup(EtherAddress eth);

    /** @brief Add a VM. */
    void insert(const EtherAddress &mac, IPAddress ip);
    /** @brief Remove a VM. */
    void remove_vm(IPAddress vm_adr);
    /** @brief Count a packet for its VM. */
    void packet_handle(Packet *p);
    /// Click Element interface
    void push(int port, Packet *p);

    /// Click Element interface
    static String read_handler(Element *e, void *user_data);
    /// Click Element interface
    void add_handlers();
    enum { h_table, h_count };

  private:
    LISPLock _lock;
    struct TransEntry {
	IPAddress eid;
	EtherAddress eth;
	uint32_t packet_count;

	TransEntry() { packet_count = 0; }
    };

    typedef Vector<TransEntry> TransTable;
    TransTable _transtable;

    IPAddress _adr1, _adr2, _adr3, _adr4; ///< historical positional form
    Vector<IPAddress> _vms;               ///< VM keyword addresses
    atomic_uint32_t _received_packet;
    atomic_uint32_t _transmit_packet;
    bool _use_mac;
};

inline int
LISPVMCounter::iplookup(IPAddress ipadr)
{
    for (int i = 0; i < _transtable.size(); i++)
	if (_transtable[i].eid == ipadr)
	    return i;

    return -1;
};

inline int
LISPVMCounter::ethlookup(EtherAddress ethadr)

{
    for (int i = 0; i < _transtable.size(); i++)
	if (_transtable[i].eth == ethadr)
	    return i;

    return -1;
};

CLICK_ENDDECLS
#endif
