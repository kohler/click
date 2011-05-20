// -*- c-basic-offset: 4 -*-
#ifndef CLICK_ROUTERT_HH
#define CLICK_ROUTERT_HH
#include "elementt.hh"
#include "eclasst.hh"
#include <click/error.hh>
#include <click/hashtable.hh>
#include <click/archive.hh>
#include <click/variableenv.hh>
typedef HashTable<String, int> StringMap;

class RouterT : public ElementClassT { public:

    RouterT();
    RouterT(const String &name, const LandmarkT &landmark, RouterT *declaration_scope = 0);
    virtual ~RouterT();

    // ELEMENTS
    int nelements() const		{ return _elements.size(); }
    int n_live_elements() const		{ return _n_live_elements; }

    inline const ElementT *element(const String &) const;
    inline ElementT *element(const String &);
    int eindex(const String &name) const { return _element_name_map[name]; }

    const ElementT *element(int i) const{ return _elements[i]; }
    ElementT *element(int i)		{ return _elements[i]; }

    class iterator;
    class const_iterator;
    class type_iterator;
    class const_type_iterator;
    inline iterator begin_elements();
    inline const_iterator begin_elements() const;
    inline type_iterator begin_elements(ElementClassT *);
    inline const_type_iterator begin_elements(ElementClassT *) const;
    inline iterator end_elements();
    inline const_iterator end_elements() const;

    bool elive(int i) const		{ return _elements[i]->live(); }
    bool edead(int i) const		{ return _elements[i]->dead(); }
    inline String ename(int) const;
    inline ElementClassT *etype(int) const;
    inline String etype_name(int) const;

    ElementT *get_element(const String &name, ElementClassT *eclass, const String &configuration, const LandmarkT &landmark);
    ElementT *add_anon_element(ElementClassT *eclass, const String &configuration = String(), const LandmarkT &landmark = LandmarkT::empty_landmark());
    void change_ename(int, const String &);
    int __map_element_name(const String &name, int new_eindex);
    void assign_element_names();
    void free_element(ElementT *);
    void free_dead_elements();

    void set_new_eindex_collector(Vector<int> *v) { _new_eindex_collector=v; }

    // TYPES
    ElementClassT *locally_declared_type(const String &) const;
    inline ElementClassT *declared_type(const String &) const;
    void add_declared_type(ElementClassT *, bool anonymous);

    void collect_types(HashTable<ElementClassT *, int> &) const;
    void collect_locally_declared_types(Vector<ElementClassT *> &) const;
    void collect_overloads(Vector<ElementClassT *> &) const;

    // CONNECTIONS
    enum { end_to = ConnectionT::end_to, end_from = ConnectionT::end_from };

    class conn_iterator;
    inline conn_iterator begin_connections() const;
    inline conn_iterator end_connections() const;

    conn_iterator find_connections_touching(int eindex, int port, bool isoutput) const;
    inline conn_iterator find_connections_touching(const PortT &port, bool isoutput) const;
    inline conn_iterator find_connections_touching(ElementT *e, bool isoutput) const;
    inline conn_iterator find_connections_touching(ElementT *e, int port, bool isoutput) const;

    inline conn_iterator find_connections_from(const PortT &port) const;
    inline conn_iterator find_connections_from(ElementT *e) const;
    inline conn_iterator find_connections_from(ElementT *e, int port) const;
    inline conn_iterator find_connections_to(const PortT &port) const;
    inline conn_iterator find_connections_to(ElementT *e) const;
    inline conn_iterator find_connections_to(ElementT *e, int port) const;

    bool has_connection(const PortT &from, const PortT &to) const;

    void find_connections_touching(const PortT &port, bool isoutput, Vector<PortT> &v, bool clear = true) const;
    inline void find_connections_from(const PortT &output, Vector<PortT> &v, bool clear = true) const {
	find_connections_touching(output, end_from, v, clear);
    }
    void find_connections_to(const PortT &input, Vector<PortT> &v) const {
	find_connections_touching(input, end_to, v);
    }

    void find_connection_vector_touching(const ElementT *e, bool isoutput, Vector<conn_iterator> &x) const;
    inline void find_connection_vector_from(const ElementT *e, Vector<conn_iterator> &x) const {
	find_connection_vector_touching(e, end_from, x);
    }
    inline void find_connection_vector_to(const ElementT *e, Vector<conn_iterator> &x) const {
	find_connection_vector_touching(e, end_to, x);
    }

    void add_tunnel(const String &namein, const String &nameout, const LandmarkT &landmark, ErrorHandler *errh);

    bool add_connection(const PortT &, const PortT &, const LandmarkT &landmark = LandmarkT::empty_landmark());
    inline bool add_connection(ElementT *, int, ElementT *, int, const LandmarkT &landmark = LandmarkT::empty_landmark());
    conn_iterator erase(conn_iterator it);
    void kill_bad_connections();

    conn_iterator change_connection_to(conn_iterator it, PortT new_to);
    conn_iterator change_connection_from(conn_iterator it, PortT new_from);

    bool insert_before(const PortT &, const PortT &);
    bool insert_after(const PortT &, const PortT &);
    inline bool insert_before(ElementT *, const PortT &);
    inline bool insert_after(ElementT *, const PortT &);

    // REQUIREMENTS
    void add_requirement(const String &type, const String &value);
    void remove_requirement(const String &type, const String &value);
    const Vector<String> &requirements() const	{ return _requirements; }

    // ARCHIVE
    void add_archive(const ArchiveElement &);
    int narchive() const			{ return _archive.size(); }
    int archive_index(const String &s) const	{ return _archive_map[s]; }
    const Vector<ArchiveElement> &archive() const{ return _archive; }
    ArchiveElement &archive(int i)		{ return _archive[i]; }
    const ArchiveElement &archive(int i) const	{ return _archive[i]; }
    inline ArchiveElement &archive(const String &s);
    inline const ArchiveElement &archive(const String &s) const;

    void add_components_to(RouterT *, const String &prefix = String()) const;

    // CHECKING, FLATTENING AND EXPANDING
    void check() const;

    void remove_duplicate_connections();
    void remove_dead_elements();

    void remove_compound_elements(ErrorHandler *, bool expand_vars);
    void remove_tunnels(ErrorHandler *errh = 0);
    void compact();

    void expand_into(RouterT *dest, const String &prefix, VariableEnvironment &env, ErrorHandler *errh);
    void flatten(ErrorHandler *errh, bool expand_vars = false);

    // UNPARSING
    void unparse(StringAccum &, const String & = String()) const;
    void unparse_requirements(StringAccum &, const String & = String()) const;
    void unparse_defines(StringAccum &, const String & = String()) const;
    void unparse_declarations(StringAccum &, const String & = String()) const;
    void unparse_connections(StringAccum &, const String & = String()) const;
    String configuration_string() const;

    // COMPOUND ELEMENTS
    String landmark() const		{ return _type_landmark.str(); }
    String decorated_landmark() const	{ return _type_landmark.decorated_str(); }
    void set_landmarkt(const LandmarkT &l) { _type_landmark = l; }
    const ElementTraits *find_traits(ElementMap *emap) const;

    bool primitive() const		{ return false; }
    bool overloaded() const;

    inline bool defines(const String &name) const;

    int nformals() const		{ return _formals.size(); }
    inline bool add_formal(const String &name, const String &type);
    const String &formal_name(int i) const { return _formals[i]; }
    const String &formal_type(int i) const { return _formal_types[i]; }

    const VariableEnvironment &scope() const { return _scope; }
    inline bool define(const String &name, const String &value);
    inline void redefine(const VariableEnvironment &);
    int ninputs() const			{ return _ninputs; }
    int noutputs() const		{ return _noutputs; }

    RouterT *declaration_scope() const	{ return _declaration_scope; }
    ElementClassT *overload_type() const { return _overload_type; }
    void set_overload_type(ElementClassT *);

    int check_pseudoelement(const ElementT *e, bool isoutput, const char *name, ErrorHandler *errh) const;
    int finish_type(ErrorHandler *errh);

    bool need_resolve() const;
    ElementClassT *resolve(int, int, Vector<String> &, ErrorHandler *, const LandmarkT &landmark);
    void create_scope(const Vector<String> &args, const VariableEnvironment &env, VariableEnvironment &new_env);
    ElementT *complex_expand_element(ElementT *, const Vector<String> &, RouterT *, const String &prefix, const VariableEnvironment &, ErrorHandler *);

    String unparse_signature() const;
    void unparse_declaration(StringAccum &, const String &, UnparseKind, ElementClassT *);

    RouterT *cast_router()		{ return this; }

  private:

    struct ElementType {
	ElementClassT * const type;
	int scope_cookie;
	int prev_name;
	ElementType(ElementClassT *c, int sc, int pn) : type(c), scope_cookie(sc), prev_name(pn) { assert(type); type->use(); }
	ElementType(const ElementType &o) : type(o.type), scope_cookie(o.scope_cookie), prev_name(o.prev_name) { type->use(); }
	~ElementType()			{ type->unuse(); }
	const String &name() const	{ return type->name(); }
      private:
	ElementType &operator=(const ElementType &);
    };

    enum { end_all = 2 };

    struct ConnectionX : public ConnectionT {
	ConnectionX **_pprev;
	ConnectionX *_next[3];
	ConnectionX(const PortT &from, const PortT &to,
		    const LandmarkT &landmark)
	    : ConnectionT(from, to, landmark) {
	}
	ConnectionX *next_from() const {
	    return _next[end_from];
	}
	ConnectionX *next_to() const {
	    return _next[end_to];
	}
    };

    struct ConnectionSet {
	ConnectionSet *next;
	ConnectionX c[1];
    };

    struct Pair {
	ConnectionX *end[2];
	Pair() { end[0] = end[1] = 0; }
	Pair(ConnectionX *from, ConnectionX *to) { end[end_from] = from; end[end_to] = to; }
	ConnectionX *&operator[](int i) { assert(i >= 0 && i <= 1); return end[i]; }
	ConnectionX *operator[](int i) const { assert(i >= 0 && i <= 1); return end[i]; }
    };

    StringMap _element_name_map;
    Vector<ElementT *> _elements;
    ElementT *_free_element;
    int _n_live_elements;
    Vector<int> *_new_eindex_collector;

    ConnectionSet *_connsets;
    int _nconnx;
    Vector<Pair> _first_conn;
    ConnectionX *_conn_head;
    ConnectionX **_conn_tail;
    ConnectionX *_free_conn;

    StringMap _declared_type_map;
    Vector<ElementType> _declared_types;

    Vector<String> _requirements;

    StringMap _archive_map;
    Vector<ArchiveElement> _archive;

    RouterT *_declaration_scope;
    int _declaration_scope_cookie;
    int _scope_cookie;

    VariableEnvironment _scope;
    Vector<String> _formals;
    Vector<String> _formal_types;
    int _ninputs;
    int _noutputs;
    bool _scope_order_error;
    bool _circularity_flag;
    bool _potential_duplicate_connections;
    ElementClassT *_overload_type;
    LandmarkT _type_landmark;
    mutable ElementTraits _traits;

    RouterT(const RouterT &);
    RouterT &operator=(const RouterT &);

    ElementClassT *declared_type(const String &, int scope_cookie) const;
    void update_noutputs(int);
    void update_ninputs(int);
    ElementT *add_element(const ElementT &);
    void assign_element_name(int);
    void free_connection(ConnectionX *c);
    void unlink_connection_from(ConnectionX *c);
    void unlink_connection_to(ConnectionX *c);
    void expand_tunnel(Vector<PortT> *port_expansions, const Vector<PortT> &ports, bool is_output, int which, ErrorHandler *) const;
    int assign_arguments(const Vector<String> &, Vector<String> *) const;

    friend class RouterUnparserT;
    friend class conn_iterator;

};

class RouterT::const_iterator { public:
    typedef int (const_iterator::*unspecified_bool_type)() const;
    operator unspecified_bool_type() const { return _e ? &const_iterator::eindex : 0; }
    int eindex() const			{ return _e->eindex(); }
    void operator++()			{ if (_e) step(_e->router(), eindex()+1);}
    void operator++(int)		{ ++(*this); }
    const ElementT *operator->() const	{ return _e; }
    const ElementT *get() const		{ return _e; }
    const ElementT &operator*() const	{ return *_e; }
  private:
    const ElementT *_e;
    const_iterator()			: _e(0) { }
    const_iterator(const RouterT *r, int ei) { step(r, ei); }
    void step(const RouterT *, int);
    friend class RouterT;
    friend class RouterT::iterator;
};

class RouterT::iterator : public RouterT::const_iterator { public:
    ElementT *operator->() const	{ return const_cast<ElementT *>(_e); }
    ElementT *get() const		{ return const_cast<ElementT *>(_e); }
    ElementT &operator*() const		{ return const_cast<ElementT &>(*_e); }
  private:
    iterator()				: const_iterator() { }
    iterator(RouterT *r, int ei)	: const_iterator(r, ei) { }
    friend class RouterT;
};

class RouterT::const_type_iterator { public:
    typedef int (const_type_iterator::*unspecified_bool_type)() const;
    operator unspecified_bool_type() const { return _e ? &const_type_iterator::eindex : 0; }
    int eindex() const			{ return _e->eindex(); }
    inline void operator++();
    inline void operator++(int);
    const ElementT *operator->() const	{ return _e; }
    const ElementT *get() const		{ return _e; }
    const ElementT &operator*() const	{ return *_e; }
  private:
    const ElementT *_e;
    ElementClassT *_t;
    const_type_iterator()		: _e(0), _t(0) { }
    const_type_iterator(const RouterT *r, ElementClassT *t, int i) : _t(t) { step(r, i); }
    void step(const RouterT *, int);
    friend class RouterT;
    friend class RouterT::type_iterator;
};

class RouterT::type_iterator : public RouterT::const_type_iterator { public:
    ElementT *operator->() const	{ return const_cast<ElementT *>(_e); }
    ElementT *get() const		{ return const_cast<ElementT *>(_e); }
    ElementT &operator*() const		{ return const_cast<ElementT &>(*_e); }
  private:
    type_iterator()			: const_type_iterator() { }
    type_iterator(RouterT *r, ElementClassT *t, int ei)	: const_type_iterator(r, t, ei) { }
    friend class RouterT;
};

class RouterT::conn_iterator { public:
    inline conn_iterator()			: _conn(0), _by(0) { }
    typedef bool (conn_iterator::*unspecified_bool_type)() const;
    operator unspecified_bool_type() const	{ return _conn ? &conn_iterator::empty : 0; }
    inline bool empty() const			{ return !_conn; }
    inline bool is_back() const;
    inline conn_iterator &operator++();
    inline void operator++(int);
    const ConnectionT *operator->() const	{ return _conn; }
    const ConnectionT *get() const		{ return _conn; }
    const ConnectionT &operator*() const	{ return *_conn; }
  private:
    const ConnectionX *_conn;
    int _by;
    inline conn_iterator(const ConnectionX *conn, int by);
    void assign(const ConnectionX *conn, int by) {
	_conn = conn;
	_by = by;
    }
    void complex_step();
    friend class RouterT;
};


inline RouterT::iterator
RouterT::begin_elements()
{
    return iterator(this, 0);
}

inline RouterT::const_iterator
RouterT::begin_elements() const
{
    return const_iterator(this, 0);
}

inline RouterT::type_iterator
RouterT::begin_elements(ElementClassT *t)
{
    return type_iterator(this, t, 0);
}

inline RouterT::const_type_iterator
RouterT::begin_elements(ElementClassT *t) const
{
    return const_type_iterator(this, t, 0);
}

inline RouterT::const_iterator
RouterT::end_elements() const
{
    return const_iterator();
}

inline RouterT::iterator
RouterT::end_elements()
{
    return iterator();
}

inline void
RouterT::const_type_iterator::operator++()
{
    if (_e)
	step(_e->router(), _e->eindex() + 1);
}

inline void
RouterT::const_type_iterator::operator++(int)
{
    ++(*this);
}

inline
RouterT::conn_iterator::conn_iterator(const ConnectionX *conn, int by)
    : _conn(conn), _by(by)
{
}

inline RouterT::conn_iterator
RouterT::begin_connections() const
{
    return conn_iterator(_conn_head, end_all);
}

inline RouterT::conn_iterator
RouterT::end_connections() const
{
    return conn_iterator();
}

inline RouterT::conn_iterator RouterT::find_connections_touching(const PortT &port, bool isoutput) const
{
    assert(port.router() == this);
    return find_connections_touching(port.eindex(), port.port, isoutput);
}

inline RouterT::conn_iterator RouterT::find_connections_touching(ElementT *e, bool isoutput) const
{
    assert(e->router() == this);
    return find_connections_touching(e->eindex(), -1, isoutput);
}

inline RouterT::conn_iterator RouterT::find_connections_touching(ElementT *e, int port, bool isoutput) const
{
    assert(e->router() == this);
    return find_connections_touching(e->eindex(), port, isoutput);
}

inline RouterT::conn_iterator RouterT::find_connections_from(const PortT &port) const
{
    return find_connections_touching(port, end_from);
}

inline RouterT::conn_iterator RouterT::find_connections_from(ElementT *e) const
{
    return find_connections_touching(e, end_from);
}

inline RouterT::conn_iterator RouterT::find_connections_from(ElementT *e, int port) const
{
    return find_connections_touching(e, port, end_from);
}

inline RouterT::conn_iterator RouterT::find_connections_to(const PortT &port) const
{
    return find_connections_touching(port, end_to);
}

inline RouterT::conn_iterator RouterT::find_connections_to(ElementT *e) const
{
    return find_connections_touching(e, end_to);
}

inline RouterT::conn_iterator RouterT::find_connections_to(ElementT *e, int port) const
{
    return find_connections_touching(e, port, end_to);
}

inline RouterT::conn_iterator &
RouterT::conn_iterator::operator++()
{
    if (_conn && unsigned(_by) <= end_all)
	_conn = _conn->_next[_by];
    else
	complex_step();
    return *this;
}

inline void
RouterT::conn_iterator::operator++(int)
{
    ++(*this);
}

inline bool
RouterT::conn_iterator::is_back() const
{
    conn_iterator x(*this);
    return x && !++x;
}

inline const ElementT *
RouterT::element(const String &s) const
{
    int i = _element_name_map[s];
    return (i >= 0 ? _elements[i] : 0);
}

inline ElementT *
RouterT::element(const String &s)
{
    int i = _element_name_map.get(s);
    return (i >= 0 ? _elements[i] : 0);
}

inline String
RouterT::ename(int e) const
{
    return _elements[e]->name();
}

inline ElementClassT *
RouterT::etype(int e) const
{
    return _elements[e]->type();
}

inline String
RouterT::etype_name(int e) const
{
    return _elements[e]->type()->name();
}

inline ElementClassT *
RouterT::declared_type(const String &name) const
{
    return declared_type(name, 0x7FFFFFFF);
}

inline bool
RouterT::add_connection(ElementT *from_elt, int from_port, ElementT *to_elt,
			int to_port, const LandmarkT &landmark)
{
    return add_connection(PortT(from_elt, from_port), PortT(to_elt, to_port), landmark);
}

inline bool
RouterT::insert_before(ElementT *e, const PortT &h)
{
    return insert_before(PortT(e, 0), h);
}

inline bool
RouterT::insert_after(ElementT *e, const PortT &h)
{
    return insert_after(PortT(e, 0), h);
}

inline bool
RouterT::defines(const String &name) const
{
    for (const String *f = _formals.begin(); f != _formals.end(); ++f)
	if (*f == name)
	    return true;
    return _scope.defines(name);
}

inline bool
RouterT::add_formal(const String &name, const String &type)
{
    if (defines(name))
	return false;
    _formals.push_back(name);
    _formal_types.push_back(type);
    return true;
}

inline bool
RouterT::define(const String &name, const String &value)
{
    if (defines(name))
	return false;
    return _scope.define(name, value, false);
}

inline void
RouterT::redefine(const VariableEnvironment &ve)
{
    for (int i = 0; i < ve.size(); i++)
	_scope.define(ve.name(i), ve.value(i), true);
}

inline ArchiveElement &
RouterT::archive(const String &name)
{
    return _archive[_archive_map.get(name)];
}

inline const ArchiveElement &
RouterT::archive(const String &name) const
{
    return _archive[_archive_map[name]];
}

inline bool
operator==(const RouterT::const_iterator &i, const RouterT::const_iterator &j)
{
    return i.get() == j.get();
}

inline bool
operator!=(const RouterT::const_iterator &i, const RouterT::const_iterator &j)
{
    return i.get() != j.get();
}

inline bool
operator==(const RouterT::const_type_iterator &i, const RouterT::const_type_iterator &j)
{
    return i.get() == j.get();
}

inline bool
operator!=(const RouterT::const_type_iterator &i, const RouterT::const_type_iterator &j)
{
    return i.get() != j.get();
}

inline bool
operator==(const RouterT::const_type_iterator &i, const RouterT::const_iterator &j)
{
    return i.get() == j.get();
}

inline bool
operator!=(const RouterT::const_type_iterator &i, const RouterT::const_iterator &j)
{
    return i.get() != j.get();
}

inline bool
operator==(const RouterT::conn_iterator &i, const RouterT::conn_iterator &j)
{
    return i.get() == j.get();
}

inline bool
operator!=(const RouterT::conn_iterator &i, const RouterT::conn_iterator &j)
{
    return i.get() != j.get();
}

inline RouterT::conn_iterator
operator+(RouterT::conn_iterator it, int x)
{
    while (x-- > 0)
	++it;
    return it;
}

#endif
