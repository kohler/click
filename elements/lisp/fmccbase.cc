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

#include <click/config.h>
#include <click/args.hh>
#include <click/glue.hh>
#include <click/error.hh>
#include "fmccbase.hh"
#include <click/bitvector.hh>
#include <click/straccum.hh>
#include <click/router.hh>

CLICK_DECLS

/*************************************************************************/

FMCCDatabase::FMCCDatabase()
{
    _entry_count = _mbase_capacity = 0;
    _interval = 120;
}

/*************************************************************************/

FMCCDatabase::~FMCCDatabase()
{
}

/*************************************************************************/

int
FMCCDatabase::configure(Vector<String> &conf, ErrorHandler *errh)
{
    if (Args(conf, this, errh)
	    .read("ENTRY_CAPACITY", _mbase_capacity)
	    .read("INTERVAL", _interval)
	    .complete()
	< 0)
	return -1;
    return 0;
}

/*************************************************************************/

int
FMCCDatabase::initialize(ErrorHandler *)
{
    return 0;
}

/*************************************************************************/

int
FMCCDatabase::ms_lookup(IPAddress etr_rloc, uint16_t map_vrs, int *indice)
{
    _lock.acquire_write();
    int r = _regs.lookup(etr_rloc, map_vrs, indice);
    _lock.release_write();
    return r;
}

/*************************************************************************/

int
FMCCDatabase::ms_register(
    uint8_t rec_count, Record *rec, int indice, bool *bol, const uint8_t *end)
{
    _lock.acquire_write();
    Record *_rec = rec;
    MBase::iterator it;
    for (int i = 1; i <= rec_count; i++) {
	// stop at the end of the packet (Record Count may lie)
	const uint8_t *r = reinterpret_cast<const uint8_t *>(_rec);
	if (r + 16 > end || r + lisp_record_size(_rec) > end)
	    break;
	it = _mbase.find(_rec->eid_prefix);
	if (!it) {
	    void *nw_entry = _alloc.allocate();
	    if (!nw_entry) {
		click_chatter(
		    "%p{element}: can't add entry (out of memory)", this);
		_lock.release_write();
		return 0;
	    }
	    MapEntry *nwe = new (nw_entry) MapEntry(_rec->eid_prefix);
	    nwe->set_with(_rec, indice);
	    _mbase.set(it, nwe);
	    ++_entry_count;
	    *bol = it->rloc_modified;
	} else {
	    it->update_with(_rec, indice); // the current record
	    *bol = it->rloc_modified;
	}
	_rec = reinterpret_cast<Record *>(reinterpret_cast<uint8_t *>(_rec)
	    + lisp_record_size(_rec)); // records have loc_count locators
    }
    _lock.release_write();
    return 1;
}

/*************************************************************************/

void
FMCCDatabase::update_with_mig(
    bool bol, IPAddress eid, IPAddress dc_src, IPAddress dc_des)
{
    _lock.acquire_write();
    MBase::iterator it = _mbase.find(eid);
    if (!it) {
	click_chatter("%p{element}: VM %s is not in the table", this,
	    eid.unparse().c_str());
    } else {
	it->last_dc_addr = dc_src;
	it->loc.locator = dc_des;
	it->migrated = bol;
	it->insert_time = click_jiffies();
	it->regi_mig_time = 0;
    }
    _lock.release_write();
    return;
}

/*************************************************************************/

/* Marks a VM as migrated when its Map-Notify arrives (not called yet). */

void
FMCCDatabase::verif_update(IPAddress eid, uint16_t map_vrs)
{
    // Map-Notify for a migrated VM: mark it migrated if the version matches.
    // (Authentication of the notify is done by HMAC, see LISPMapServer
    // and LISPMapRegistrar.)
    _lock.acquire_write();
    MBase::iterator it = _mbase.find(eid);
    _success = it;
    if (!it)
	click_chatter("%p{element}: this VM is not in the table", this);
    else if (map_vrs == it->version_num) {
	it->migrated = true;
	it->rloc_modified = false;
    } else
	click_chatter("%p{element}: Map-Notify version does not match", this);
    _lock.release_write();
}

/*************************************************************************/

String
FMCCDatabase::read_handler(Element *e, void *user_data)
{
    FMCCDatabase *mbse = (FMCCDatabase *)e;
    StringAccum sa;
    int i = 1;
    switch (reinterpret_cast<uintptr_t>(user_data)) {
    case h_base: {
	MBase::iterator end_it = mbse->_mbase.end();
	for (MBase::iterator it = mbse->_mbase.begin(); it != end_it; ++it) {
	    sa << '\n'
	       << i << " . " << "VM:" << it->eid_prefix
	       << ",  OLD_Destination :  " << it->last_dc_addr
	       << ",  Mask :  " << it->mask_len
	       << ",  Version:  " << it->version_num
	       << "  TTL:   " << it->rec_ttl
	       << ",  Authorisation bit :  " << it->aut_bit
	       << ", NEW_Destination : " << it->loc.locator
	       << ", Migrated : " << it->migrated << ",  size  "
	       << mbse->_mbase.size() << '\n'
	       << '\n';
	    i++;
	}
	break;
    }
    }
    return sa.take_string();
}

/*************************************************************************/

void
FMCCDatabase::add_handlers()
{
    add_read_handler("fmcc base", read_handler, h_base);
    add_read_handler("table", read_handler, h_base); // Click-style name
    add_data_handlers("entry count", Handler::OP_READ, &_entry_count);
    add_data_handlers(
	"entries", Handler::OP_READ, &_entry_count); // Click-style name
    add_data_handlers("ETR registred ", Handler::OP_READ, &_regs._etr_count);
    add_data_handlers(
	"etrs", Handler::OP_READ, &_regs._etr_count); // Click-style name
    add_data_handlers("Comparaison ", Handler::OP_READ, &_success);
    add_data_handlers(
	"last_check", Handler::OP_READ, &_success); // Click-style name
}

/*************************************************************************/

CLICK_ENDDECLS
EXPORT_ELEMENT(FMCCDatabase)
ELEMENT_MT_SAFE(FMCCDatabase)
