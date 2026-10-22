/*
 * etrbase.{cc,hh} -- ETR database of the site's EID prefixes (LISPEtrDatabase)
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
#include "etrbase.hh"
#include <click/args.hh>
#include <click/bitvector.hh>
#include <click/straccum.hh>
#include <click/router.hh>
#include <click/error.hh>
#include <click/glue.hh>
#include <clicknet/ip.h>
#include <clicknet/udp.h>
#include "lispcommon.hh"
CLICK_DECLS

/***********************************************************************************/

LISPEtrDatabase::LISPEtrDatabase()
{
    _entry_count = _response = 0;
    _ttl = 600;
    _map_version = 1;
}

/***********************************************************************************/

LISPEtrDatabase::~LISPEtrDatabase()
{
}

/***********************************************************************************/

int
LISPEtrDatabase::configure(Vector<String> &conf, ErrorHandler *errh)
{
    // LISPEtrDatabase(RLOC [, EID1, EID2, EID3, EID4] [, EID prefix ...] [, TTL minutes])
    // The four positional EIDs (historical form) are host EIDs (/32).
    Vector<String> eid_strs;
    if (Args(conf, this, errh)
	    .read_mp("src_adr", _my_addr)
	    .read_p("adr1", _adr1)
	    .read_p("adr2", _adr2)
	    .read_p("adr3", _adr3)
	    .read_p("adr4", _adr4)
	    .read_all("EID", AnyArg(), eid_strs)
	    .read("TTL", _ttl)
	    .complete()
	< 0)
	return -1;
    for (int i = 0; i < eid_strs.size(); ++i) {
	IPAddress a, m;
	if (!IPPrefixArg(true).parse(eid_strs[i], a, m, this))
	    return errh->error(
		"EID %<%s%> is not an IPv4 prefix", eid_strs[i].c_str());
	int len = m.mask_to_prefix_len();
	if (len < 0)
	    return errh->error(
		"EID %<%s%> has a non-contiguous mask", eid_strs[i].c_str());
	_eids.push_back(LISPPrefix(a, len));
    }

    return 0;
}

/***********************************************************************************/

int
LISPEtrDatabase::initialize(ErrorHandler *)
{
    IPAddress hosts[4] = {_adr1, _adr2, _adr3, _adr4};
    for (int i = 0; i < 4; ++i)
	if (hosts[i])
	    insert(hosts[i], 32, _ttl);
    for (int i = 0; i < _eids.size(); ++i)
	insert(_eids[i].addr, _eids[i].len, _ttl);
    return 0;
}

/***********************************************************************************/

int
LISPEtrDatabase::size_table()
{
    return _base.size();
}

/***********************************************************************************/

bool
LISPEtrDatabase::find_ip(IPAddress ip)
{
    return lisp_lpm(_base, _lens, ip) != 0;
}

/***********************************************************************************/

Vector<IPAddress>
LISPEtrDatabase::remove_host(IPAddress eid)
{
    Vector<IPAddress> itrs;
    _lock.acquire_write();
    Base::iterator it = _base.find(LISPPrefix(eid, 32)); // host entry
    if (it) {
	for (ItrRloc *r = it->list.front(); r; r = r->link.next())
	    itrs.push_back(r->itr_addr); // copy before freeing
	_lens.remove(32);
	BaseEntry *entry = _base.erase(it);
	entry->free_itrs();
	_alloc.deallocate(entry);
	--_entry_count;
	++_map_version;
    }
    _base.balance();
    _lock.release_write();

    return itrs;
}

/***********************************************************************************/

int
LISPEtrDatabase::insert(IPAddress eid, int mask_len, uint32_t ttl)
{
    _lock.acquire_write();
    LISPPrefix key(eid, mask_len);
    Base::iterator it = _base.find(key);
    if (!it) {
	void *new_entry = _alloc.allocate();
	if (!new_entry) {
	    _lock.release_write();
	    return 0;
	}
	++_entry_count;
	BaseEntry *be = new (new_entry) BaseEntry(key);
	be->ttl = ttl;
	_lens.add(mask_len);

	_base.set(it, be);
	++_map_version;
    }
    _lock.release_write();
    return 1;
}

/***********************************************************************************/

Packet *
LISPEtrDatabase::register_records(uint8_t *rec_count)
{
    _lock.acquire_read();
    if (!_entry_count) {
	_lock.release_read();
	return 0;
    }
    uint32_t length = _entry_count * sizeof(Record);
    WritablePacket *q = Packet::make(length);
    if (!q) {
	click_chatter("%p{element}: can't make Map-Register packet", this);
	_lock.release_read();
	return 0;
    }
    Record *msg = (Record *)(q->data());
    *rec_count = 0;
    Base::iterator end_it = _base.end();
    for (Base::iterator it = _base.begin(); it != end_it; ++it) {
	lisp_record_fill(msg, it->ttl, it->mask_len, _map_version, false,
	    it->eid_prefix, 0, 100, _my_addr,
	    LISP_LOC_F_LOCAL | LISP_LOC_F_REACH); // our own locator, up
	++*rec_count;
	++msg;
    }
    _lock.release_read();
    return q;
}

/***********************************************************************************/

Packet *
LISPEtrDatabase::reply_record(IPAddress eid, IPAddress itr_src)
{
    _lock.acquire_write(); // reply_record() records the ITR: write lock
    BaseEntry *it = lisp_lpm(_base, _lens, eid); // longest-prefix match
    if (!it) {
	_lock.release_write();
	return 0;
    }
    WritablePacket *q = Packet::make(sizeof(Record));
    if (!q) {
	_lock.release_write();
	return 0;
    }
    ++_response;
    Record *msg = (Record *)q->data();
    lisp_record_fill(msg, it->ttl, it->mask_len, _map_version, false,
	it->eid_prefix, 0, 100, _my_addr,
	LISP_LOC_F_LOCAL | LISP_LOC_F_REACH); // our own locator, up

    it->insert_itr(itr_src);
    _lock.release_write();
    return q;
}

/***********************************************************************************/

String
LISPEtrDatabase::read_handler(Element *e, void *user_data)
{
    LISPEtrDatabase *bse = (LISPEtrDatabase *)e;
    StringAccum sa;
    int i = 1;
    switch (reinterpret_cast<uintptr_t>(user_data)) {
    case h_base: {
	Base::iterator end_it = bse->_base.end();
	for (Base::iterator it = bse->_base.begin(); it != end_it; ++it) {
	    sa << '\n'
	       << i << " . " << "EID-Prefix :" << it->eid_prefix
	       << ", Mask :  " << IPAddress::make_prefix(it->mask_len)
	       << ", TTL : " << it->ttl << '\n'
	       << ", ETR cache : ";
	    i++;
	    for (ItrRloc *_itr = it->list.front(); _itr;
		 _itr = _itr->link.next())
		sa << _itr->itr_addr << '\n' << '\n';
	}
	break;
    }
    }

    return sa.take_string();
}

/***********************************************************************************/

void
LISPEtrDatabase::add_handlers()
{
    add_read_handler("base", read_handler, h_base);
    add_data_handlers("base length", Handler::OP_READ, &_entry_count);
    add_data_handlers(
	"entries", Handler::OP_READ, &_entry_count); // Click-style name
    add_data_handlers("map version", Handler::OP_READ, &_map_version);
    add_data_handlers(
	"map_version", Handler::OP_READ, &_map_version); // Click-style name
    add_data_handlers("rply", Handler::OP_READ, &_response);
    add_data_handlers(
	"replies", Handler::OP_READ, &_response); // Click-style name
    add_write_handler("add", write_handler, h_add);
    add_write_handler("remove", write_handler, h_remove);
}

void
LISPEtrDatabase::cleanup(CleanupStage)
{
    for (Base::iterator it = _base.begin(); it;) {
	BaseEntry *entry = _base.erase(it);
	entry->free_itrs();
	entry->~BaseEntry();
	_alloc.deallocate(entry);
    }
}

int
LISPEtrDatabase::remove(const LISPPrefix &key)
{
    _lock.acquire_write();
    Base::iterator it = _base.find(key);
    int found = it ? 1 : 0;
    if (it) {
	BaseEntry *entry = _base.erase(it);
	entry->free_itrs();
	_lens.remove(key.len);
	_alloc.deallocate(entry);
	--_entry_count;
	++_map_version;
    }
    _lock.release_write();
    return found;
}

// "add PREFIX [TTL]" and "remove PREFIX": change the local EID database.
int
LISPEtrDatabase::write_handler(
    const String &str, Element *e, void *user_data, ErrorHandler *errh)
{
    LISPEtrDatabase *eb = static_cast<LISPEtrDatabase *>(e);
    IPAddress a, m;
    uint32_t ttl = eb->_ttl;
    if (Args(eb, errh)
	    .push_back_words(str)
	    .read_mp("PREFIX", IPPrefixArg(true), a, m)
	    .read_p("TTL", ttl)
	    .complete()
	< 0)
	return -1;
    int len = m.mask_to_prefix_len();
    if (len < 0)
	return errh->error("non-contiguous mask");
    if (reinterpret_cast<uintptr_t>(user_data) == h_add)
	return eb->insert(a, len, ttl) ? 0 : errh->error("out of memory");
    return eb->remove(LISPPrefix(a, len)) ? 0
					  : errh->error("no such EID prefix");
}

/***********************************************************************************/

CLICK_ENDDECLS
EXPORT_ELEMENT(LISPEtrDatabase)
ELEMENT_MT_SAFE(LISPEtrDatabase)
