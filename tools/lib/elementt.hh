// -*- c-basic-offset: 4 -*-
#ifndef CLICK_ELEMENTT_HH
#define CLICK_ELEMENTT_HH
#include "eclasst.hh"

struct ElementT {
    
    int flags;

    ElementT();
    ElementT(const String &, ElementClassT *, const String &, const String & = String());
    ~ElementT();

    RouterT *router() const		{ return _owner; }
    int eindex() const			{ return _eindex; }
    
    bool live() const			{ return _type; }
    bool dead() const			{ return !_type; }
    void kill();

    const String &name() const		{ return _name; }
    const char *name_c_str() const	{ return _name.c_str(); }
    bool anonymous() const		{ return _name && _name[0] == ';'; }
    
    ElementClassT *type() const		{ return _type; }
    String type_name() const		{ return _type->name(); }
    const char *type_name_c_str() const	{ return _type->printable_name_c_str(); }
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
    inline bool tunnel_connected() const;
    ElementT *tunnel_input() const	{ return _tunnel_input; }
    ElementT *tunnel_output() const	{ return _tunnel_output; }

    int ninputs() const			{ return _ninputs; }
    int noutputs() const		{ return _noutputs; }
    
    inline String declaration() const;

    void *user_data() const		{ return _user_data; }
    void set_user_data(void *v)		{ _user_data = v; }
    void set_user_data(intptr_t v)	{ _user_data = (void *)v; }

    static bool name_ok(const String &, bool allow_anon_names = false);
    static void redeclaration_error(ErrorHandler *, const char *type, String name, const String &landmark, const String &old_landmark);
    
  private:

    int _eindex;
    String _name;
    ElementClassT *_type;
    String _configuration;
    String _landmark;
    int _ninputs;
    int _noutputs;
    ElementT *_tunnel_input;
    ElementT *_tunnel_output;
    RouterT *_owner;
    void *_user_data;

    ElementT(const ElementT &);
    ElementT &operator=(const ElementT &);

    friend class RouterT;
    
};

struct PortT {
  
    ElementT *element;
    int port;

    PortT()				: element(0) { }
    PortT(ElementT *e, int p)		: element(e), port(p) { }

    bool live() const			{ return element != 0; }
    bool dead() const			{ return element == 0; }
    RouterT *router() const		{ return (element ? element->router() : 0); }

    int eindex() const			{ return (element ? element->eindex() : -1); }
    
    int index_in(const Vector<PortT> &, int start = 0) const;
    int force_index_in(Vector<PortT> &, int start = 0) const;

    String unparse_input() const;
    String unparse_output() const;
    
    static void sort(Vector<PortT> &);

};

class ConnectionT { public:

    ConnectionT();
    ConnectionT(const PortT &, const PortT &, const String & = String());
    ConnectionT(const PortT &, const PortT &, const String &, int, int);

    bool live() const			{ return _from.live(); }
    bool dead() const			{ return _from.dead(); }
    void kill()				{ _from.element = 0; }
    
    const PortT &from() const		{ return _from; }
    const PortT &to() const		{ return _to; }
    ElementT *from_element() const	{ return _from.element; }
    int from_eindex() const		{ return _from.eindex(); }
    int from_port() const		{ return _from.port; }
    ElementT *to_element() const	{ return _to.element; }
    int to_eindex() const		{ return _to.eindex(); }
    int to_port() const			{ return _to.port; }
    const String &landmark() const	{ return _landmark; }

    int next_from() const		{ return _next_from; }
    int next_to() const			{ return _next_to; }
    
    String unparse() const;

  private:

    PortT _from;
    PortT _to;
    String _landmark;
    int _next_from;
    int _next_to;

    friend class RouterT;
    
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
operator==(const PortT &h1, const PortT &h2)
{
    return h1.element == h2.element && h1.port == h2.port;
}

inline bool
operator!=(const PortT &h1, const PortT &h2)
{
    return h1.element != h2.element || h1.port != h2.port;
}

inline bool
operator<(const PortT &h1, const PortT &h2)
{
    return h1.eindex() < h2.eindex() || (h1.element == h2.element && h1.port < h2.port);
}

inline bool
operator>(const PortT &h1, const PortT &h2)
{
    return h1.eindex() > h2.eindex() || (h1.element == h2.element && h1.port > h2.port);
}

inline bool
operator<=(const PortT &h1, const PortT &h2)
{
    return h1.eindex() < h2.eindex() || (h1.element == h2.element && h1.port <= h2.port);
}

inline bool
operator>=(const PortT &h1, const PortT &h2)
{
    return h1.eindex() > h2.eindex() || (h1.element == h2.element && h1.port >= h2.port);
}

#endif
