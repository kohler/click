// -*- c-basic-offset: 4; related-file-name: "../include/click/ino.hh" -*-
/*
 * ino.{cc,hh} -- inode numbers for Click file systems
 * Eddie Kohler
 *
 * Copyright (c) 2002 International Computer Science Institute
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
    delete[] ((uint8_t *)_x);
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
    Entry *nse = (Entry *)(new uint8_t[sizeof(Entry) * new_cap]);
    if (!nse)
	return -ENOMEM;
    memcpy(nse, _x, sizeof(Entry) * _cap);
    for (int i = _cap; i < new_cap; i++)
	new((void *)&nse[i]) String();
    delete[] ((uint8_t *)_x);
    _x = nse;
    _cap = new_cap;
    return 0;
}


extern "C" {
static int
entry_compar(const void *v1, const void *v2)
{
    const ClickIno::Entry *a = reinterpret_cast<const ClickIno::Entry *>(v1);
    const ClickIno::Entry *b = reinterpret_cast<const ClickIno::Entry *>(v2);
    return String::compare(a->name, b->name);
}
}

int
ClickIno::true_prepare(Router *r, uint32_t generation)
{
    // config lock must be held!

    int nelem = (r ? r->nelements() : 0) + 1;
    if (grow(nelem) < 0)
	return -ENOMEM;

    // add sentinel entry
    _x[0].name = String();
    _x[0].elementno_plus1 = 0;
    _x[0].xindex = 0;
    _x[0].skip = 0;
    _x[0].flags = X_FAKE;
    _nentries = 1;

    // exit early if router is empty
    if (nelem == 1) {
	_generation = generation;
	_router = r;
	return 0;
    }

    // add entries for actual elements
    for (int i = 1; i < nelem; i++) {
	_x[i].name = r->ename(i - 1);
	_x[i].elementno_plus1 = i;
	_x[i].skip = 0;
	_x[i].flags = 0;
    }

    // sort sorted_elements
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
 
    // resort sorted_elements if necessary
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


extern "C" {
static int
string_compar(const void *v1, const void *v2)
{
    const String *a = reinterpret_cast<const String *>(v1);
    const String *b = reinterpret_cast<const String *>(v2);
    return String::compare(*a, *b);
}
}

void
ClickIno::calculate_handler_conflicts(int parent_elementno)
{
    // configuration lock must be held!
    
    // no conflicts if no router, no children, or this element is fake
    assert(parent_elementno >= -1 && parent_elementno < _nentries - 1);
    int parent_xindex = xindex(parent_elementno);
    if ((_x[parent_xindex].flags & (X_FAKE | X_SUBDIR_CONFLICTS_CALCULATED))
	|| _x[parent_xindex].skip == 0) {
	_x[parent_xindex].flags |= X_SUBDIR_CONFLICTS_CALCULATED;
	return;
    }

    // find the relevant handler indexes and names
    Vector<int> hindexes;
    _router->element_handlers(parent_elementno, hindexes);
    Vector<String> names;
    for (int i = 0; i < hindexes.size(); i++) {
	const Router::Handler &h = _router->handler(hindexes[i]);
	if (h.visible())
	    names.push_back(h.name());
    }

    // sort names
    if (names.size())
	click_qsort(&names[0], names.size(), sizeof(String), string_compar);

    // run over the arrays, marking conflicts
    int xi = parent_xindex + 1;
    int next_xi = next_xindex(parent_elementno);
    int hi = 0;
    while (xi < next_xi && hi < names.size()) {
	int compare = String::compare(_x[xi].name, names[hi]);
	if (compare == 0) {	// there is a conflict
	    _x[xi].flags |= X_HANDLER_CONFLICT;
	    xi += _x[xi].skip + 1;
	    hi++;
	} else if (compare < 0)
	    xi += _x[xi].skip + 1;
	else
	    hi++;
    }

    // mark subdirectory as calculated
    _x[parent_xindex].flags |= X_SUBDIR_CONFLICTS_CALCULATED;
}


int
ClickIno::nlink(ino_t ino)
{
    // must be called with config_lock held
    int elementno = INO_ELEMENTNO(ino);
    int nlink = 2;
    if (INO_DIRTYPE(ino) != INO_DT_H) {
	if (INO_DT_HAS_U(ino) && _router)
	    nlink += _router->nelements();
	if (INO_DT_HAS_N(ino)) {
	    int xi = xindex(elementno) + 1;
	    if (!(_x[xi - 1].flags & X_SUBDIR_CONFLICTS_CALCULATED))
		calculate_handler_conflicts(elementno);
	    int next_xi = next_xindex(elementno);
	    while (xi < next_xi) {
		if (!(_x[xi].flags & X_HANDLER_CONFLICT) || INO_DIRTYPE(ino) == INO_DT_N)
		    nlink++;
		xi += _x[xi].skip + 1;
	    }
	}
    }
    return nlink;
}

int
ClickIno::name_search(const String &n, int first_xi, int last_xi, int name_offset) const
{
    while (first_xi <= last_xi) {
	int mid = (first_xi + last_xi) >> 1;
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

ino_t
ClickIno::lookup(ino_t ino, const String &component)
{
    // must be called with config_lock held
    int elementno = INO_ELEMENTNO(ino);

    // quit early on empty string
    if (!component.length())
	return 0;

    // quick check for dot
    if (component[0] == '.' && component.length() == 1)
	return ino;
    
    // look for numbers
    if (INO_DT_HAS_U(ino) && component[0] >= '1' && component[0] <= '9') {
	int eindex = component[0] - '0';
	for (int i = 1; i < component.length(); i++)
	    if (component[i] >= '0' && component[i] <= '9' && eindex < 1000000000)
		eindex = (eindex * 10) + component[i] - '0';
	    else
		goto number_failed;
	eindex--;
	if (!_router || eindex >= _router->nelements())
	    goto number_failed;
	return INO_MKHDIR(eindex);
    }
    
  number_failed:
    // look for handlers
    if (INO_DT_HAS_H(ino)) {
	int hi = Router::find_handler(_router, elementno, component);
	if (hi >= 0) {
	    const Router::Handler &h = Router::handler(_router, hi);
	    if (h.visible())
		return INO_MKHANDLER(elementno, hi);
	}
    }

    // look for names
    if (INO_DT_HAS_N(ino)) {
	// delimit boundaries of search region
	int first_xi = xindex(elementno) + 1;
	int last_xi = next_xindex(elementno) - 1;
	int name_offset = _x[first_xi - 1].name.length() + 1;
	int found = name_search(component, first_xi, last_xi, (name_offset > 1 ? name_offset : 0));
	if (found >= 0)
	    return INO_MKHNDIR(ClickIno::elementno(found));
    }

    // check for dot dot
    if (component[0] == '.' && component.length() == 2 && component[1] == '.') {
	int xi = xindex(elementno);
	int slash = _x[xi].name.find_right('/');
	if (slash < 0 || INO_DIRTYPE(ino) == INO_DT_H)
	    return INO_GLOBALDIR;
	int found = name_search(_x[xi].name.substring(0, slash - 1), 1, _nentries - 1, 0);
	if (found >= 0)
	    return INO_MKHNDIR(ClickIno::elementno(found));
	panic("clickfs: ..");	// should never happen
    }

    // no luck
    return 0;
}

int
ClickIno::readdir(ino_t ino, uint32_t &f_pos, filldir_t filldir, void *thunk)
{
    // File positions:
    // 0x00000-0x0FFFF  ignored
    // 0x10000-0x1FFFF  handlers
    // 0x20000-0x2FFFF  numbers
    // 0x30000-0x3FFFF	names

#define RD_HOFF		0x10000
#define RD_UOFF		0x20000
#define RD_NOFF		0x30000
#define RD_XOFF		0x40000
#define FILLDIR(a, b, c, d, e, f)  do { if (!filldir(a, b, c, d, e, f)) return 0; } while (0)
    
    int elementno = INO_ELEMENTNO(ino);

    // handler names
    if (f_pos < RD_HOFF)
	f_pos = RD_HOFF;
    if (f_pos < RD_UOFF && INO_DT_HAS_H(ino)) {
	Vector<int> hi;
	Router::element_handlers(_router, elementno, hi);
	while (f_pos >= RD_HOFF && f_pos < hi.size() + RD_HOFF) {
	    const Router::Handler &h = Router::handler(_router, hi[f_pos - RD_HOFF]);
	    if (h.visible())
		FILLDIR(h.name().data(), h.name().length(), INO_MKHANDLER(elementno, hi[f_pos - RD_HOFF]), DT_REG, f_pos, thunk);
	    f_pos++;
	}
    }

    // subdirectory numbers
    if (f_pos < RD_UOFF)
	f_pos = RD_UOFF;
    if (f_pos < RD_NOFF && INO_DT_HAS_U(ino) && _router) {
	char buf[10];
	int nelem = _router->nelements();
	while (f_pos >= RD_UOFF && f_pos < RD_UOFF + nelem) {
	    int elem = f_pos - RD_UOFF;
	    sprintf(buf, "%d", elem + 1);
	    FILLDIR(buf, strlen(buf), INO_MKHDIR(elem), DT_DIR, f_pos, thunk);
	    f_pos++;
	}
    }

    // figure out edges of directory
    int xi = xindex(elementno) + 1;
    int next_xi = next_xindex(elementno);

    // subdirectory names
    if (f_pos < RD_NOFF)
	f_pos = RD_NOFF;
    if (f_pos < RD_XOFF && INO_DT_HAS_N(ino)) {
	bool include_conflicts = (INO_DIRTYPE(ino) == INO_DT_N);
	if (!include_conflicts && !(_x[xi - 1].flags & X_SUBDIR_CONFLICTS_CALCULATED))
	    calculate_handler_conflicts(elementno);
	int name_offset = _x[xi - 1].name.length();
	if (name_offset > 0)
	    name_offset++;	// skip slash
	for (int j = RD_NOFF; xi < next_xi; xi += _x[xi].skip + 1, j++)
	    if (f_pos == j) {
		if (!(_x[xi].flags & X_HANDLER_CONFLICT) || include_conflicts)
		    FILLDIR(_x[xi].name.data() + name_offset, _x[xi].name.length() - name_offset, INO_MKHNDIR(ClickIno::elementno(xi)), DT_DIR, f_pos, thunk);
		f_pos++;
	    }
    }
    
    f_pos = RD_XOFF;
    return 1;
}

#if 0
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
	if (_x[i].flags & (~X_SUBDIR_CONFLICTS_CALCULATED)) {
	    sa << '\t';
	    if (_x[i].flags & X_FAKE)
		sa << 'X';
	    if (_x[i].flags & X_HANDLER_CONFLICT)
		sa << 'H';
	}
	sa << '\n';
    }
    return sa.take_string();
}
#endif
