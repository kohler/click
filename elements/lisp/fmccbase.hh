/*
 * fmccbase.{cc,hh} -- FMCC database: where each VM is (FMCCDatabase)
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

/** @file fmccbase.hh
 * @brief FMCC database: where each VM is (FMCCDatabase). */

#ifndef CLICK_FMCCBASE_HH
#define CLICK_FMCCBASE_HH

#include <click/element.hh>
#include <click/sync.hh>
#include <click/timer.hh>
#include <click/list.hh>
#include <click/hashcontainer.hh>
#include <click/hashallocator.hh>
#include <click/vector.hh>
#include "lispcommon.hh"
CLICK_DECLS

/*
=c

FMCCDatabase(I<keywords> ENTRY_CAPACITY, INTERVAL)

=s lisp

FMCC database: where each VM is, and whether it migrated

=d

Filled by FMCCManager from Map-Registers, updated by FMCCMigrator.  Has no ports.

=h table read-only

The VMs: address, old and new data center, migrated flag.

=h entries, etrs, last_check read-only

Counters.

=a FMCCManager, FMCCMigrator
*/
/** @brief FMCC database: where each VM is, and whether it migrated (Click element FMCCDatabase).
 *
 * Arguments, ports and handlers: see the element documentation comment
 * above, also available as the FMCCDatabase(7click) man page. */
class FMCCDatabase : public Element {
  public:
    FMCCDatabase();
    ~FMCCDatabase();

    /// Click Element interface
    const char *class_name() const { return "FMCCDatabase"; }
    /// Click Element interface
    int configure(Vector<String> &, ErrorHandler *);

    /** @brief Record a Map-Register from an ETR.
     * @return EQL, MODIF or CRD (see LISPRegistrations::lookup) */
    int ms_lookup(IPAddress, uint16_t, int *);
    /** @brief Store the records of a Map-Register.
     * @param rec_count number of records announced by the message
     * @param rec first record
     * @param indice index of the registering ETR
     * @param[out] bol true if the VM's locator changed (it moved)
     * @param end end of the packet: records are not read past it */
    int ms_register(uint8_t, Record *, int, bool *, const uint8_t *end);
    /** @brief Mark a VM as migrated when its Map-Notify arrives (not called yet). */
    void verif_update(IPAddress, uint16_t);
    /** @brief Record a migration: the VM leaves a data center for another. */
    void update_with_mig(bool, IPAddress, IPAddress, IPAddress);

    /// Click Element interface
    int initialize(ErrorHandler *);

    /// Click Element interface
    static String read_handler(Element *, void *);
    /// Click Element interface
    void add_handlers();

    /************************************************************/
    /** @brief A VM: where it is and whether it migrated, keyed by its address. */

    struct MapEntry : public LISPMapping {
	// VM table: keyed by the VM's address (eid_prefix)
	IPAddress last_dc_addr;        ///< data center the VM left
	bool migrated;                 ///< a migration of this VM was ordered
	bool rloc_modified;            ///< the VM's locator changed (it moved)
	click_jiffies_t insert_time;   ///< time of the last migration
	click_jiffies_t send_time;     ///< not used
	click_jiffies_t regi_mig_time; ///< not used
	MapEntry *_hashnext;           ///< HashContainer chain link

	typedef IPAddress key_type; ///< HashContainer key type
	typedef IPAddress
	    key_const_reference; ///< HashContainer key reference type
	/** @brief HashContainer key of this entry. */
	key_const_reference hashkey() const { return eid_prefix; }

	/** @brief Entry for the VM with address @a key. */
	MapEntry(IPAddress key)
	  : last_dc_addr(IPAddress("0.0.0.0")),
	    migrated(false),
	    rloc_modified(false),
	    insert_time(0),
	    send_time(0),
	    regi_mig_time(0),
	    _hashnext(0)
	{
	    eid_prefix = key;
	}
	/** @brief Initialize from a registered record (VM address included). */
	void set_with(const Record *r, int indice)
	{
	    eid_prefix = r->eid_prefix;
	    LISPMapping::set_with(r, indice);
	}
	/** @brief Update from a new registration; sets rloc_modified if the VM moved. */
	void update_with(const Record *r, int indice)
	{
	    if (LISPMapping::update_with(r, indice))
		rloc_modified = true;
	}
    };

    /************************************************************/

    enum { EQL, MODIF, ERR, CRD };
    enum { h_base, h_cnt };

    /******************************************************************/

  private:
    LISPRegistrations _regs; ///< ETRs that registered

    typedef HashContainer<MapEntry> MBase;
    MBase _mbase;
    SizedHashAllocator<sizeof(MapEntry)> _alloc;

    LISPLock _lock;

    uint32_t _interval;
    atomic_uint32_t _entry_count;
    uint32_t _mbase_capacity;
    bool _success;
};

CLICK_ENDDECLS
#endif
