// -*- c-basic-offset: 4 -*-
#ifndef CLICK_ELEMENTT_HH
#define CLICK_ELEMENTT_HH
#include "eclasst.hh"

struct ElementT {
    
    int flags;

    ElementT();
    ElementT(const String &, ElementClassT *, const String &, const String & = String());
    ~ElementT();

    int idx() const			{ return _idx; }
    
    bool live() const			{ return _type; }
    bool dead() const			{ return !_type; }
    void kill();

    const String &name() const		{ return _name; }
    const char *name_cc() const		{ return _name.cc(); }
    
    ElementClassT *type() const		{ return _type; }
    String type_name() const		{ return _type->name(); }
    int type_uid() const		{ return _type ? _type->uid() : -1; }
    void set_type(ElementClassT *);

    const String &config() const	{ return _configuration; }
    const String &configuration() const	{ return _configuration; }
    void set_config(const String &s)	{ _configuration = s; }
    void set_configuration(const String &s) { _configuration = s; }
    String &config()			{ return _configuration; }
    String &configuration()		{ return _configuration; }
    
    const String &landmark() const	{ return _landmark; }
    void set_landmark(const String &s)	{ _landmark = s; }
    String &landmark()			{ return _landmark; }
    
    bool tunnel() const		{ return _type==ElementClassT::tunnel_type(); }
    bool tunnel_connected() const;
    ElementT *tunnel_input() const	{ return _tunnel_input; }
    ElementT *tunnel_output() const	{ return _tunnel_output; }

    int ninputs() const			{ return _ninputs; }
    int noutputs() const		{ return _noutputs; }
    
    String declaration() const;
    
  private:

    mutable String _name;		// mutable for cc()
    int _idx;
    ElementClassT *_type;
    String _configuration;
    String _landmark;
    int _ninputs;
    int _noutputs;
    ElementT *_tunnel_input;
    ElementT *_tunnel_output;

    ElementT(const ElementT &);
    ElementT &operator=(const ElementT &);

    friend class RouterT;
    
};

struct HookupI {
  
    int idx;
    int port;

    HookupI()				: idx(-1) { }
    HookupI(int i, int p)		: idx(i), port(p) { }

    bool live() const			{ return idx >= 0; }
    bool dead() const			{ return idx < 0; }

    int index_in(const Vector<HookupI> &, int start = 0) const;
    int force_index_in(Vector<HookupI> &, int start = 0) const;

    static int sorter(const void *, const void *);
    static void sort(Vector<HookupI> &);

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
    return _name + " :: " + _type->name();
}

inline bool
ElementT::tunnel_connected() const
{
    return _tunnel_input || _tunnel_output;
}

inline bool
operator==(const HookupI &h1, const HookupI &h2)
{
    return h1.idx == h2.idx && h1.port == h2.port;
}

inline bool
operator!=(const HookupI &h1, const HookupI &h2)
{
    return h1.idx != h2.idx || h1.port != h2.port;
}

inline bool
operator<(const HookupI &h1, const HookupI &h2)
{
    return h1.idx < h2.idx || (h1.idx == h2.idx && h1.port < h2.port);
}

inline bool
operator>(const HookupI &h1, const HookupI &h2)
{
    return h1.idx > h2.idx || (h1.idx == h2.idx && h1.port > h2.port);
}

inline bool
operator<=(const HookupI &h1, const HookupI &h2)
{
    return h1.idx < h2.idx || (h1.idx == h2.idx && h1.port <= h2.port);
}

inline bool
operator>=(const HookupI &h1, const HookupI &h2)
{
    return h1.idx > h2.idx || (h1.idx == h2.idx && h1.port >= h2.port);
}

#endif
