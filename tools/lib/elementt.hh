// -*- c-basic-offset: 4 -*-
#ifndef CLICK_ELEMENTT_HH
#define CLICK_ELEMENTT_HH
#include "eclasst.hh"

struct ElementT {
    
    String name;
    String configuration;
    int tunnel_input;
    int tunnel_output;
    String landmark;
    int flags;

    ElementT();
    ElementT(const String &, ElementClassT *, const String &, const String & = String());
    ElementT(const ElementT &);
    ~ElementT();

    ElementClassT *type() const		{ return _type; }
    String type_name() const		{ return _type->name(); }
    int type_uid() const		{ return _type ? _type->uid() : -1; }
    bool live() const			{ return _type; }
    bool dead() const			{ return !_type; }
    bool tunnel() const			{ return _type == ElementClassT::tunnel_type(); }
    void kill();
    void set_type(ElementClassT *);

    String declaration() const;
    
    ElementT &operator=(const ElementT &);

  private:

    ElementClassT *_type;
    
};

struct Hookup {
  
    int idx;
    int port;

    Hookup()				: idx(-1) { }
    Hookup(int i, int p)		: idx(i), port(p) { }

    bool live() const			{ return idx >= 0; }
    bool dead() const			{ return idx < 0; }

    int index_in(const Vector<Hookup> &, int start = 0) const;
    int force_index_in(Vector<Hookup> &, int start = 0) const;

    static int sorter(const void *, const void *);
    static void sort(Vector<Hookup> &);

};


inline void
ElementT::kill()
{
    if (_type)
	_type->unuse();
    _type = 0;
}

inline void
ElementT::set_type(ElementClassT *t)
{
    assert(t);
    t->use();
    if (_type)
	_type->unuse();
    _type = t;
}

inline String
ElementT::declaration() const
{
    assert(_type);
    return name + " :: " + _type->name();
}

inline bool
operator==(const Hookup &h1, const Hookup &h2)
{
    return h1.idx == h2.idx && h1.port == h2.port;
}

inline bool
operator!=(const Hookup &h1, const Hookup &h2)
{
    return h1.idx != h2.idx || h1.port != h2.port;
}

inline bool
operator<(const Hookup &h1, const Hookup &h2)
{
    return h1.idx < h2.idx || (h1.idx == h2.idx && h1.port < h2.port);
}

inline bool
operator>(const Hookup &h1, const Hookup &h2)
{
    return h1.idx > h2.idx || (h1.idx == h2.idx && h1.port > h2.port);
}

inline bool
operator<=(const Hookup &h1, const Hookup &h2)
{
    return h1.idx < h2.idx || (h1.idx == h2.idx && h1.port <= h2.port);
}

inline bool
operator>=(const Hookup &h1, const Hookup &h2)
{
    return h1.idx > h2.idx || (h1.idx == h2.idx && h1.port >= h2.port);
}

#endif
