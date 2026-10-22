/*
 * itrcache.{cc,hh} -- ITR map-cache: EID prefix -> locator set (LISPMapCache)
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
#include "itrcache.hh"
#include <click/args.hh>
#include <click/bitvector.hh>
#include <click/straccum.hh>
#include <click/router.hh>
#include <click/error.hh>
#include <click/glue.hh>

CLICK_DECLS

/***********************************************************************/

LISPMapCache::LISPMapCache()
  : _timer(this), _entry_capacity(0), _packet_capacity(2048)
{
    _entry_count = _rply_count = _packet_count = _drops = _modif_entry = 0;
}

/***********************************************************************/

LISPMapCache::~LISPMapCache()
{
}

/***********************************************************************/

int
LISPMapCache::configure(Vector<String> &conf, ErrorHandler *errh)
{
    _rqst_timeout = 300;
    if (Args(conf, this, errh)
	    .read("RQST_TIMEOUT", _rqst_timeout)
	    .read("CAPACITY", _packet_capacity)
	    .read("ENTRY_CAPACITY", _entry_capacity)
	    .complete()
	< 0)
	return -1;
    return 0;
}

int
LISPMapCache::initialize(ErrorHandler *)
{
    _timer.initialize(this);
    _timer.schedule_after_sec(1);
    return 0;
}

void
LISPMapCache::cleanup(CleanupStage)
{
    clear(); // free entries and queued packets
}

/***********************************************************************/

void
LISPMapCache::clear()
{
    for (Cache::iterator it = _cache.begin(); it;) {
	CacheEntry *ce = _cache.erase(it);
	while (Packet *p = ce->list_head) {
	    ce->list_head = p->next();
	    p->kill();
	}
	_alloc.deallocate(ce);
    }
    _entry_count = _rply_count = _packet_count = 0;
    _lens.clear();
    _resp_list.__clear();
    _rqst_list.__clear();
}

/***********************************************************************/

void
LISPMapCache::rqst_clean()
{
    CacheEntry *entry;
    click_jiffies_t now = click_jiffies();
    while ((entry = _rqst_list.front())
	&& (entry->rqst_expired(now, _rqst_timeout))) {
	_cache.erase(entry->prefix);
	_lens.remove(entry->prefix.len);
	_rqst_list.pop_front();
	Packet *p;
	while ((p = entry->list_head)) {
	    entry->list_head = p->next();
	    p->kill();
	    _packet_count--;
	    _drops++;
	}
	_alloc.deallocate(entry);
	_entry_count--;
    }
    while (entry && _packet_capacity && _packet_count > _packet_capacity) {
	while (entry->list_head && _packet_count > _packet_capacity) {
	    Packet *p = entry->list_head;
	    if (!(entry->list_head = p->next()))
		entry->list_tail = 0;
	    p->kill();
	    _packet_count--;
	    _drops++;
	}
	entry = entry->link.next();
    }
}

/***********************************************************************/

void
LISPMapCache::resp_clean()
{
    CacheEntry *entry;
    while ((entry = _resp_list.front())
	&& (entry->resp_expired(click_jiffies())
	    || (_entry_capacity && _entry_count > _entry_capacity))) {
	_cache.erase(entry->prefix);
	_lens.remove(entry->prefix.len);
	_resp_list.pop_front();
	_rply_count--;
	_alloc.deallocate(entry);
	_entry_count--;
    }
    return;
};

/***********************************************************************/
// Every second: drop requests that got no reply in time and mappings whose
// TTL has expired, so the next packet triggers a new Map-Request.
void
LISPMapCache::run_timer(Timer *)
{
    _lock.acquire_write();
    rqst_clean();
    resp_clean();
    _lock.release_write();
    _timer.reschedule_after_sec(1);
}
/***********************************************************************/

LISPMapCache::CacheEntry *
LISPMapCache::pending_entry(IPAddress eid)
{
    _lock.acquire_write();
    CacheEntry *ce = pending_entry_locked(eid);
    _lock.release_write();
    return ce;
}

// Find or create the entry for EID.  The caller holds the write lock.
LISPMapCache::CacheEntry *
LISPMapCache::pending_entry_locked(IPAddress eid)
{
    LISPPrefix key(eid, 32); // pending request for this EID
    Cache::iterator it = _cache.find(key);
    if (!it) {
	void *new_entry = _alloc.allocate();
	if (!new_entry)
	    return 0;
	_entry_count++;
	if (_entry_capacity && _entry_count > _entry_capacity)
	    resp_clean();
	CacheEntry *ce = new (new_entry) CacheEntry(key);
	_cache.set(it, ce);
	_lens.add(32);
	_rqst_list.push_back(ce);
    }

    return it.get();
}

/***********************************************************************/

int
LISPMapCache::insert(
    IPAddress eid, uint16_t port, uint32_t nonce1, uint32_t nonce2)
{
    _lock.acquire_write();
    CacheEntry *entry = pending_entry_locked(eid);
    if (!entry) {
	_lock.release_write(); // was left held on this path
	return 0;
    }
    entry->randport = port;
    entry->randnonce1 = nonce1;
    entry->randnonce2 = nonce2;
    entry->send_time = click_jiffies();
    _cache.balance();
    _lock.release_write();
    return 1;
}

/***********************************************************************/

void
LISPMapCache::queue_packet(IPAddress eid, Packet *p)
{
    _lock.acquire_write();
    _packet_count++;
    if (_packet_capacity && _packet_count > _packet_capacity)
	rqst_clean();
    Cache::iterator it = _cache.find(LISPPrefix(eid, 32)); // pending entry
    if (it) {
	if (it->list_tail)
	    it->list_tail->set_next(p);
	else
	    it->list_head = p;
	it->list_tail = p;
	p->set_next(0);
    } else {
	// no pending request for this EID: the packet can't be queued
	_packet_count--;
	_drops++;
	p->kill();
    }
    _lock.release_write();
}

/***********************************************************************/

LISPMapCache::CacheEntry *
LISPMapCache::pending_for(
    const LISPPrefix &prefix, uint32_t nonce1, uint32_t nonce2)
{
    // new requests are in rqst_list; refresh requests (after an SMR) are
    // resolved entries in resp_list that carry the nonces of the request
    for (CacheEntry *ce = _rqst_list.front(); ce; ce = ce->link.next())
	if (prefix.contains(ce->prefix.addr) && ce->randnonce1 == nonce1
	    && ce->randnonce2 == nonce2)
	    return ce;
    for (CacheEntry *ce = _resp_list.front(); ce; ce = ce->link.next())
	if (prefix.contains(ce->prefix.addr) && ce->randnonce1 == nonce1
	    && ce->randnonce2 == nonce2)
	    return ce;
    return 0;
}

bool
LISPMapCache::has_locator(IPAddress eid, IPAddress rloc)
{
    _lock.acquire_read();
    bool found = false;
    CacheEntry *e = lisp_lpm(_cache, _lens, eid, &CacheEntry::is_complete);
    for (int i = 0; e && i < e->nlocs && !found; ++i)
	found = IPAddress(e->locs[i].locator) == rloc;
    _lock.release_read();
    return found;
}

Packet *
LISPMapCache::insert_response(
    const Record *rec, uint32_t nonce1, uint32_t nonce2, IPAddress *locator)
{
    _lock.acquire_write();
    Packet *list = 0;
    LISPPrefix key(rec->eid_prefix, rec->mask_len);
    CacheEntry *pend = pending_for(key, nonce1, nonce2);
    if (pend) {
	// queued packets leave with the pending entry
	list = pend->list_head;
	pend->list_head = pend->list_tail = 0;
	for (Packet *p = list; p; p = p->next())
	    _packet_count--;
	// the resolved entry is keyed by the mapped prefix
	CacheEntry *res = pend;
	pend->randnonce1 = pend->randnonce2 = 0; // a nonce is used once
	if (!(key == pend->prefix)) {
	    res = _cache.get(key);
	    if (!res) {
		void *mem = _alloc.allocate();
		if (mem) {
		    res = new (mem) CacheEntry(key);
		    _cache.set(res);
		    _lens.add(key.len);
		    _entry_count++;
		}
	    } else if (!res->complete)
		_rqst_list.erase(res);
	    else
		_resp_list.erase(res);
	    // forget the pending request
	    if (pend->complete)
		_resp_list.erase(pend);
	    else
		_rqst_list.erase(pend);
	    _cache.erase(pend->prefix);
	    _lens.remove(pend->prefix.len);
	    _alloc.deallocate(pend);
	    _entry_count--;
	} else if (pend->complete)
	    _resp_list.erase(pend); // refreshed mapping
	else
	    _rqst_list.erase(pend); // pending -> resolved
	if (res) {
	    res->rec_ttl = lisp_record_ttl(rec);
	    res->version_num = lisp_record_version(rec);
	    res->nlocs
		= rec->loc_count < LISP_MAX_LOC ? rec->loc_count : LISP_MAX_LOC;
	    for (int i = 0; i < res->nlocs; ++i)
		res->locs[i] = *lisp_record_locator(rec, i);
	    int li
		= lisp_choose_locator(res->locs, res->nlocs, key.addr.addr());
	    res->rloc = *locator
		= li >= 0 ? IPAddress(res->locs[li].locator) : IPAddress();
	    res->complete = true;
	    res->insert_time = click_jiffies();
	    _resp_list.push_back(res);
	}
    }
    _rply_count++;
    _lock.release_write();
    return list;
}

/***********************************************************************/

void
LISPMapCache::remove_mapping(IPAddress eid)
{
    _lock.acquire_write();
    // the resolved mapping that covers EID
    CacheEntry *entry = lisp_lpm(_cache, _lens, eid, &CacheEntry::is_complete);
    if (entry) {
	_cache.erase(entry->prefix);
	_lens.remove(entry->prefix.len);
	_resp_list.erase(entry); // unlink before freeing
	_alloc.deallocate(entry);
	_entry_count--;
    }
    _lock.release_write();
}

/***********************************************************************/

String
LISPMapCache::read_handler(Element *e, void *user_data)
{
    LISPMapCache *itrcache = (LISPMapCache *)e;
    StringAccum sa;
    switch (reinterpret_cast<uintptr_t>(user_data)) {
    case h_cache: {
	for (CacheEntry *ce = itrcache->_resp_list.front(); ce;
	     ce = ce->link.next()) {
	    sa << ce->prefix << " -> " << ce->rloc << "  ttl " << ce->rec_ttl
	       << " min  version " << ce->version_num << '\n';
	}
	break;
    }
    }
    return sa.take_string();
}

/***********************************************************************/

/***********************************************************************/

void
LISPMapCache::add_handlers()
{
    add_read_handler("cache", read_handler, h_cache);
    add_data_handlers("drops", Handler::OP_READ, &_drops);
    add_data_handlers("entry count", Handler::OP_READ, &_entry_count);
    add_data_handlers(
	"entries", Handler::OP_READ, &_entry_count); // Click-style name
    add_data_handlers("modif entry", Handler::OP_READ, &_modif_entry);
    add_data_handlers(
	"modified", Handler::OP_READ, &_modif_entry); // Click-style name
    add_data_handlers("packet  count", Handler::OP_READ, &_packet_count);
    add_data_handlers(
	"queued", Handler::OP_READ, &_packet_count); // Click-style name
    add_data_handlers("reply count", Handler::OP_READ, &_rply_count);
    add_data_handlers(
	"replies", Handler::OP_READ, &_rply_count); // Click-style name
}

/***********************************************************************/

CLICK_ENDDECLS
EXPORT_ELEMENT(LISPMapCache)
ELEMENT_MT_SAFE(LISPMapCache)
