#ifndef CLICK_HASHALLOCATOR_HH
#define CLICK_HASHALLOCATOR_HH
#if HAVE_VALGRIND && HAVE_VALGRIND_MEMCHECK_H
# include <valgrind/memcheck.h>
#endif
CLICK_DECLS

template <size_t size>
class HashAllocator { public:

    HashAllocator();
    ~HashAllocator();

    inline void *allocate();
    inline void deallocate(void *p);

    void swap(HashAllocator<size> &o);

  private:

    struct link {
	link *next;
    };

    struct buffer {
	buffer *next;
	size_t pos;
	size_t maxpos;
    };

    enum {
	buffer_elements = 127
    };

    link *_free;
    buffer *_buffer;

    void *hard_allocate();

};

template <size_t size>
HashAllocator<size>::HashAllocator()
    : _free(0), _buffer(0)
{
    // static assert that size >= sizeof(link)
    switch (size >= sizeof(link)) case 0: case size >= sizeof(link): ;
#ifdef VALGRIND_CREATE_MEMPOOL
    VALGRIND_CREATE_MEMPOOL(this, 0, 0);
#endif
}

template <size_t size>
HashAllocator<size>::~HashAllocator()
{
    while (buffer *b = _buffer) {
	_buffer = b->next;
	delete[] reinterpret_cast<char *>(b);
    }
#ifdef VALGRIND_DESTROY_MEMPOOL
    VALGRIND_DESTROY_MEMPOOL(this);
#endif
}

template <size_t size>
inline void *HashAllocator<size>::allocate()
{
    if (link *l = _free) {
#ifdef VALGRIND_MEMPOOL_ALLOC
	VALGRIND_MEMPOOL_ALLOC(this, l, size);
	VALGRIND_MAKE_MEM_DEFINED(&l->next, sizeof(l->next));
#endif
	_free = l->next;
#ifdef VALGRIND_MAKE_MEM_DEFINED
	VALGRIND_MAKE_MEM_UNDEFINED(&l->next, sizeof(l->next));
#endif
	return l;
    } else if (_buffer && _buffer->pos < _buffer->maxpos) {
	void *data = reinterpret_cast<char *>(_buffer) + _buffer->pos;
	_buffer->pos += size;
#ifdef VALGRIND_MEMPOOL_ALLOC
	VALGRIND_MEMPOOL_ALLOC(this, data, size);
#endif
	return data;
    } else
	return hard_allocate();
}

template <size_t size>
inline void HashAllocator<size>::deallocate(void *p)
{
    if (p) {
	reinterpret_cast<link *>(p)->next = _free;
	_free = reinterpret_cast<link *>(p);
#ifdef VALGRIND_MEMPOOL_FREE
	VALGRIND_MEMPOOL_FREE(this, p);
#endif
    }
}

template <size_t size>
void *HashAllocator<size>::hard_allocate()
{
    size_t nelements = (_buffer ? buffer_elements >> 1 : buffer_elements);
    buffer *b = reinterpret_cast<buffer *>(new char[sizeof(buffer) + size * nelements]);
    if (b) {
	b->next = _buffer;
	_buffer = b;
	b->pos = sizeof(buffer) + size;
	b->maxpos = sizeof(buffer) + size * nelements;
	void *data = reinterpret_cast<char *>(_buffer) + sizeof(buffer);
#ifdef VALGRIND_MEMPOOL_ALLOC
	VALGRIND_MEMPOOL_ALLOC(this, data, size);
#endif
	return data;
    } else
	return 0;
}

template <size_t size>
void HashAllocator<size>::swap(HashAllocator<size> &x)
{
    link *xfree = _free;
    _free = x._free;
    x._free = xfree;

    buffer *xbuffer = _buffer;
    _buffer = x._buffer;
    x._buffer = xbuffer;

#ifdef VALGRIND_MOVE_MEMPOOL
    VALGRIND_MOVE_MEMPOOL(this, reinterpret_cast<HashAllocator<size> *>(100));
    VALGRIND_MOVE_MEMPOOL(&x, this);
    VALGRIND_MOVE_MEMPOOL(reinterpret_cast<HashAllocator<size> *>(100), &x);
#endif
}

CLICK_ENDDECLS
#endif
