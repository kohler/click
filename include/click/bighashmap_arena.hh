// -*- c-basic-offset: 4; related-file-name: "../../lib/bighashmap_arena.cc" -*-
#ifndef CLICK_BIGHASHMAP_ARENA_HH
#define CLICK_BIGHASHMAP_ARENA_HH
CLICK_DECLS

class BigHashMap_Arena { public:

    BigHashMap_Arena(uint32_t element_size);
    ~BigHashMap_Arena();

    void *alloc();
    void free(void *);
    
  private:

    struct Link {
	Link *next;
    };
    Link *_free;

    enum { NELEMENTS = 127 };	// not a power of 2 so we don't fall into a
				// too-large bucket
    char *_cur_buffer;
    int _buffer_pos;
    
    uint32_t _element_size;

    char **_buffers;
    int _nbuffers;
    int _buffers_cap;
    
    void *hard_alloc();
    
};

class BigHashMap_ArenaFactory { public:

    BigHashMap_ArenaFactory();
    virtual ~BigHashMap_ArenaFactory();

    static void static_initialize();
    static void static_cleanup();
    
    static BigHashMap_Arena *get_arena(uint32_t, BigHashMap_ArenaFactory * =0);
    virtual BigHashMap_Arena *get_arena_func(uint32_t);
    
  private:

    BigHashMap_Arena **_arenas[2];
    int _narenas[2];

    static BigHashMap_ArenaFactory *the_factory;
    
};

inline void *
BigHashMap_Arena::alloc()
{
    if (_free) {
	void *ret = _free;
	_free = _free->next;
	return ret;
    } else if (_buffer_pos > 0) {
	_buffer_pos -= _element_size;
	return _cur_buffer + _buffer_pos;
    } else
	return hard_alloc();
}

inline void
BigHashMap_Arena::free(void *v)
{
    Link *link = reinterpret_cast<Link *>(v);
    link->next = _free;
    _free = link;
}

CLICK_ENDDECLS
#endif
