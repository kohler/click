// -*- c-basic-offset: 4; related-file-name: "../include/click/ino.hh" -*-
/*
 * ino.{cc,hh} -- inode numbers for Click file systems
 * Eddie Kohler
 *
 * Copyright (c) 2002 International Computer Science Institute
 * Copyright (c) 2008 Meraki, Inc.
 *
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files (the "Software")
 * to deal in the Software without restriction, subject to the conditions
 * listed in the Click LICENSE file. These conditions include: you must
 * preserve this copyright notice, and you cannot mention the copyright
 * holders in advertising related to the Software without their permission.
 * The Software is provided WITHOUT ANY WARRANTY, EXPRESS OR IMPLIED. This
 * notice is a summary of the Click LICENSE file; the license in that file is
 * legally binding.
 */

#include <click/config.h>
#include <click/glue.hh>
#include <click/ino.hh>
#include <click/router.hh>
#if INO_DEBUG
# include <click/straccum.hh>
#endif
#if CLICK_LINUXMODULE
# include <click/cxxprotect.h>
CLICK_CXX_PROTECT
# include <linux/fs.h>
CLICK_CXX_UNPROTECT
# include <click/cxxunprotect.h>
#endif
CLICK_DECLS

void
ClickIno::initialize()
{
    _x = 0;
    _nentries = _cap = 0;
    _router = 0;
    _generation = 0;
}

void
ClickIno::cleanup()
{
    for (int i = 0; i < _cap; i++)
	_x[i].name.~String();
    CLICK_LFREE(_x, sizeof(Entry) * _cap);
    initialize();
}

int
ClickIno::grow(int min_size)
{
    if (_cap >= min_size)
	return 0;
    int new_cap = (_cap ? _cap : 128);
    while (new_cap < min_size)
	new_cap *= 2;
    // cheat on memory: bad me!
    Entry *nse = (Entry *)CLICK_LALLOC(sizeof(Entry) * new_cap);
    if (!nse)
	return -ENOMEM;
    memcpy(nse, _x, sizeof(Entry) * _cap);
    for (int i = _cap; i < new_cap; i++)
	new((void *)&(nse[i].name)) String();
    CLICK_LFREE(_x, sizeof(Entry) * _cap);
    _x = nse;
    _cap = new_cap;
    return 0;
}


static int
entry_compar(const void *v1, const void *v2, void *)
{
    const ClickIno::Entry *a = reinterpret_cast<const ClickIno::Entry *>(v1);
    const ClickIno::Entry *b = reinterpret_cast<const ClickIno::Entry *>(v2);
    return String::compare(a->name, b->name);
}

int
ClickIno::true_prepare(Router *r, uint32_t generation)
{
    // config lock must be held!

    int nelem = (r ? r->nelements() : 0) + 1;
    // Save some memory when replacing a large config with a small one
    if (nelem < _nentries / 2 && _nentries > 256)
	cleanup();
    if (grow(nelem) < 0)
	return -ENOMEM;

    // add sentinel entry
    _x[0].name = String();
    _x[0].elementno_plus1 = 0;
    _x[0].xindex = 0;
    _x[0].skip = 0;
    _x[0].flags = 0;
    _nentries = 1;

    // exit early if router is empty
    if (nelem == 1) {
	_generation = generation;
	_router = r;
	_x[0].flags = 0;
	return 0;
    }

    // add entries for actual elements
    for (int i = 1; i < nelem; i++) {
	_x[i].name = r->ename(i - 1);
	_x[i].elementno_plus1 = i;
	_x[i].skip = 0;
	_x[i].flags = 0;
    }

    // sort _x
    click_qsort(&_x[1], nelem - 1, sizeof(Entry), entry_compar);

    // add new _x entries for intermediate directories
    int n = nelem;
    for (int i = 1; i < nelem; i++) {
	// make local copies of the names since we might resize _x
	String name = _x[i].name;
	String last_name = _x[i-1].name;
	int slash = name.find_right('/');
	// rely on '/' being less than any other valid name character in
	// ASCII. If the prefix of "last_name" matches the part of "name"
	// before a slash, then we know that "last_name" matches the whole
	// component (rather than a fake match like "a/b/cc" <-> "a/b/c/e").
	while (slash >= 0 && (last_name.length() < slash || memcmp(name.data(), last_name.data(), slash) != 0)) {
	    if (n >= _cap && grow(n + 1) < 0)
		return -ENOMEM;
	    _x[n].name = name.substring(0, slash);
	    _x[n].elementno_plus1 = n;
	    _x[n].skip = 0;
	    _x[n].flags = X_FAKE;
	    slash = _x[n].name.find_right('/');
	    n++;
	}
    }

    // resort _x if necessary
    if (n != nelem)
	click_qsort(&_x[1], n - 1, sizeof(Entry), entry_compar);

    // calculate 'skip'
    _x[0].skip = n - 1;
    for (int i = 1; i < n - 1; i++) {
	const String &name = _x[i].name;
	int length = name.length();
	int j = i + 1;
	while (j < n && _x[j].name.length() > length
	       && _x[j].name[length] == '/') {
	    assert(_x[j].name.substring(0, length) == name);
	    _x[i].skip++;
	    j++;
	}
    }

    // calculate 'xindex'
    for (int i = 1; i < n; i++)
	_x[_x[i].elementno_plus1].xindex = i;

    // done
    _nentries = n;
    _generation = generation;
    _router = r;
    return 0;
}


int
ClickIno::name_search(const String &n, int first_xi, int last_xi, int name_offset) const
{
    while (first_xi <= last_xi) {
	int mid = first_xi + ((last_xi - first_xi) >> 1);
	int cmp = String::compare(n, _x[mid].name.substring(name_offset));
	if (cmp == 0)
	    return mid;
	else if (cmp < 0)
	    last_xi = mid - 1;
	else
	    first_xi = mid + 1;
    }
    return -1;
}

int
ClickIno::element_name_search(const String &n, int elementno) const
{
    int xi = xindex(elementno);
    int firstpos = _x[xi].name ? _x[xi].name.length() + 1 : 0;
    return name_search(n, xi + 1, next_xindex(elementno) - 1, firstpos);
}

int
ClickIno::nlink(ino_t ino)
{
    // must be called with config_lock held
    int elementno = ino_element(ino);

    // it might be a handler
    if (is_handler(ino)) {
	// Number of links per handler: one for the .h directory; plus
	// if element, one for the .e/EINDEX directory; plus
	// if no conflict, one for the global or element directory.
	int handlerno = ino_handler(ino);
	const Handler *h = Router::handler(_router, handlerno);
	return 1 + (elementno >= 0 ? 1 : 0)
	    + (element_name_search(h->name(), elementno) < 0 ? 1 : 0);
    }

    // otherwise, it is a directory
    int nlink = 2;
    if (ino == ino_enumdir && _router)
	nlink += _router->nelements();
    if (has_names(ino)) {
	int xi = xindex(elementno) + 1;
	int next_xi = next_xindex(elementno);
	if (has_handlers(ino) && !(_x[xi - 1].flags & X_FAKE))
	    nlink++;		// for ".h" subdirectory
	while (xi < next_xi) {
	    nlink++;
	    xi += _x[xi].skip + 1;
	}
    }
    if (ino == ino_globaldir)
	nlink++;		// for ".e" subdirectory
    return nlink;
}

ino_t
ClickIno::lookup(ino_t ino, const String &component)
{
    // BEWARE: "component" might be a fake String pointing to mutable data; do
    // not save any references to it!

    // must be called with config_lock held
    int elementno = ino_element(ino);
    int nelements = (_router ? _router->nelements() : 0);

    // quit early on empty string
    if (!component.length())
	return 0;

    // quick check for dot
    if (component[0] == '.' && component.length() == 1)
	return ino;

    // look for numbers
    if (ino == ino_enumdir && component[0] >= '0' && component[0] <= '9') {
	int eindex = component[0] - '0';
	for (int i = 1; i < component.length(); i++)
	    if (component[i] >= '0' && component[i] <= '9' && eindex < 1000000000)
		eindex = (eindex * 10) + component[i] - '0';
	    else
		goto number_failed;
	if (!_router || eindex >= _router->nelements())
	    goto number_failed;
	return make_dir(dt_hu, eindex);
    }

  number_failed:
    // look for element number directory
    if (ino == ino_globaldir && component.equals(".e", 2))
	return ino_enumdir;

    // look for handler directory
    if (has_handlers(ino) && has_names(ino) && component.equals(".h", 2)
	&& !(_x[xindex(elementno)].flags & X_FAKE))
	return make_dir(dt_hh, elementno);

    // look for names
    if (has_names(ino)) {
	// delimit boundaries of search region
	int found = element_name_search(component, elementno);
	if (found >= 0)
	    return make_dir(dt_hn, ClickIno::elementno(found));
    }

    // look for handlers (no initial period)
    if (has_handlers(ino) && elementno < nelements && component[0] != '.') {
	Element *element = Router::element(_router, elementno);
	int hi = Router::hindex(element, component);
	if (hi >= 0)
	    if (Router::handler(_router, hi)->visible())
		return make_handler(elementno, hi);
    }

    // check for dot dot
    if (component.equals("..", 2)) {
	if (ino == ino_enumdir || ino == make_dir(dt_hh, -1))
	    return ino_globaldir;
	else if (dirtype(ino) == dt_hu)
	    return ino_enumdir;
	else if (dirtype(ino) == dt_hh)
	    return make_dir(dt_hn, elementno);
	else {
	    int xi = xindex(elementno);
	    int slash = _x[xi].name.find_right('/');
	    if (slash < 0)
		return ino_globaldir;
	    int found = name_search(_x[xi].name.substring(0, slash), 1, _nentries - 1, 0);
	    assert(found >= 0);
	    return make_dir(dt_hn, ClickIno::elementno(found));
	}
    }

    // no luck
    return 0;
}

static bool
check_handler_name(const String &hname)
{
    const char *s = hname.begin(), *end = hname.end();
    if (s == end || *s == '.')
	return false;
    for (; s != end; ++s)
	if (*s == '/' || *s == 0)
	    return false;
    return true;
}

int
ClickIno::readdir(ino_t ino, uint32_t &f_pos, filldir_t filldir, void *thunk)
{
    // File positions:
    // 0x000000           ..
    // 0x000001           .
    // 0x000002-0x0FFFFF  ignored
    // 0x100000-0x1FFFFF  handlers
    // 0x200000-0x2FFFFF  numbers
    // 0x300000-0x3FFFFF  names

#define RD_HOFF		0x100000
#define RD_UOFF		0x200000
#define RD_NOFF		0x300000
#define RD_XOFF		0x400000
#define FILLDIR(a, b, c, d, e, f)  do { if (!filldir(a, b, c, d, e, f)) return stored; else stored++; } while (0)

    int elementno = ino_element(ino);
    int nelements = (_router ? _router->nelements() : 0);
    int stored = 0;

    // ".." and "."
    if (f_pos == 0) {
	if (ino_t dotdot = lookup(ino, String::make_stable("..", 2)))
	    FILLDIR("..", 2, dotdot, DT_DIR, f_pos, thunk);
	f_pos++;
    }
    if (f_pos == 1) {
	FILLDIR(".", 1, ino, DT_DIR, f_pos, thunk);
	f_pos++;
    }

    // handler names
    if (f_pos < RD_HOFF)
	f_pos = RD_HOFF;
    if (f_pos < RD_UOFF && has_handlers(ino) && elementno < nelements) {
	Element *element = Router::element(_router, elementno);
	Vector<int> his;
	Router::element_hindexes(element, his);
	while (f_pos >= RD_HOFF && f_pos < (uint32_t) his.size() + RD_HOFF) {
	    // Traverse element_hindexes in reverse because new handler
	    // names are added at the end.
	    int hi = his[his.size() - (f_pos - RD_HOFF) - 1];
	    const Handler* h = Router::handler(_router, hi);
	    if (!has_names(ino) || element_name_search(h->name(), elementno) < 0) {
		if (h->visible() && check_handler_name(h->name()))
		    FILLDIR(h->name().data(), h->name().length(), make_handler(elementno, hi), DT_REG, f_pos, thunk);
	    }
	    f_pos++;
	}
    }

    // subdirectory numbers
    if (f_pos < RD_UOFF)
	f_pos = RD_UOFF;
    if (f_pos < RD_NOFF && ino == ino_enumdir && _router) {
	char buf[10];
	int nelem = _router->nelements();
	while (f_pos >= RD_UOFF && f_pos < (uint32_t) RD_UOFF + nelem) {
	    int elem = f_pos - RD_UOFF;
	    sprintf(buf, "%d", elem);
	    FILLDIR(buf, strlen(buf), make_dir(dt_hu, elem), DT_DIR, f_pos, thunk);
	    f_pos++;
	}
    }

    // figure out edges of directory
    int xi = xindex(elementno) + 1;
    int next_xi = next_xindex(elementno);

    // subdirectory names
    if (f_pos < RD_NOFF)
	f_pos = RD_NOFF;
    if (f_pos < RD_XOFF && has_names(ino)) {
	int name_offset = _x[xi - 1].name.length();
	if (name_offset > 0)
	    name_offset++;	// skip slash
	for (uint32_t j = RD_NOFF; xi < next_xi; xi += _x[xi].skip + 1, j++)
	    if (f_pos == j) {
		FILLDIR(_x[xi].name.data() + name_offset, _x[xi].name.length() - name_offset, make_dir(dt_hn, ClickIno::elementno(xi)), DT_DIR, f_pos, thunk);
		f_pos++;
	    }
    }

    // ".e" in global directory
    if (f_pos < RD_XOFF)
	f_pos = RD_XOFF;
    if (f_pos == RD_XOFF && ino == ino_globaldir) {
	FILLDIR(".e", 2, ino_enumdir, DT_DIR, f_pos, thunk);
	f_pos++;
    }
    if (f_pos <= RD_XOFF + 1 && has_names(ino)
	&& !(_x[xindex(elementno)].flags & X_FAKE)) {
	FILLDIR(".h", 2, make_dir(dt_hh, elementno), DT_DIR, f_pos, thunk);
	f_pos++;
    }

    f_pos = RD_XOFF + 2;
    return stored;
}

#if INO_DEBUG
String
ClickIno::info() const
{
    StringAccum sa;
    for (int i = 0; i < _nentries; i++) {
	sa << i << ". " << _x[i].name;
	if (_x[i].name.length() >= 40)
	    sa << ' ';
	else
	    sa.append("                                                                                                                                       ", 40 - _x[i].name.length());
	sa << 'E' << (_x[i].elementno_plus1 - 1) << '/'
	   << 'X' << _x[i].xindex << '\t'
	   << "->" << (i + 1 + _x[i].skip);
	if (_x[i].flags) {
	    sa << '\t';
	    if (_x[i].flags & X_FAKE)
		sa << 'F';
	}
	sa << '\n';
    }
    return sa.take_string();
}
#endif

CLICK_ENDDECLS
