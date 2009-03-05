// -*- related-file-name: "../include/click/hashallocator.hh" -*-
/*
 * hashallocator.cc -- simple allocator for HashTable
 * Eddie Kohler
 *
 * Copyright (c) 2008 Meraki, Inc.
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
#include <click/glue.hh>
#include <click/hashallocator.hh>
#include <click/integers.hh>
CLICK_DECLS

HashAllocator::HashAllocator(size_t size)
    : _free(0), _buffer(0), _size(size)
{
#ifdef VALGRIND_CREATE_MEMPOOL
    VALGRIND_CREATE_MEMPOOL(this, 0, 0);
#endif
}

HashAllocator::~HashAllocator()
{
    while (buffer *b = _buffer) {
	_buffer = b->next;
	delete[] reinterpret_cast<char *>(b);
    }
#ifdef VALGRIND_DESTROY_MEMPOOL
    VALGRIND_DESTROY_MEMPOOL(this);
#endif
}

void *HashAllocator::hard_allocate()
{
    size_t nelements;

    if (!_buffer)
	nelements = (min_buffer_size - sizeof(buffer)) / _size;
    else {
	size_t shift = sizeof(size_t) * 8 - ffs_msb(_buffer->maxpos + _size);
	size_t new_size = 1 << (shift + 1);
	if (new_size > max_buffer_size)
	    new_size = max_buffer_size;
	nelements = (new_size - sizeof(buffer)) / _size;
    }
    if (nelements < min_nelements)
	nelements = min_nelements;

    buffer *b = reinterpret_cast<buffer *>(new char[sizeof(buffer) + _size * nelements]);
    if (b) {
	b->next = _buffer;
	_buffer = b;
	b->maxpos = sizeof(buffer) + _size * nelements;
	b->pos = sizeof(buffer) + _size;
	void *data = reinterpret_cast<char *>(_buffer) + sizeof(buffer);
#ifdef VALGRIND_MEMPOOL_ALLOC
	VALGRIND_MEMPOOL_ALLOC(this, data, _size);
#endif
	return data;
    } else
	return 0;
}

void HashAllocator::swap(HashAllocator &x)
{
    size_t xsize = _size;
    _size = x._size;
    x._size = xsize;

    link *xfree = _free;
    _free = x._free;
    x._free = xfree;

    buffer *xbuffer = _buffer;
    _buffer = x._buffer;
    x._buffer = xbuffer;

#ifdef VALGRIND_MOVE_MEMPOOL
    VALGRIND_MOVE_MEMPOOL(this, reinterpret_cast<HashAllocator *>(100));
    VALGRIND_MOVE_MEMPOOL(&x, this);
    VALGRIND_MOVE_MEMPOOL(reinterpret_cast<HashAllocator *>(100), &x);
#endif
}

CLICK_ENDDECLS
