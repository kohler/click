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

DirectIPLookup::DirectIPLookup()
{
    flush_table();
}

DirectIPLookup::~DirectIPLookup()
{
}

int
DirectIPLookup::initialize(ErrorHandler *)
{
    return 0;
}

void
DirectIPLookup::push(int, Packet *p)
{
    IPAddress gw;
    int port = DirectIPLookup::lookup_route(p->dst_ip_anno(), gw);

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
    uint16_t vport_i = _tbl_0_23[ip_addr >> 8];

    if (vport_i & 0x8000)
        vport_i = _tbl_24_31[((vport_i & 0x7fff) << 8) | (ip_addr & 0xff)];

    gw = _vport[vport_i].gw;
    return _vport[vport_i].port;
}

int
DirectIPLookup::add_route(const IPRoute& route, bool allow_replace, IPRoute* old_route, ErrorHandler *errh)
{
    uint32_t start, end, i, j, sec_i, sec_start, sec_end, hash;
    uint32_t prefix = ntohl(route.addr.addr());
    uint32_t plen = route.prefix_len();
    int rt_i = find_entry(prefix, plen);
    uint16_t vport_i;

    if (rt_i >= 0) {
	// Attempt to replace an existing route.
	// Save the old route if requested so that a rollback can be performed
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
	// Allocate a new _rtable[] entry
	if (_rt_empty_head >= 0) {
	    rt_i = _rt_empty_head;
	    _rt_empty_head = _rtable[_rt_empty_head].ll_next;
	} else {
	    if (_rt_size == RT_SIZE_MAX)
		return -ENOMEM;
	    if (plen > 24 && (_sec_t_empty_head & 0x8000) &&
	       _sec_t_size == SEC_SIZE_MAX << 8)
		return -ENOMEM;
	    if (_vport_empty_head == -1 && _vport_t_size == VPORTS_MAX)
		return -ENOMEM;
	    rt_i = _rt_size++;
	}

	_rtable[rt_i].prefix = prefix;	// in host-order format
	_rtable[rt_i].plen = plen;

	// Insert the new entry in our hashtable
	hash = prefix_hash(prefix, plen);
	_rtable[rt_i].ll_prev = -1;
	_rtable[rt_i].ll_next = _rt_hashtbl[hash];
	if (_rt_hashtbl[hash] >= 0)
	    _rtable[_rt_hashtbl[hash]].ll_prev = rt_i;
	_rt_hashtbl[hash] = rt_i;
    }
   
    vport_i = vport_ref(route.gw, route.port);
    _rtable[rt_i].vport = vport_i;

    start = prefix >> 8;
    if (plen >= 24)
	end = start + 1;
    else
	end = start + (1 << (24 - plen));
    for (i = start; i < end; i++) {
	if (_tbl_0_23[i] & 0x8000) {
	    // Entries with plen > 24 already there in _tbl_24_31[]!
	    sec_i = (_tbl_0_23[i] & 0x7fff) << 8;
	    if (plen > 24) {
		sec_start = prefix & 0xFF;
		sec_end = sec_start + (1 << (32 - plen));
	    } else {
		sec_start = 0;
		sec_end = 256;
	    }
	    for (j = sec_i + sec_start; j < sec_i + sec_end; j++) {
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
		    if ((_sec_t_empty_head & 0x8000) == 0) {
			sec_i = _sec_t_empty_head << 8;
			_sec_t_empty_head = _tbl_24_31[sec_i];
		    } else {
			sec_i = _sec_t_size;
			_sec_t_size += 256;
		    }
		    sec_start = prefix & 0xFF;
		    sec_end = sec_start + (1 << (32 - plen));
		    for (j = 0; j < 256; j++) {
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
DirectIPLookup::remove_route(const IPRoute& route, IPRoute* old_route, ErrorHandler *errh)
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
		    _tbl_24_31[sec_i] = _sec_t_empty_head;
	    	    _sec_t_empty_head = sec_i >> 8;
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

int
DirectIPLookup::flush_handler(const String &, Element *e, void *,
				ErrorHandler *)
{
    DirectIPLookup *t = static_cast<DirectIPLookup *>(e);

    t->flush_table();
    return 0;
}

String
DirectIPLookup::dump_routes()
{
    StringAccum sa;
    for (uint32_t i = 0; i < PREF_HASHSIZE; i++)
	for (int rt_i = _rt_hashtbl[i]; rt_i >= 0 ; rt_i = _rtable[rt_i].ll_next) {
	    const CleartextEntry& rt = _rtable[rt_i];
	    if (_vport[rt.vport].port != -1) {
		IPRoute route = IPRoute(IPAddress(htonl(rt.prefix)), IPAddress::make_prefix(rt.plen), _vport[rt.vport].gw, _vport[rt.vport].port);
		route.unparse(sa, true) << '\n';
	    }
	}
    return sa.take_string();
}

void
DirectIPLookup::add_handlers()
{
    // Must keep those in sync with iproutetable.cc!  We need to have our
    // own add_handlers() in order to support the flush() operation.
    add_write_handler("add", add_route_handler, 0);
    add_write_handler("set", add_route_handler, (void*) 1);
    add_write_handler("remove", remove_route_handler, 0);
    add_write_handler("ctrl", ctrl_handler, 0);
    add_read_handler("table", table_handler, 0);
    set_handler("lookup", Handler::OP_READ | Handler::READ_PARAM | Handler::ONE_HOOK, lookup_handler);
    add_write_handler("flush", flush_handler, 0);
}

int
DirectIPLookup::find_entry(uint32_t prefix, uint32_t plen) const
{
    int rt_i;

    for (rt_i = _rt_hashtbl[prefix_hash(prefix,plen)]; rt_i >= 0 ;
      rt_i = _rtable[rt_i].ll_next)
	if (_rtable[rt_i].prefix == prefix && _rtable[rt_i].plen == plen)
	    return rt_i;
    return -1;
}

inline uint32_t
DirectIPLookup::prefix_hash(uint32_t prefix, uint32_t len) const
{
    // An arbitrary hash function - it'd better be good...
    uint32_t hash = prefix ^ (len << 5) ^ (prefix >> (len >> 2)) - len;
    hash ^= (hash >> 23) ^ ((hash >> 15) * len) ^ ((prefix >> 17) * 53);
    hash -= (prefix >> 3) ^ ((hash >> len) * 7) ^ ((hash >> 11) * 103);
    hash =  (hash ^ (hash >> 17)) & (PREF_HASHSIZE - 1);

    return hash;
};

void
DirectIPLookup::flush_table()
{
    memset(_rt_hashtbl, -1, sizeof(_rt_hashtbl));

    // _vport[0] is our "discard" port
    _vport_head = 0;
    _vport[0].ll_prev = -1;
    _vport[0].ll_next = -1;
    _vport[0].refcount = 1;		// _rtable[0] will point to _vport[0]
    _vport[0].gw = IPAddress(0);
    _vport[0].port = DISCARD_PORT;
    _vport_t_size = 1;
    _vport_empty_head = -1;

    // _rtable[0] is the default route entry
    _rt_hashtbl[prefix_hash(0,0)] = 0;
    _rtable[0].ll_prev = -1;
    _rtable[0].ll_next = -1;
    _rtable[0].prefix = 0;
    _rtable[0].plen = 0;
    _rtable[0].vport = 0;
    _rt_size = 1;
    _rt_empty_head = -1;

    // Bzeroed lookup tables resolve 0.0.0.0/0 to _vport[0]
    memset(&_tbl_0_23, 0, sizeof(_tbl_0_23));
    memset(&_tbl_24_31, 0, sizeof(_tbl_24_31));

    // Prefix len helper tables also have to be cleared
    memset(&_tbl_0_23_plen, 0, sizeof(_tbl_0_23_plen));
    memset(&_tbl_24_31_plen, 0, sizeof(_tbl_24_31_plen));

    _sec_t_size = 0;
    _sec_t_empty_head = 0x8000;
}

uint16_t
DirectIPLookup::vport_ref(IPAddress gw, int16_t port)
{
    int16_t vport_i;

    // Search for an existing entry
    for (vport_i = _vport_head; vport_i >= 0; vport_i = _vport[vport_i].ll_next)
	if (gw == _vport[vport_i].gw && port == _vport[vport_i].port)
	    break;

    if (vport_i >= 0)
	_vport[vport_i].refcount++;
    else {
	// Create a new vport entry
	if (_vport_empty_head >= 0) {
	    vport_i = _vport_empty_head;
	    _vport_empty_head = _vport[vport_i].ll_next;
	} else
	    vport_i = _vport_t_size++;
	_vport[vport_i].refcount = 1;
	_vport[vport_i].gw = gw;
	_vport[vport_i].port = port;

	// Add the entry to the vport linked list
	_vport[vport_i].ll_prev = -1;
	_vport[vport_i].ll_next = _vport_head;
	if (_vport_head >= 0)
	    _vport[_vport_head].ll_prev = vport_i;
	_vport_head = vport_i;
    }

    return vport_i;
}

void
DirectIPLookup::vport_unref(uint16_t vport_i)
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

CLICK_ENDDECLS
ELEMENT_REQUIRES(IPRouteTable userlevel|bsdmodule)
EXPORT_ELEMENT(DirectIPLookup)
