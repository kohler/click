/*
 * mapbase.{cc,hh} -- Map-Server mapping database (LISPMappingDB)
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
#include "mapbase.hh"
#include <click/bitvector.hh>
#include <click/straccum.hh>
#include <click/router.hh>

CLICK_DECLS

/******************************************************************************************************************/

LISPMappingDB::LISPMappingDB()
  : _timer(this)
{
    _reg_timeout = 180;
    _expired_count = 0;
    _entry_count = _mbase_capacity = 0;
    _interval = 120;
}

/******************************************************************************************************************/
LISPMappingDB::~LISPMappingDB()
{
}

/******************************************************************************************************************/

int
LISPMappingDB::configure(Vector<String> &conf, ErrorHandler *errh)
{
    if (Args(conf, this, errh)
	    .read("ENTRY_CAPACITY", _mbase_capacity)
	    .read("INTERVAL", _interval)
	    .read("REG_TIMEOUT",
		_reg_timeout) // seconds without Map-Register before a mapping is removed
	    .complete()
	< 0)
	return -1;

    return 0;
}

/******************************************************************************************************************/

int
LISPMappingDB::initialize(ErrorHandler *)
{
    _timer.initialize(this);
    _timer.schedule_after_sec(1);
    return 0;
}

// Every second: remove the mappings of ETRs that stopped registering
// (RFC 9301: a Map-Server times registrations out, about 3 missed
// Map-Registers).
void
LISPMappingDB::run_timer(Timer *)
{
    click_jiffies_t now = click_jiffies();
    click_jiffies_t timeout = (click_jiffies_t)_reg_timeout * CLICK_HZ;
    _lock.acquire_write();
    for (MBase::iterator it = _mbase.begin(); it;)
	if (it->reg_idce >= 0 && it->reg_idce < _regs._etrs.size()
	    && _regs._etrs[it->reg_idce].reg_expired(now, timeout)) {
	    MapEntry *e = _mbase.erase(it);
	    _lens.remove(e->prefix.len);
	    _alloc.deallocate(e);
	    --_entry_count;
	    ++_expired_count;
	} else
	    ++it;
    // the next Map-Register of an expired ETR must be processed again
    for (int i = 0; i < _regs._etrs.size(); ++i)
	if (_regs._etrs[i].reg_expired(now, timeout))
	    _regs._etrs[i].map_version
		= 0xFFFF; // never a valid 12-bit map-version
    _lock.release_write();
    _timer.reschedule_after_sec(1);
}

/******************************************************************************************************************/

/******************************************************************************************************************/

int
LISPMappingDB::mr_lookup(IPAddress eid,
    uint16_t /*mask_len: LPM uses the address*/, bool *aut_bit, IPAddress *rloc)
{
    _lock.acquire_read();
    MapEntry *it = lisp_lpm(_mbase, _lens, eid); // longest-prefix match
    int li = it ? lisp_choose_locator(it->locs, it->nlocs, eid.addr()) : -1;
    if (li < 0) {
	_lock.release_read();
	return 0;
    }

    *rloc = it->locs[li].locator;
    *aut_bit = false;

    _lock.release_read();
    return 1;
}

/******************************************************************************************************************/

int
LISPMappingDB::ms_lookup(IPAddress etr_rloc, uint16_t map_vrs, int *indice)
{
    // Registrations are authenticated by the Map-Server (HMAC, see
    // LISPMapServer SITE); here we only track each ETR's registrations.
    _lock.acquire_write();
    int r = _regs.lookup(etr_rloc, map_vrs, indice);
    _lock.release_write();
    return r;
}

/******************************************************************************************************************/

int
LISPMappingDB::ms_register(
    uint8_t rec_count, Record *rec, int indice, const uint8_t *end)
{
    _lock.acquire_write();
    Record *_rec = rec;
    MBase::iterator it;
    for (int i = 1; i <= rec_count; i++) {
	// stop at the end of the packet (Record Count may lie)
	const uint8_t *r = reinterpret_cast<const uint8_t *>(_rec);
	if (r + 16 > end || r + lisp_record_size(_rec) > end)
	    break;
	LISPPrefix key(_rec->eid_prefix, _rec->mask_len);
	it = _mbase.find(key);
	if (!it) {
	    void *nw_entry = _alloc.allocate();
	    if (!nw_entry) {
		click_chatter(
		    "%p{element}: can't add entry (out of memory)", this);
		_lock.release_write();
		return 0;
	    }
	    MapEntry *nwe = new (nw_entry) MapEntry(key);
	    _lens.add(key.len);
	    nwe->set_with(_rec, indice);
	    _mbase.set(it, nwe);
	    ++_entry_count;
	} else
	    it->update_with(_rec, indice); // the current record
	_rec = reinterpret_cast<Record *>(reinterpret_cast<uint8_t *>(_rec)
	    + lisp_record_size(_rec)); // records have loc_count locators
    }
    _lock.release_write();
    return 1;
}

/******************************************************************************************************************/

String
LISPMappingDB::read_handler(Element *e, void *user_data)
{
    LISPMappingDB *mbse = (LISPMappingDB *)e;
    StringAccum sa;

    switch (reinterpret_cast<uintptr_t>(user_data)) {
    case h_base: {
	MBase::iterator end_it = mbse->_mbase.end();
	for (MBase::iterator it = mbse->_mbase.begin(); it != end_it; ++it) {
	    sa << it->eid_prefix << " mask " << it->mask_len << " version  "
	       << it->version_num << "  ttl  " << it->rec_ttl << "  auth bit "
	       << it->aut_bit << "RLOC" << it->loc.locator << '\n';
	}
	break;
    }
    }
    return sa.take_string();
}

/******************************************************************************************************************/

void
LISPMappingDB::add_handlers()
{
    add_read_handler("map base", read_handler, h_base);
    add_read_handler("table", read_handler, h_base); // Click-style name
    add_data_handlers("entry count", Handler::OP_READ, &_entry_count);
    add_data_handlers(
	"entries", Handler::OP_READ, &_entry_count); // Click-style name
    add_data_handlers("ETR registred ", Handler::OP_READ, &_regs._etr_count);
    add_data_handlers(
	"etrs", Handler::OP_READ, &_regs._etr_count); // Click-style name
    add_data_handlers("expired", Handler::OP_READ, &_expired_count);
}

/******************************************************************************************************************/

CLICK_ENDDECLS
EXPORT_ELEMENT(LISPMappingDB)
ELEMENT_MT_SAFE(LISPMappingDB)
