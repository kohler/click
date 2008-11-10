// -*- c-basic-offset: 4 -*-
/*
 * directiplookup.{cc,hh} -- lookup for output port and next-hop gateway
 * in one to max. two DRAM accesses with potential CPU cache / TLB misses
 * Marko Zec
 *
 * Copyright (c) 2005 International Computer Science Institute
 * Copyright (c) 2005 University of Zagreb
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
#include "directiplookup.hh"
#include <click/ipaddress.hh>
#include <click/straccum.hh>
#include <click/router.hh>
#include <click/error.hh>
CLICK_DECLS


// DIRECTIPLOOKUP::TABLE

// The DirectIPLookup table must be stored in a sub-object in the Linux
// kernel, because it's too large to be allocated all at once.

int
DirectIPLookup::Table::initialize()
{
    assert(!_tbl_0_23 && !_tbl_24_31 && !_vport && !_rtable && !_rt_hashtbl
	   && !_tbl_0_23_plen && !_tbl_24_31_plen);

    _tbl_24_31_capacity = 4096;
    _vport_capacity = 1024;
    _rtable_capacity = 2048;

    if ((_tbl_0_23 = (uint16_t *) CLICK_LALLOC((sizeof(uint16_t) + sizeof(uint8_t)) * (1 << 24)))
	&& (_tbl_24_31 = (uint16_t *) CLICK_LALLOC((sizeof(uint16_t) + sizeof(uint8_t)) * _tbl_24_31_capacity))
	&& (_vport = (VirtualPort *) CLICK_LALLOC(sizeof(VirtualPort) * _vport_capacity))
	&& (_rtable = (CleartextEntry *) CLICK_LALLOC(sizeof(CleartextEntry) * _rtable_capacity))
	&& (_rt_hashtbl = (int *) CLICK_LALLOC(sizeof(int) * PREF_HASHSIZE))) {
	_tbl_0_23_plen = (uint8_t *) (_tbl_0_23 + (1 << 24));
	_tbl_24_31_plen = (uint8_t *) (_tbl_24_31 + _tbl_24_31_capacity);
	return 0;
    } else
	return -ENOMEM;
}

void
DirectIPLookup::Table::cleanup()
{
    CLICK_LFREE(_tbl_0_23, (sizeof(uint16_t) + sizeof(uint8_t)) * (1 << 24));
    CLICK_LFREE(_tbl_24_31, (sizeof(uint16_t) + sizeof(uint8_t)) * _tbl_24_31_capacity);
    CLICK_LFREE(_vport, sizeof(VirtualPort) * _vport_capacity);
    CLICK_LFREE(_rtable, sizeof(CleartextEntry) * _rtable_capacity);
    CLICK_LFREE(_rt_hashtbl, sizeof(int) * PREF_HASHSIZE);
    _tbl_0_23 = _tbl_24_31 = 0;
    _vport = 0;
    _rtable = 0;
    _tbl_0_23_plen = _tbl_24_31_plen = 0;
    _rt_hashtbl = 0;
}


inline uint32_t
DirectIPLookup::Table::prefix_hash(uint32_t prefix, uint32_t len)
{
    // An arbitrary hash function - it'd better be good...
    uint32_t hash = prefix ^ (len << 5) ^ ((prefix >> (len >> 2)) - len);
    hash ^= (hash >> 23) ^ ((hash >> 15) * len) ^ ((prefix >> 17) * 53);
    hash -= (prefix >> 3) ^ ((hash >> len) * 7) ^ ((hash >> 11) * 103);
    hash =  (hash ^ (hash >> 17)) & (PREF_HASHSIZE - 1);

    return hash;
}

void
DirectIPLookup::Table::flush()
{
    memset(_rt_hashtbl, -1, sizeof(int) * PREF_HASHSIZE);

    // _vport[0] is our "discard" port
    _vport_head = 0;
    _vport[0].ll_prev = -1;
    _vport[0].ll_next = -1;
    _vport[0].refcount = 1;		// _rtable[0] will point to _vport[0]
    _vport[0].gw = IPAddress(0);
    _vport[0].port = DISCARD_PORT;
    _vport_size = 1;
    _vport_empty_head = -1;

    // _rtable[0] is the default route entry
    _rt_hashtbl[prefix_hash(0, 0)] = 0;
    _rtable[0].ll_prev = -1;
    _rtable[0].ll_next = -1;
    _rtable[0].prefix = 0;
    _rtable[0].plen = 0;
    _rtable[0].vport = 0;
    _rtable_size = 1;
    _rt_empty_head = -1;

    // Bzeroed lookup tables resolve 0.0.0.0/0 to _vport[0]
    memset(_tbl_0_23, 0, (sizeof(uint16_t) + sizeof(uint8_t)) * (1 << 24));

    _tbl_24_31_size = 0;
    _tbl_24_31_empty_head = 0x8000;
}

String
DirectIPLookup::Table::dump() const
{
    StringAccum sa;
    for (uint32_t i = 0; i < PREF_HASHSIZE; i++)
	for (int rt_i = _rt_hashtbl[i]; rt_i >= 0; rt_i = _rtable[rt_i].ll_next) {
	    const CleartextEntry &rt = _rtable[rt_i];
	    if (_vport[rt.vport].port != -1) {
		IPRoute route = IPRoute(IPAddress(htonl(rt.prefix)), IPAddress::make_prefix(rt.plen), _vport[rt.vport].gw, _vport[rt.vport].port);
		route.unparse(sa, true) << '\n';
	    }
	}
    return sa.take_string();
}

int
DirectIPLookup::Table::vport_find(IPAddress gw, int16_t port)
{
    for (int vp = _vport_head; vp >= 0; vp = _vport[vp].ll_next)
	if (_vport[vp].gw == gw && _vport[vp].port == port)
	    return vp;
    if (_vport_empty_head < 0 && _vport_size == _vport_capacity) {
	if (_vport_capacity == vport_capacity_limit)
	    return -ENOMEM;
	VirtualPort *new_vport = (VirtualPort *) CLICK_LALLOC(sizeof(VirtualPort) * 2 * _vport_capacity);
	if (!new_vport)
	    return -ENOMEM;
	memcpy(new_vport, _vport, sizeof(VirtualPort) * _vport_capacity);
	CLICK_LFREE(_vport, sizeof(VirtualPort) * _vport_capacity);
	_vport = new_vport;
	_vport_capacity *= 2;
    }
    if (_vport_empty_head < 0) {
	_vport[_vport_size].ll_next = _vport_empty_head;
	_vport_empty_head = _vport_size;
	++_vport_size;
    }
    _vport[_vport_empty_head].refcount = 0;
    return _vport_empty_head;
}

void
DirectIPLookup::Table::vport_unref(uint16_t vport_i)
{
    if (--_vport[vport_i].refcount == 0) {
	int16_t prev, next;

	// Prune our entry from the vport list
	prev = _vport[vport_i].ll_prev;
	next = _vport[vport_i].ll_next;
	if (prev >= 0)
	    _vport[prev].ll_next = next;
	else
	    _vport_head = next;
	if (next >= 0)
	    _vport[next].ll_prev = prev;

	// Add the entry to empty vports list
	_vport[vport_i].ll_next = _vport_empty_head;
	_vport_empty_head = vport_i;
    }
}

int
DirectIPLookup::Table::find_entry(uint32_t prefix, uint32_t plen) const
{
    int hash = prefix_hash(prefix, plen);
    for (int rt_i = _rt_hashtbl[hash]; rt_i >= 0; rt_i = _rtable[rt_i].ll_next)
	if (_rtable[rt_i].prefix == prefix && _rtable[rt_i].plen == plen)
	    return rt_i;
    return -1;
}

int
DirectIPLookup::Table::add_route(const IPRoute& route, bool allow_replace, IPRoute* old_route, ErrorHandler *errh)
{
    uint32_t prefix = ntohl(route.addr.addr());
    uint32_t plen = route.prefix_len();

    int rt_i = find_entry(prefix, plen);
    if (rt_i >= 0) {
	// Attempt to replace an existing route.
	// Save the old route if requested so that a rollback can be performed.
	if ((rt_i != 0 || (rt_i == 0 && _vport[0].port != DISCARD_PORT)) &&
	    old_route)
	    *old_route = IPRoute(IPAddress(htonl(_rtable[rt_i].prefix)),
				 IPAddress::make_prefix(_rtable[rt_i].plen),
				 _vport[_rtable[rt_i].vport].gw,
				 _vport[_rtable[rt_i].vport].port);
	if (rt_i == 0) {
	    // We actually only update the vport entry for the default route
	    if (_vport[0].port != DISCARD_PORT && !allow_replace)
		return -EEXIST;
	    _vport[0].gw = route.gw;
	    _vport[0].port = route.port;
	    return 0;
	}
	// Check if we allow for atomic route replacements at all
	if (!allow_replace)
	    return -EEXIST;
	vport_unref(_rtable[rt_i].vport);

    } else {
	// Attempt to allocate a new _rtable[] entry.
	if (_rt_empty_head < 0 && _rtable_size == _rtable_capacity) {
	    CleartextEntry *new_rtable = (CleartextEntry *) CLICK_LALLOC(sizeof(CleartextEntry) * _rtable_capacity * 2);
	    if (!new_rtable)
		return -ENOMEM;
	    memcpy(new_rtable, _rtable, sizeof(CleartextEntry) * _rtable_capacity);
	    CLICK_LFREE(_rtable, sizeof(CleartextEntry) * _rtable_capacity);
	    _rtable = new_rtable;
	    _rtable_capacity *= 2;
	}
	if (_rt_empty_head < 0) {
	    _rtable[_rtable_size].ll_next = _rt_empty_head;
	    _rt_empty_head = _rtable_size;
	    ++_rtable_size;
	}
	rt_i = _rt_empty_head;
    }

    // find vport index
    int vport_i = vport_find(route.gw, route.port);
    if (vport_i < 0)
	return vport_i;

    // find overflow table space
    int start = prefix >> 8;
    int end = start + (plen < 24 ? 1 << (24 - plen) : 1);
    if (plen > 24 && !(_tbl_0_23[start] & 0x8000)
	&& (_tbl_24_31_empty_head & 0x8000) != 0) {
	if (_tbl_24_31_size == _tbl_24_31_capacity
	    && _tbl_24_31_capacity >= tbl_24_31_capacity_limit)
	    return -ENOMEM;
	if (_tbl_24_31_size == _tbl_24_31_capacity) {
	    uint16_t *new_tbl = (uint16_t *) CLICK_LALLOC((sizeof(uint16_t) + sizeof(uint8_t)) * 2 * _tbl_24_31_capacity);
	    if (!new_tbl)
		return -ENOMEM;
	    memcpy(new_tbl, _tbl_24_31, sizeof(uint16_t) * _tbl_24_31_capacity);
	    memcpy(new_tbl + _tbl_24_31_capacity, _tbl_24_31_plen, sizeof(uint8_t) * _tbl_24_31_capacity);
	    CLICK_LFREE(_tbl_24_31, (sizeof(uint16_t) + sizeof(uint8_t)) * _tbl_24_31_capacity);
	    _tbl_24_31 = new_tbl;
	    _tbl_24_31_plen = (uint8_t *) (new_tbl + 2 * _tbl_24_31_capacity);
	    _tbl_24_31_capacity *= 2;
	}
	_tbl_24_31_empty_head = _tbl_24_31_size >> 8;
	_tbl_24_31[_tbl_24_31_empty_head << 8] = 0x8000;
	_tbl_24_31_size += 256;
    }

    // At this point we have successfully allocated all memory.
    if (rt_i == _rt_empty_head) {
	_rt_empty_head = _rtable[rt_i].ll_next;

	_rtable[rt_i].prefix = prefix;	// in host-order format
	_rtable[rt_i].plen = plen;

	// Insert the new entry in our hashtable
	uint32_t hash = prefix_hash(prefix, plen);
	_rtable[rt_i].ll_prev = -1;
	_rtable[rt_i].ll_next = _rt_hashtbl[hash];
	if (_rt_hashtbl[hash] >= 0)
	    _rtable[_rt_hashtbl[hash]].ll_prev = rt_i;
	_rt_hashtbl[hash] = rt_i;
    }

    if (vport_i == _vport_empty_head) {
	_vport_empty_head = _vport[vport_i].ll_next;
	_vport[vport_i].refcount = 0;
	_vport[vport_i].gw = route.gw;
	_vport[vport_i].port = route.port;

	// Add the entry to the vport linked list
	_vport[vport_i].ll_prev = -1;
	_vport[vport_i].ll_next = _vport_head;
	if (_vport_head >= 0)
	    _vport[_vport_head].ll_prev = vport_i;
	_vport_head = vport_i;
    }
    ++_vport[vport_i].refcount;
    _rtable[rt_i].vport = vport_i;

    for (int i = start; i < end; i++) {
	if (_tbl_0_23[i] & 0x8000) {
	    // Entries with plen > 24 already there in _tbl_24_31[]!
	    int sec_i = (_tbl_0_23[i] & 0x7fff) << 8, sec_start, sec_end;
	    if (plen > 24) {
		sec_start = prefix & 0xFF;
		sec_end = sec_start + (1 << (32 - plen));
	    } else {
		sec_start = 0;
		sec_end = 256;
	    }
	    for (int j = sec_i + sec_start; j < sec_i + sec_end; j++) {
		if (plen > _tbl_24_31_plen[j]) {
		    _tbl_24_31[j] = vport_i;
		    _tbl_24_31_plen[j] = plen;
		} else if (plen < _tbl_24_31_plen[j]) {
		    // Skip a sequence of more-specific entries
		    if (_tbl_24_31_plen[j] > 24) {
			j |= 0x000000ff >> (_tbl_24_31_plen[j] - 24);
		    } else {
			i |= 0x00ffffff >> _tbl_24_31_plen[j];
			break;
		    }
		} else if (allow_replace) {
		    _tbl_24_31[j] = vport_i;
		} else {
		    // plen == _tbl_24_31_plen[j] -> damn!
		    return errh->error("BUG: _tbl_24_31[%08X] collision", j);
		}
	    }
	} else {
	    if (plen > _tbl_0_23_plen[i]) {
		if (plen > 24) {
		    // Allocate a new _tbl_24_31[] entry and populate it
		    assert(!(_tbl_24_31_empty_head & 0x8000));
		    int sec_i = _tbl_24_31_empty_head << 8;
		    _tbl_24_31_empty_head = _tbl_24_31[sec_i];
		    int sec_start = prefix & 0xFF;
		    int sec_end = sec_start + (1 << (32 - plen));
		    for (int j = 0; j < 256; j++) {
			if (j >= sec_start && j < sec_end) {
			    _tbl_24_31[sec_i + j] = vport_i;
			    _tbl_24_31_plen[sec_i + j] = plen;
			} else {
			    _tbl_24_31[sec_i + j] = _tbl_0_23[i];
			    _tbl_24_31_plen[sec_i + j] = _tbl_0_23_plen[i];
			}
		    }
		    _tbl_0_23[i] = (sec_i >> 8) | 0x8000;
		} else {
		    _tbl_0_23[i] = vport_i;
		    _tbl_0_23_plen[i] = plen;
		}
	    } else if (plen < _tbl_0_23_plen[i]) {
		// Skip a sequence of more-specific entries
		i |= 0x00ffffff >> _tbl_0_23_plen[i];
	    } else if (allow_replace) {
		_tbl_0_23[i] = vport_i;
	    } else {
		// plen == _tbl_0_23_plen[i] - must never happen!!!
		return errh->error("BUG: _tbl_0_23[%08X] collision", i);
	    }
	}
    }

    return 0;
}

int
DirectIPLookup::Table::remove_route(const IPRoute& route, IPRoute* old_route, ErrorHandler *errh)
{
    uint32_t prefix = ntohl(route.addr.addr());
    uint32_t plen = route.prefix_len();
    int rt_i = find_entry(prefix, plen);
    IPRoute found_route;

    if (rt_i < 0 || (rt_i == 0 && _vport[0].port == DISCARD_PORT))
	return -ENOENT;

    found_route = IPRoute(IPAddress(htonl(_rtable[rt_i].prefix)),
			  IPAddress::make_prefix(_rtable[rt_i].plen),
			  _vport[_rtable[rt_i].vport].gw,
			  _vport[_rtable[rt_i].vport].port);
    if (!route.match(found_route))
	return -ENOENT;

    if (old_route)
	*old_route = found_route;

    if (plen == 0) {
	// Default route is a special case.  We never remove it from lookup
	// tables, but instead only point it to the "discard port".
	if (rt_i > 0)	// Must never happen, checking it just in case...
	    return errh->error("BUG: default route rt_i=%d, should be 0", rt_i);
	_vport[0].port = DISCARD_PORT;
    } else {
	uint32_t start, end, i, j, sec_i, sec_start, sec_end;
	int newent = -1;
	int newmask, prev, next;

	vport_unref(_rtable[rt_i].vport);

	// Prune our entry from the prefix/len hashtable
	prev = _rtable[rt_i].ll_prev;
	next = _rtable[rt_i].ll_next;
	if (prev >= 0)
	    _rtable[prev].ll_next = next;
	else
	    _rt_hashtbl[prefix_hash(prefix, plen)] = next;
	if (next >= 0)
	    _rtable[next].ll_prev = prev;

	// Add entry to the list of empty _rtable entries
	_rtable[rt_i].ll_next = _rt_empty_head;
	_rt_empty_head = rt_i;

	// Find an entry covering current prefix/len with the longest prefix.
	for (newmask = plen - 1 ; newmask >= 0 ; newmask--)
	    if (newmask == 0) {
		newent = 0;	// rtable[0] is always the default route
		break;
	    } else {
		newent = find_entry(prefix & (0xffffffff << (32 - newmask)),
				    newmask);
		if (newent > 0)
		    break;
	    }

	// Replace prefix/plen with newent/mask in lookup tables
	start = prefix >> 8;
	if (plen >= 24)
	    end = start + 1;
	else
	    end = start + (1 << (24 - plen));
	for (i = start; i < end; i++) {
	    if (_tbl_0_23[i] & 0x8000) {
		sec_i = (_tbl_0_23[i] & 0x7fff) << 8;
		if (plen > 24) {
		    sec_start = prefix & 0xFF;
		    sec_end = sec_start + (1 << (32 - plen));
		} else {
		    sec_start = 0;
		    sec_end = 256;
		}
		for (j = sec_i + sec_start; j < sec_i + sec_end; j++) {
		    if (plen == _tbl_24_31_plen[j]) {
			_tbl_24_31[j] = _rtable[newent].vport;
			_tbl_24_31_plen[j] = newmask;
		    } else if (plen < _tbl_24_31_plen[j]) {
			// Skip a sequence of more-specific entries
			if (_tbl_24_31_plen[j] > 24) {
			    j |= 0x000000ff >> (_tbl_24_31_plen[j] - 24);
			} else {
			    i |= 0x00ffffff >> _tbl_24_31_plen[j];
			    break;
			}
		    } else {
			// plen > _tbl_24_31_plen[j] -> damn!
			return
			  errh->error("BUG: _tbl_24_31[%08X] inconsistency", j);
		    }
		}
		// Check if we can prune the entire secondary table range?
		for (j = sec_i ; j < sec_i + 255; j++)
		    if (_tbl_24_31_plen[j] != _tbl_24_31_plen[j+1])
			break;
		if (j == sec_i + 255) {
		    // Yup, adjust entries in primary tables...
		    _tbl_0_23[i] = _tbl_24_31[sec_i];
		    _tbl_0_23_plen[i] = _tbl_24_31_plen[sec_i];
		    // ... and free up the entry (adding it to free space list)
		    _tbl_24_31[sec_i] = _tbl_24_31_empty_head;
		    _tbl_24_31_empty_head = sec_i >> 8;
		}
	    } else {
		if (plen == _tbl_0_23_plen[i]) {
		    _tbl_0_23[i] = _rtable[newent].vport;
		    _tbl_0_23_plen[i] = newmask;
		} else if (plen < _tbl_0_23_plen[i]) {
		    // Skip a sequence of more-specific entries
		    i |= 0x00ffffff >> _tbl_0_23_plen[i];
		}
	    }
	}
    }
    return 0;
}


// DIRECTIPLOOKUP

DirectIPLookup::DirectIPLookup()
{
}

DirectIPLookup::~DirectIPLookup()
{
}

int
DirectIPLookup::configure(Vector<String> &conf, ErrorHandler *errh)
{
    int r;
    if ((r = _t.initialize()) < 0)
	return r;
    _t.flush();
    return IPRouteTable::configure(conf, errh);
}

void
DirectIPLookup::cleanup(CleanupStage)
{
    _t.cleanup();
}

void
DirectIPLookup::push(int, Packet *p)
{
    IPAddress gw;
    int port = lookup_route(p->dst_ip_anno(), gw);

    if (port >= 0) {
        if (gw)
            p->set_dst_ip_anno(gw);
        output(port).push(p);
    } else
        p->kill();
}

int
DirectIPLookup::lookup_route(IPAddress dest, IPAddress &gw) const
{
    uint32_t ip_addr = ntohl(dest.addr());
    uint16_t vport_i = _t._tbl_0_23[ip_addr >> 8];

    if (vport_i & 0x8000)
        vport_i = _t._tbl_24_31[((vport_i & 0x7fff) << 8) | (ip_addr & 0xff)];

    gw = _t._vport[vport_i].gw;
    return _t._vport[vport_i].port;
}

int
DirectIPLookup::add_route(const IPRoute& route, bool allow_replace, IPRoute* old_route, ErrorHandler *errh)
{
    return _t.add_route(route, allow_replace, old_route, errh);
}

int
DirectIPLookup::remove_route(const IPRoute& route, IPRoute* old_route, ErrorHandler *errh)
{
    return _t.remove_route(route, old_route, errh);
}

int
DirectIPLookup::flush_handler(const String &, Element *e, void *,
				ErrorHandler *)
{
    DirectIPLookup *t = static_cast<DirectIPLookup *>(e);
    t->_t.flush();
    return 0;
}

String
DirectIPLookup::dump_routes()
{
    return _t.dump();
}

void
DirectIPLookup::add_handlers()
{
    IPRouteTable::add_handlers();
    add_write_handler("flush", flush_handler, 0, Handler::BUTTON);
}

CLICK_ENDDECLS
ELEMENT_REQUIRES(IPRouteTable userlevel|bsdmodule)
EXPORT_ELEMENT(DirectIPLookup)
