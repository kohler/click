// -*- c-basic-offset: 4; related-file-name: "../include/click/nameinfo.hh" -*-
/*
 * nameinfo.{cc,hh} -- stores name information
 * Eddie Kohler
 *
 * Copyright (c) 2005-2008 The Regents of the University of California
 * Copyright (c) 2011 Meraki, Inc.
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
#include <click/nameinfo.hh>
#include <click/glue.hh>
#include <click/confparse.hh>
#include <click/router.hh>
#include <click/error.hh>
#include <click/packet_anno.hh>
CLICK_DECLS

/** @file nameinfo.hh
 * @brief Click databases for names, such as IP addresses and services.
 */

/** @class NameInfo
 * @brief Manages Click name databases.
 *
 * NameInfo is the user interface to Click's name databases, a general
 * mechanism for mapping names to values.  NameInfo and NameDB implement
 * generally useful properties like names local to a compound element context.
 *
 * Every name database maps names, which are nonempty strings, to values,
 * which are sequences of bytes.  A database is identified by its type, which
 * indicates the kind of name stored in the database, and, optionally, its
 * compound element context, which is a pair of router and element name
 * prefix.  The database is used for queries that take place in the given
 * context.  A query bubbles up through the databases, starting with the most
 * specific context and getting more general, until it succeeds.  A database
 * stores values with a fixed size; it is an error to query a database with
 * the wrong size.
 *
 * Many common types are defined in the NameDB::DBType enumeration, but
 * NameInfo allows arbitrary other types to be added at run time.
 *
 * Here's an example of how to query NameInfo:
 *
 * @code
 * EtherAddress ethaddr;
 * if (NameInfo::query(NameInfo::T_ETHERNET_ADDR, this, name, &ethaddr, 6))
 *     click_chatter("Found address!");
 * else
 *     click_chatter("Didn't find address.");
 * @endcode
 *
 * Many elements also add their own databases to NameInfo; see, for example,
 * the IPNameInfo or IPFilter elements for examples.
 */

/** @class NameDB
 * @brief Superclass for databases mapping names to values.
 *
 * NameDB databases are Click's general way to map names to values.  Many
 * Click elements need to parse names into values of various types.  They use
 * NameDB to do it; NameDB and NameInfo implement generally useful properties
 * like names local to a compound element context, and support space- and
 * time-efficient lookups.
 *
 * Most users should access NameDB's functionality through NameInfo.
 *
 * @sa NameInfo
 */

/** @class StaticNameDB
 * @brief A fixed database mapping names to 4-byte integer values.
 *
 * StaticNameDB is a NameDB database that maps names to 4-byte integer values.
 * The database contents are taken from an array of Entry structures passed in
 * to the constructor.  Each Entry specifies a name-value definition.  The
 * entries must be sorted by name.  This is the most space-efficient way to
 * implement a static name database for integers.
 *
 * StaticNameDB define() operations always fail.
 *
 * @sa NameInfo, NameDB
 */

/** @class DynamicNameDB
 * @brief A modifiable database mapping names to arbitrary values.
 *
 * DynamicNameDB is a NameDB database that maps names to arbitrary values.
 * The database is initially empty.  DynamicNameDB supports define()
 * operations; that's how information is added to it.
 *
 * DynamicNameDB objects are automatically created by NameInfo::getdb().
 *
 * @sa NameInfo, NameDB
 */

static NameInfo *the_name_info;

#define MKAI(n) MAKE_ANNOTATIONINFO(n ## _ANNO_OFFSET, n ## _ANNO_SIZE)

static const StaticNameDB::Entry annotation_entries[] = {
    { "AGGREGATE", MKAI(AGGREGATE) },
    { "DST_IP", MKAI(DST_IP) },
    { "DST_IP6", MKAI(DST_IP6) },
    { "EXTRA_LENGTH", MKAI(EXTRA_LENGTH) },
    { "EXTRA_PACKETS", MKAI(EXTRA_PACKETS) },
    { "FIRST_TIMESTAMP", MKAI(FIRST_TIMESTAMP) },
    { "FIX_IP_SRC", MKAI(FIX_IP_SRC) },
    { "FWD_RATE", MKAI(FWD_RATE) },
    { "GRID_ROUTE_CB", MKAI(GRID_ROUTE_CB) },
    { "ICMP_PARAMPROB", MKAI(ICMP_PARAMPROB) },
    { "IPREASSEMBLER", MKAI(IPREASSEMBLER) },
#ifdef IPSEC_SA_DATA_REFERENCE_ANNO_OFFSET
    { "IPSEC_SA_DATA_REFERENCE", MKAI(IPSEC_SA_DATA_REFERENCE) },
#endif
    { "IPSEC_SPI", MKAI(IPSEC_SPI) },
    { "MISC_IP", MKAI(MISC_IP) },
    { "PACKET_NUMBER", MKAI(PACKET_NUMBER) },
    { "PAINT", MKAI(PAINT) },
#if HAVE_INT64_TYPES
    { "PERFCTR", MKAI(PERFCTR) },
#endif
    { "REV_RATE", MKAI(REV_RATE) },
    { "SEQUENCE_NUMBER", MKAI(SEQUENCE_NUMBER) },
    { "VLAN", MKAI(VLAN_TCI) },
    { "VLAN_TCI", MKAI(VLAN_TCI) },
    { "WIFI_EXTRA", MKAI(WIFI_EXTRA) }
};

bool
NameDB::define(const String &, const void *, size_t)
{
    return false;
}

String
NameDB::revquery(const void *, size_t)
{
    return String();
}

bool
StaticNameDB::query(const String &name, void *value, size_t vsize)
{
    assert(vsize == 4);
    const char *namestr = name.c_str();
    size_t l = 0, r = _nentries;
    // common case: looking up an integer in a name table
    if ((unsigned char) namestr[0] < (unsigned char) _entries[0].name[0])
	r = 0;
    while (l < r) {
	size_t m = l + (r - l) / 2;
	int cmp = strcmp(namestr, _entries[m].name);
	if (cmp == 0) {
	    *reinterpret_cast<uint32_t *>(value) = _entries[m].value;
	    return true;
	} else if (cmp < 0)
	    r = m;
	else
	    l = m + 1;
    }
    return false;
}

String
StaticNameDB::revquery(const void *value, size_t vsize)
{
    assert(vsize == 4);
    uint32_t ivalue;
    memcpy(&ivalue, value, 4);
    for (size_t i = 0; i < _nentries; i++)
	if (_entries[i].value == ivalue)
	    return String::make_stable(_entries[i].name);
    return String();
}


void *
DynamicNameDB::find(const String &name, bool create)
{
    if (_sorted > 20)
	sort();

    if (_sorted == 100) {
	size_t l = 0, r = _names.size();
	while (l < r) {
	    size_t m = l + (r - l) / 2;
	    int cmp = String::compare(name, _names[m]);
	    if (cmp == 0)
		return _values.data() + value_size() * m;
	    else if (cmp < 0)
		r = m;
	    else
		l = m + 1;
	}
    } else {
	_sorted++;
	for (int i = 0; i < _names.size(); i++)
	    if (name == _names[i])
		return _values.data() + value_size() * i;
    }

    if (create && name) {
	_sorted = 0;
	_names.push_back(name);
	_values.extend(value_size());
	return _values.data() + _values.length() - value_size();
    } else
	return 0;
}

static int
namelist_sort_compar(const void *athunk, const void *bthunk, void *othunk)
{
    const int *a = (const int *) athunk, *b = (const int *) bthunk;
    const String *o = (const String *) othunk;
    return String::compare(o[*a], o[*b]);
}

void
DynamicNameDB::sort()
{
    if (_sorted == 100 || _names.size() == 0)
	return;

    Vector<int> permutation(_names.size(), 0);
    for (int i = 0; i < _names.size(); i++)
	permutation[i] = i;
    click_qsort(permutation.begin(), permutation.size(), sizeof(int), namelist_sort_compar, _names.begin());

    Vector<String> new_names(_names.size(), String());
    StringAccum new_values(_values.length());
    new_values.extend(_values.length());

    String *nn = new_names.begin();
    char *nv = new_values.data();
    for (int i = 0; i < _names.size(); i++, nn++, nv += value_size()) {
	*nn = _names[permutation[i]];
	memcpy(nv, _values.data() + value_size() * permutation[i], value_size());
    }

    _names.swap(new_names);
    _values.swap(new_values);
    _sorted = 100;
}

bool
DynamicNameDB::query(const String& name, void* value, size_t vsize)
{
    assert(value_size() == vsize);
    if (void *x = find(name, false)) {
	memcpy(value, x, vsize);
	return true;
    } else
	return false;
}

bool
DynamicNameDB::define(const String& name, const void* value, size_t vsize)
{
    assert(value_size() == vsize);
    if (void *x = find(name, true)) {
	memcpy(x, value, vsize);
	return true;
    } else
	return false;
}


String
DynamicNameDB::revquery(const void *value, size_t vsize)
{
    const uint8_t *dx = (const uint8_t *) _values.data();
    for (int i = 0; i < _names.size(); i++, dx += vsize)
	if (memcmp(dx, value, vsize) == 0)
	    return _names[i];
    return String();
}


NameInfo::NameInfo()
{
#if CLICK_NAMEDB_CHECK
    _check_generation = (uintptr_t) this;
#endif
}

NameInfo::~NameInfo()
{
    for (int i = 0; i < _namedbs.size(); i++) {
	_namedbs[i]->_installed = 0;
	delete _namedbs[i];
    }
}

void
NameInfo::static_initialize()
{
    the_name_info = new NameInfo;
    NameDB *db = new StaticNameDB(NameInfo::T_ANNOTATION, String(), annotation_entries, sizeof(annotation_entries) / sizeof(annotation_entries[0]));
    NameInfo::installdb(db, 0);
}

void
NameInfo::static_cleanup()
{
    delete the_name_info;
}

#if 0
String
NameInfo::NameList::rlookup(uint32_t val)
{
    assert(_value_size == 4);
    const uint32_t *x = (const uint32_t *) _values.value_size();
    for (int i = 0; i < _names.size(); i++)
	if (x[i] == val)
	    return _names[i];
    return String();
}
#endif

NameDB *
NameInfo::namedb(uint32_t type, size_t vsize, const String &prefix, NameDB *install)
{
    NameDB *db;

    // binary-search types
    size_t l = 0, r = _namedb_roots.size(), m;
    while (l < r) {
	m = l + (r - l) / 2;
	if (type == _namedb_roots[m]->_type)
	    goto found_root;
	else if (type < _namedb_roots[m]->_type)
	    r = m;
	else
	    l = m + 1;
    }

    // type not found
    if (install == install_dynamic_sentinel())
	install = new DynamicNameDB(type, prefix, vsize);
    if (install) {
	assert(!install->_installed);
	install->_installed = this;
	_namedbs.push_back(install);
	_namedb_roots.insert(_namedb_roots.begin() + l, install);
	return install;
    } else
	return 0;

  found_root:
    // walk tree to find prefix match; keep track of closest prefix
    db = _namedb_roots[m];
    NameDB *closest = 0;
    while (db) {
	if (db->_context.length() <= prefix.length()
	    && memcmp(db->_context.data(), prefix.data(), db->_context.length()) == 0) {
	    closest = db;
	    db = db->_context_child;
	} else
	    db = db->_context_sibling;
    }

    // prefix found?
    if (closest && closest->_context == prefix) {
	assert(closest->_value_size == vsize);
	return closest;
    }

    // prefix not found
    if (install == install_dynamic_sentinel())
	install = new DynamicNameDB(type, prefix, vsize);
    if (install) {
	assert(!install->_installed);
	install->_installed = this;
	_namedbs.push_back(install);
	install->_context_parent = closest;
	NameDB **pp = (closest ? &closest->_context_child : &_namedb_roots[m]);
	install->_context_sibling = *pp;
	*pp = install;
	// adopt nodes that should be our children
	pp = &install->_context_sibling;
	while (*pp) {
	    if (prefix.length() < (*pp)->_context.length()
		&& memcmp((*pp)->_context.data(), prefix.data(), prefix.length()) == 0) {
		NameDB *new_child = *pp;
		*pp = new_child->_context_sibling;
		new_child->_context_parent = install;
		new_child->_context_sibling = install->_context_child;
		install->_context_child = new_child;
	    } else
		pp = &(*pp)->_context_sibling;
	}
	return install;
    } else {
	assert(!closest || closest->_value_size == vsize);
	return closest;
    }
}

NameDB *
NameInfo::getdb(uint32_t type, const Element *e, size_t vsize, bool create)
{
    if (e) {
	if (NameInfo *ni = (create ? e->router()->force_name_info() : e->router()->name_info())) {
	    NameDB *install = (create ? ni->install_dynamic_sentinel() : 0);
	    String ename = e->name();
	    int last_slash = ename.find_right('/');
	    if (last_slash >= 0)
		return ni->namedb(type, vsize, ename.substring(0, last_slash + 1), install);
	    else
		return ni->namedb(type, vsize, String(), install);
	}
    }

    NameDB *install = (create ? the_name_info->install_dynamic_sentinel() : 0);
    return the_name_info->namedb(type, vsize, String(), install);
}

void
NameInfo::installdb(NameDB *db, const Element *prefix)
{
    NameInfo *ni = (prefix ? prefix->router()->force_name_info() : the_name_info);
    NameDB *curdb = ni->namedb(db->type(), db->value_size(), db->context(), db);
    if (curdb && curdb != db) {
	assert(!curdb->_context_child || curdb->_context_child->context().length() > db->context().length());
	assert(!db->_installed);
	db->_installed = ni;
	db->_context_child = curdb->_context_child;
	db->_context_parent = curdb;
	curdb->_context_child = db;
	for (NameDB *child = db->_context_child; child; child = child->_context_sibling)
	    child->_context_parent = db;
	ni->_namedbs.push_back(db);
    }
#if CLICK_NAMEDB_CHECK
    ni->check(ErrorHandler::default_handler());
#endif
}

void
NameInfo::uninstalldb(NameDB *db)
{
    if (!db->_installed)
	return;

    // This is an uncommon operation, so don't worry about its performance.
    NameInfo *ni = db->_installed;
    int m;
    for (m = 0; m < ni->_namedb_roots.size(); m++)
	if (ni->_namedb_roots[m]->_type == db->_type)
	    break;

    NameDB **pp = (db->_context_parent ? &db->_context_parent->_context_child
		   : &ni->_namedb_roots[m]);
    // Remove from sibling list
    for (NameDB *sib = *pp; sib != db; sib = sib->_context_sibling)
	/* do nothing */;
    // Patch children in
    *pp = db->_context_sibling;
    while (NameDB *cdb = db->_context_child) {
	db->_context_child = cdb->_context_sibling;
	cdb->_context_parent = db->_context_parent;
	cdb->_context_sibling = *pp;
	*pp = cdb;
    }
    // Maybe remove root
    if (!*pp && !db->_context_parent)
	ni->_namedb_roots.erase(pp);
    // Remove from _namedbs
    for (int i = 0; i < ni->_namedbs.size(); i++)
	if (ni->_namedbs[i] == db) {
	    ni->_namedbs[i] = ni->_namedbs.back();
	    ni->_namedbs.pop_back();
	    break;
	}
    // Mark as not installed
    db->_installed = 0;

#if CLICK_NAMEDB_CHECK
    ni->check(ErrorHandler::default_handler());
#endif
}

bool
NameInfo::query(uint32_t type, const Element *e, const String &name, void *value, size_t vsize)
{
    while (1) {
	NameDB *db = getdb(type, e, vsize, false);
	while (db) {
	    if (db->query(name, value, vsize))
		return true;
	    db = db->context_parent();
	}
	if (!e)
	    return false;
	e = 0;
    }
}

bool
NameInfo::query_int(uint32_t type, const Element *e, const String &name, int32_t *value)
{
    return query(type, e, name, value, 4) || IntArg().parse(name, *value);
}

bool
NameInfo::query_int(uint32_t type, const Element *e, const String &name, uint32_t *value)
{
    return query(type, e, name, value, 4) || IntArg().parse(name, *value);
}

String
NameInfo::revquery(uint32_t type, const Element *e, const void *value, size_t vsize)
{
    while (1) {
	NameDB *db = getdb(type, e, vsize, false);
	while (db) {
	    if (String s = db->revquery(value, vsize))
		return s;
	    db = db->context_parent();
	}
	if (!e)
	    return String();
	e = 0;
    }
}


#if CLICK_NAMEDB_CHECK
void
NameInfo::check(ErrorHandler *errh)
{
    StringAccum sa;
    sa << "NameInfo[" << (void*) this << "]: ";
    PrefixErrorHandler perrh(errh, sa.take_string());
    _check_generation++;
    for (int i = 0; i < _namedb_roots.size(); i++) {
	NameDB *db = _namedb_roots[i];
	if (i < _namedb_roots.size() - 1
	    && db->type() >= _namedb_roots[i+1]->type())
	    perrh.error("db roots out of order at %i (%x/%x)", i, (unsigned) db->type(), (unsigned) _namedb_roots[i+1]->type());
	checkdb(db, 0, &perrh);
    }
    for (int i = 0; i < _namedbs.size(); i++)
	if (_namedbs[i]->_check_generation != _check_generation)
	    perrh.error("DB[%x %s %p] in namedbs, but inaccessible", _namedbs[i]->_type, _namedbs[i]->_context.c_str(), _namedbs[i]);
}

void
NameInfo::checkdb(NameDB *db, NameDB *parent, ErrorHandler *errh)
{
    StringAccum sa;
    sa.snprintf(20, "DB[%x ", db->_type);
    if (db->_context)
	sa << db->_context << ' ';
    sa << (void*) db << "]: ";
    PrefixErrorHandler perrh(errh, sa.take_string());

    // check self
    if (!db->_installed)
	perrh.error("not installed");
    else if (db->_installed != this)
	perrh.error("installed in %p, not this NameInfo", db->_installed);
    if (db->_check_generation == _check_generation)
	perrh.error("installed in more than one place");
    db->_check_generation = _check_generation;
    for (int i = 0; i < _namedbs.size(); i++)
	if (_namedbs[i] == db)
	    goto found_in_namedbs;
    perrh.error("not in _namedbs");
  found_in_namedbs:

    // check parent relationships
    if (db->_context_parent != parent)
	perrh.error("bad parent (%p/%p)", db->_context_parent, parent);
    else if (parent && (db->_context.length() < parent->_context.length()
			|| db->_context.substring(0, parent->_context.length()) != parent->_context))
	perrh.error("parent prefix (%s) disagrees with prefix", parent->_context.c_str());
    if (db->_context && db->_context.back() != '/')
	perrh.error("prefix doesn't end with '/'");
    if (parent && parent->_type != db->_type)
	perrh.error("parent DB[%x %s %p] has different type", parent->_type, parent->_context.c_str(), parent);
    if (parent && parent->_value_size != db->_value_size)
	perrh.error("parent DB[%x %s %p] has different value size (%u/%u)", parent->_type, parent->_context.c_str(), parent, parent->_value_size, db->_value_size);

    // check sibling relationships
    for (NameDB* sib = db->_context_sibling; sib; sib = sib->_context_sibling) {
	int l1 = db->_context.length(), l2 = sib->_context.length();
	if (l1 < l2 ? sib->_context.substring(0, l1) == db->_context
	    : db->_context.substring(0, l2) == sib->_context)
	    perrh.error("sibling DB[%x %s %p] should have parent/child relationship", sib->_type, sib->_context.c_str(), sib);
	if (sib->_type != db->_type)
	    perrh.error("sibling DB[%x %s %p] has different type", sib->_type, sib->_context.c_str(), sib);
	if (sib->_value_size != db->_value_size)
	    perrh.error("sibling DB[%x %s %p] has different value size (%u/%u)", sib->_type, sib->_context.c_str(), sib, sib->_value_size, db->_value_size);
    }

    // check db itself
    db->check(&perrh);

    // recurse down and to the side
    perrh.message("OK");
    if (db->_context_child) {
	PrefixErrorHandler perrh2(errh, "  ");
	checkdb(db->_context_child, db, &perrh2);
    }
    if (db->_context_sibling)
	checkdb(db->_context_sibling, parent, errh);
}

void
NameDB::check(ErrorHandler *)
{
}

void
StaticNameDB::check(ErrorHandler *errh)
{
    for (size_t i = 0; i < _nentries - 1; i++)
	if (strcmp(_entries[i].name, _entries[i+1].name) >= 0)
	    errh->error("entries %d/%d (%s/%s) out of order", i, i+1, _entries[i].name, _entries[i+1].name);
}

void
DynamicNameDB::check(ErrorHandler *errh)
{
    if (_sorted == 100)
	for (int i = 0; i < _names.size() - 1; i++)
	    if (String::compare(_names[i], _names[i+1]) >=0)
		errh->error("entries %d/%d (%s/%s) out of order", i, i+1, _names[i].c_str(), _names[i+1].c_str());
    if ((size_t) _values.length() != _names.size() * value_size())
	errh->error("odd value length %d (should be %d)", _values.length(), _names.size() * value_size());
}

void
NameInfo::check(const Element *e, ErrorHandler *errh)
{
    if (e)
	if (NameInfo *ni = e->router()->name_info())
	    ni->check(errh);
    the_name_info->check(errh);
}
#endif

CLICK_ENDDECLS
