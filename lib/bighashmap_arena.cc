// -*- c-basic-offset: 4; related-file-name: "../include/click/bighashmap_arena.hh" -*-
/*
 * bighashmap_arena.{cc,hh} -- memory allocation for hash tables, etc.
 * Eddie Kohler
 *
 * Copyright (c) 2000 Mazu Networks, Inc.
 * Copyright (c) 2002 International Computer Science Institute
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
#include <click/bighashmap_arena.hh>
#include <click/glue.hh>
CLICK_DECLS

HashMap_Arena::HashMap_Arena(uint32_t element_size)
    : _free(0),
      _cur_buffer(0), _buffer_pos(0),
      _element_size(element_size < sizeof(Link) ? sizeof(Link) : element_size),
      _buffers(new char *[8]), _nbuffers(0), _buffers_cap(8),
      _detached(false)
{
    _refcount = 0;
}

HashMap_Arena::~HashMap_Arena()
{
    for (int i = 0; i < _nbuffers; i++)
	delete[] _buffers[i];
    delete[] _buffers;
}

void *
HashMap_Arena::hard_alloc()
{
    assert(_buffer_pos == 0);

    if (_nbuffers == _buffers_cap) {
	char **new_buffers = new char *[_buffers_cap * 2];
	if (!new_buffers)
	    return 0;
	memcpy(new_buffers, _buffers, _buffers_cap * sizeof(char *));
	delete[] _buffers;
	_buffers = new_buffers;
	_buffers_cap *= 2;
    }

    char *new_buffer = new char[_element_size * NELEMENTS];
    if (!new_buffer)
	return 0;
    _buffers[_nbuffers] = _cur_buffer = new_buffer;
    _nbuffers++;
    _buffer_pos = _element_size * (NELEMENTS - 1);
    return _cur_buffer + _buffer_pos;
}

//

HashMap_ArenaFactory *HashMap_ArenaFactory::the_factory = 0;
static const uint32_t min_large = 256;
static const int shifts[2] = { 2, 7 };
static const int offsets[2] = { (1 << shifts[0]) - 1, (1 << shifts[1]) - 1 };

HashMap_ArenaFactory::HashMap_ArenaFactory()
{
    _arenas[0] = _arenas[1] = 0;
    _narenas[0] = _narenas[1] = 0;
}

HashMap_ArenaFactory::~HashMap_ArenaFactory()
{
    for (int which = 0; which < 2; which++) {
	for (int i = 0; i < _narenas[which]; i++)
	    if (_arenas[which][i]) {
		_arenas[which][i]->detach();
		_arenas[which][i]->unuse();
	    }
	delete[] _arenas[which];
    }
}

void
HashMap_ArenaFactory::static_initialize()
{
    if (!the_factory)
	the_factory = new HashMap_ArenaFactory;
}

void
HashMap_ArenaFactory::static_cleanup()
{
    delete the_factory;
    the_factory = 0;
}

HashMap_Arena *
HashMap_ArenaFactory::get_arena(uint32_t element_size, HashMap_ArenaFactory *factory)
{
    if (!the_factory)
	static_initialize();
    if (!factory)
	factory = the_factory;
    return factory->get_arena_func(element_size);
}

HashMap_Arena *
HashMap_ArenaFactory::get_arena_func(uint32_t element_size)
{
    int which = (element_size < min_large ? 0 : 1);
    int arenanum = (element_size + offsets[which]) >> shifts[which];

    int new_narenas = _narenas[which];
    while (new_narenas <= arenanum)
	new_narenas = (new_narenas ? new_narenas * 2 : 32);
    if (new_narenas != _narenas[which]) {
	if (HashMap_Arena **new_a = new HashMap_Arena *[new_narenas]) {
	    for (int i = 0; i < new_narenas; i++)
		new_a[i] = (i < _narenas[which] ? _arenas[which][i] : (HashMap_Arena *)0);
	    delete[] _arenas[which];
	    _arenas[which] = new_a;
	    _narenas[which] = new_narenas;
	} else
	    return 0;
    }

    if (!_arenas[which][arenanum]) {
	if (!(_arenas[which][arenanum] = new HashMap_Arena(arenanum << shifts[which])))
	    return 0;
	_arenas[which][arenanum]->use();
    }

    return _arenas[which][arenanum];
}

CLICK_ENDDECLS
