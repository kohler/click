// -*- c-basic-offset: 4; related-file-name: "../../lib/ino.cc" -*-
#ifndef CLICK_INO_HH
#define CLICK_INO_HH
#include <click/router.hh>
CLICK_DECLS

#define INO_DEBUG			0

// /click: .e > .h > element names > global handlers [dt_global]
// /click/.e: element numbers [dt_u]
// /click/.e/NUM: handlers [dt_hu]
// /click/.h: global handlers [dt_hh]
// /click/ELEMENTNAME: .h > element names > element handlers [dt_hn]
// /click/ELEMENTNAME/.h: element handlers [dt_hh]

class ClickIno { public:

    void initialize();
    void cleanup();

    uint32_t generation() const		{ return _generation; }

    // NB: inode number 0 is reserved for the system.
    enum { dt_u = 1U, dt_hh = 2U, dt_hu = 3U, dt_hn = 4U, dt_global = 5U };
    enum { ino_globaldir = (unsigned) (dt_global << 28),
	   ino_enumdir = (unsigned) (dt_u << 28) };
    static bool is_handler(unsigned ino)   { return (int32_t) ino < 0; }
    static unsigned dirtype(unsigned ino)  { return ino >> 28; }
    static bool has_element(unsigned ino)  { return ino & 0xFFFFU; }
    static bool has_handlers(unsigned ino) { return ino > (dt_u << 28); }
    static bool has_names(unsigned ino)    { return ino >= (dt_hn << 28); }
    static int ino_element(unsigned ino)   { return (int) (ino & 0xFFFFU) - 1; }
    static int ino_handler(unsigned ino) {
	return (has_element(ino) ? 0 : Router::FIRST_GLOBAL_HANDLER)
	    + ((ino >> 16) & 0x7FFFU);
    }

    static unsigned make_handler(int e, int hi) {
	return 0x80000000U | ((hi & 0x7FFFU) << 16) | ((e + 1) & 0xFFFFU);
    }
    static unsigned make_dir(unsigned dtype, int e) {
	return (dtype << 28) | ((e + 1) & 0xFFFFU);
    }


    // These operations should be called with a configuration lock held.
    inline int prepare(Router *router, uint32_t generation);
    int nlink(ino_t ino);
    ino_t lookup(ino_t dir, const String &component);

    // readdir handles '..' (f_pos 0) and '.' (f_pos 1).
    // It returns the number of things stored.
    typedef bool (*filldir_t)(const char *name, int name_len, ino_t ino, int dirtype, uint32_t f_pos, void *user_data);
    int readdir(ino_t dir, uint32_t &f_pos, filldir_t fd, void *user_data);

#if INO_DEBUG
    String info() const;
#endif

    struct Entry {
	// Name of this entry.
	String name;

	// Corresponding eindex plus 1. Might be larger than the number of
	// elements in the router, because of fake directories added for
	// compound "elements".
	uint16_t elementno_plus1;

	// '_x[i].xindex' equals the index in _x of the entry for element
	// number 'i - 1'.
	uint16_t xindex;

	// Number of child entries. 'name' is guaranteed to be a prefix of
	// every child entry.
	uint16_t skip;

	// See enum below. X_FAKE is true on fake directories added for
	// compound elements.
	uint16_t flags;
    };

  private:
    enum { X_FAKE = 1 };

    Entry* _x;
    int _nentries;
    int _cap;
    Router* _router;
    uint32_t _generation;

    inline int xindex(int elementno) const;
    inline int next_xindex(int elementno) const;
    inline int elementno(int xindex) const;

    int name_search(const String &n, int first_xi, int last_xi, int name_offset) const;
    int element_name_search(const String &n, int elementno) const;

    int grow(int min_size);
    int true_prepare(Router*, uint32_t);
};


inline int
ClickIno::prepare(Router* router, uint32_t generation)
{
    if (generation != _generation)
	return true_prepare(router, generation);
    else
	return 0;
}

inline int
ClickIno::xindex(int elementno) const
{
    assert(elementno >= -1 && elementno < _nentries - 1);
    return _x[elementno + 1].xindex;
}

inline int
ClickIno::next_xindex(int elementno) const
{
    int xi = xindex(elementno);
    return xi + _x[xi].skip + 1;
}

inline int
ClickIno::elementno(int xindex) const
{
    return _x[xindex].elementno_plus1 - 1;
}

CLICK_ENDDECLS
#endif
