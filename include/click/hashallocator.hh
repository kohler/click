#ifndef CLICK_HASHALLOCATOR_HH
#define CLICK_HASHALLOCATOR_HH
CLICK_DECLS

template <size_t size>
class hashallocator { public:

    hashallocator();
    ~hashallocator();

    inline void *allocate();
    inline void deallocate(void *p);

    void swap(hashallocator<size> &o);

  private:

    struct link {
	link *next;
    };

    struct buffer {
	buffer *next;
	size_t pos;
    };

    enum {
	buffer_elements = 127
    };

    link *_free;
    buffer *_buffer;

    void *hard_allocate();
    
};

template <size_t size>
hashallocator<size>::hashallocator()
    : _free(0), _buffer(0)
{
    // static assert that size >= sizeof(link)
    switch (size >= sizeof(link)) case 0: case size >= sizeof(link): ;
}

template <size_t size>
hashallocator<size>::~hashallocator()
{
    while (buffer *b = _buffer) {
	_buffer = b->next;
	delete[] reinterpret_cast<char *>(b);
    }
}

template <size_t size>
inline void *hashallocator<size>::allocate()
{
    if (link *l = _free) {
	_free = _free->next;
	return l;
    } else if (_buffer && _buffer->pos > sizeof(buffer)) {
	_buffer->pos -= size;
	return reinterpret_cast<char *>(_buffer) + _buffer->pos;
    } else
	return hard_allocate();
}

template <size_t size>
inline void hashallocator<size>::deallocate(void *p)
{
    reinterpret_cast<link *>(p)->next = _free;
    _free = reinterpret_cast<link *>(p);
}

template <size_t size>
void *hashallocator<size>::hard_allocate()
{
    size_t nelements = (_buffer ? buffer_elements >> 1 : buffer_elements);
    buffer *b = reinterpret_cast<buffer *>(new char[sizeof(buffer) + size * nelements]);
    if (b) {
	b->next = _buffer;
	_buffer = b;
	b->pos = sizeof(buffer) + size * (nelements - 1);
	return reinterpret_cast<char *>(b) + b->pos;
    } else
	return 0;
}

template <size_t size>
void hashallocator<size>::swap(hashallocator<size> &o)
{
    link *ofree = _free;
    _free = o._free;
    o._free = ofree;

    buffer *obuffer = _buffer;
    _buffer = o._buffer;
    o._buffer = obuffer;
}

CLICK_ENDDECLS
#endif
