// -*- c-basic-offset: 4; related-file-name: "../../lib/ino.cc" -*-
#ifndef CLICK_INO_HH
#define CLICK_INO_HH
#include <click/string.hh>
CLICK_DECLS
class Router;

// NB: inode number 0 is reserved for the system.
#define INO_DIRTYPE(ino)		((int) ((unsigned) (ino) >> 28))
#define INO_ELEMENTNO(ino)		((int)((ino) & 0xFFFFU) - 1)
#define INO_HANDLERNO(ino)		((((ino) & 0xFFFFU) ? 0 : Router::FIRST_GLOBAL_HANDLER) + (((ino) >> 16) & 0x7FFFU))
#define INO_DT_U			0x1U /* .e directory */
#define INO_DT_HH			0x2U /* .h directory */
#define INO_DT_HU			0x3U /* .e/index directory */
#define INO_DT_HN			0x4U /* element name directory */
#define INO_DT_GLOBAL			0x5U /* global directory */
#define INO_DT_HAS_H(ino)		(INO_DIRTYPE((ino)) > (int) INO_DT_U)
#define INO_DT_HAS_N(ino)		(INO_DIRTYPE((ino)) >= (int) INO_DT_HN)

#define INO_MKHANDLER(e, hi)		((((hi) & 0x7FFFU) << 16) | (((e) + 1) & 0xFFFFU) | 0x80000000U)
#define INO_MKHHDIR(e)			((INO_DT_HH << 28) | (((e) + 1) & 0xFFFFU))
#define INO_MKHUDIR(e)			((INO_DT_HU << 28) | (((e) + 1) & 0xFFFFU))
#define INO_MKHNDIR(e)			((INO_DT_HN << 28) | (((e) + 1) & 0xFFFFU))
#define INO_GLOBALDIR			(INO_DT_GLOBAL << 28)
#define INO_ENUMBERSDIR			(INO_DT_U << 28)
#define INO_ISHANDLER(ino)		(((ino) & 0x80000000U) != 0)

#define INO_DEBUG			0

// /click: .e > .h > element names > global handlers
// /click/.e: element numbers
// /click/.e/NUM: handlers
// /click/.h: global handlers
// /click/ELEMENTNAME: .h > element names > element handlers
// /click/ELEMENTNAME/.h: element handlers

class ClickIno { public:

    void initialize();
    void cleanup();

    uint32_t generation() const		{ return _generation; }
    
    // All operations should be called with a configuration lock held.
    inline int prepare(Router *router, uint32_t generation);
    int nlink(ino_t ino);
    ino_t lookup(ino_t dir, const String &component);

    // readdir doesn't handle '.' or '..'.
    // It returns 0 for "filldir failed, have more", 1 for "out", <0 on error.
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
