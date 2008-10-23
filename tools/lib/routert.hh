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
    void deanonymize_elements();
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
    int nconnections() const			{ return _conn.size(); }
    const Vector<ConnectionT> &connections() const { return _conn; }
    const ConnectionT &connection(int cid) const {
	return _conn[cid];
    }
    bool connection_live(int cid) const {
	return _conn[cid].live();
    }

    enum { end_to = ConnectionT::end_to, end_from = ConnectionT::end_from };
    
    class conn_iterator;
    inline conn_iterator begin_connections() const;
    inline conn_iterator end_connections() const;
    conn_iterator begin_connections_touching(int eindex, int port, bool isoutput) const;
    inline conn_iterator begin_connections_touching(const PortT &port, bool isoutput) const;
    inline conn_iterator begin_connections_touching(ElementT *e, bool isoutput) const;
    
    inline conn_iterator begin_connections_from(const PortT &port) const;
    inline conn_iterator begin_connections_from(ElementT *e) const;
    inline conn_iterator begin_connections_to(const PortT &port) const;
    inline conn_iterator begin_connections_to(ElementT *e) const;
    inline conn_iterator find_connection(int ci) const;
    
    void add_tunnel(const String &namein, const String &nameout, const LandmarkT &, ErrorHandler *);

    bool add_connection(const PortT &, const PortT &, const LandmarkT &landmark = LandmarkT::empty_landmark());
    inline bool add_connection(ElementT *, int, ElementT *, int, const LandmarkT &landmark = LandmarkT::empty_landmark());
    void kill_connection(const conn_iterator &);
    void kill_bad_connections();
    void compact_connections();

    void change_connection_to(int, PortT);
    void change_connection_from(int, PortT);

    inline bool has_connection(const PortT &from, const PortT &to) const;
    int find_connection(const PortT &from, const PortT &to) const;
    void find_connections_touching(ElementT *e, bool isoutput, Vector<int> &v) const;

    /** @brief Return the ID of unique connection touching port @a port.
     * @a port port specification
     * @a isoutput true if @a port is an output port
     *
     * Returns -1 if there is no connection touching the port, or -2 if there
     * is more than one.
     * @sa connection() */
    int find_connection_id_touching(const PortT &port, bool isoutput) const;

    void find_connections_touching(const PortT &port, bool isoutput, Vector<PortT> &v, bool clear = true) const;
    void find_connections_touching(const PortT &port, bool isoutput, Vector<int> &v) const;
    void find_connection_vector_touching(ElementT *e, bool isoutput, Vector<int> &v) const;
    
    inline int find_connection_id_from(const PortT &output) const {
	return find_connection_id_touching(output, end_from);
    }
    inline const PortT &find_connection_from(const PortT &output) const;
    inline void find_connections_from(ElementT *e, Vector<int> &v) const {
	find_connections_touching(e, end_from, v);
    }
    inline void find_connections_from(const PortT &output, Vector<PortT> &v, bool clear = true) const {
	find_connections_touching(output, end_from, v, clear);
    }
    void find_connections_from(const PortT &output, Vector<int> &v) const {
	find_connections_touching(output, end_from, v);
    }
    void find_connection_vector_from(ElementT *e, Vector<int> &v) const {
	find_connection_vector_touching(e, end_from, v);
    }
    
    inline int find_connection_id_to(const PortT &input) const {
	return find_connection_id_touching(input, end_to);
    }
    inline const PortT &find_connection_to(const PortT &input) const;
    void find_connections_to(ElementT *e, Vector<int> &v) const {
	find_connections_touching(e, end_to, v);
    }
    void find_connections_to(const PortT &input, Vector<PortT> &v) const {
	find_connections_touching(input, end_to, v);
    }
    void find_connections_to(const PortT &input, Vector<int> &v) const {
	find_connections_touching(input, end_to, v);
    }
    void find_connection_vector_to(ElementT *e, Vector<int> &v) const {
	find_connection_vector_touching(e, end_to, v);
    }

    bool insert_before(const PortT &, const PortT &);
    bool insert_after(const PortT &, const PortT &);
    inline bool insert_before(ElementT *, const PortT &);
    inline bool insert_after(ElementT *, const PortT &);
    
    // REQUIREMENTS
    void add_requirement(const String &);
    void remove_requirement(const String &);
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
    void remove_dead_elements(ErrorHandler * = 0);

    void remove_compound_elements(ErrorHandler *, bool expand_vars);
    void remove_tunnels(ErrorHandler * = 0);

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
    
    int nformals() const		{ return _nformals; }
    const VariableEnvironment &scope() const { return _scope; }
    inline bool define(const String &name, const String &value, bool isformal);
    inline void redefine(const VariableEnvironment &);
    int ninputs() const			{ return _ninputs; }
    int noutputs() const		{ return _noutputs; }
    
    RouterT *declaration_scope() const	{ return _declaration_scope; }
    ElementClassT *overload_type() const { return _overload_type; }
    void set_overload_type(ElementClassT *);

    int finish_type(ErrorHandler *);
    
    bool need_resolve() const;
    ElementClassT *resolve(int, int, Vector<String> &, ErrorHandler *, const LandmarkT &landmark);
    void create_scope(const Vector<String> &args, const VariableEnvironment &env, VariableEnvironment &new_env);
    ElementT *complex_expand_element(ElementT *, const Vector<String> &, RouterT *, const String &prefix, const VariableEnvironment &, ErrorHandler *);

    String unparse_signature() const;
    void unparse_declaration(StringAccum &, const String &, UnparseKind, ElementClassT *);    

    RouterT *cast_router()		{ return this; }

  private:
  
    struct Pair {
	int end[2];
	Pair() { end[0] = end[1] = -1; }
	Pair(int from, int to) { end[end_from] = from; end[end_to] = to; }
	int &operator[](int i) { assert(i >= 0 && i <= 1); return end[i]; }
	int operator[](int i) const { assert(i >= 0 && i <= 1); return end[i]; }
    };

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

    StringMap _element_name_map;
    Vector<ElementT *> _elements;
    ElementT *_free_element;
    int _n_live_elements;
    Vector<int> *_new_eindex_collector;

    Vector<ConnectionT> _conn;
    Vector<Pair> _first_conn;
    int _free_conn;
    
    StringMap _declared_type_map;
    Vector<ElementType> _declared_types;

    Vector<String> _requirements;

    StringMap _archive_map;
    Vector<ArchiveElement> _archive;

    RouterT *_declaration_scope;
    int _declaration_scope_cookie;
    int _scope_cookie;

    VariableEnvironment _scope;
    int _nformals;
    int _ninputs;
    int _noutputs;
    bool _scope_order_error;
    bool _circularity_flag;
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
    void free_connection(int ci);
    void unlink_connection_from(int ci);
    void unlink_connection_to(int ci);
    void expand_tunnel(Vector<PortT> *port_expansions, const Vector<PortT> &ports, bool is_output, int which, ErrorHandler *) const;
    int assign_arguments(const Vector<String> &, Vector<String> *) const;

    friend class RouterUnparserT;
    friend class conn_iterator;
    
};

class RouterT::const_iterator { public:
    operator bool() const		{ return _e; }
    int eindex() const			{ return _e->eindex(); }
    void operator++()			{ if (_e) step(_e->router(), eindex()+1);}
    void operator++(int)		{ ++(*this); }
    operator const ElementT *() const	{ return _e; }
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
    operator ElementT *() const		{ return const_cast<ElementT *>(_e); }
    ElementT *operator->() const	{ return const_cast<ElementT *>(_e); }
    ElementT *get() const		{ return const_cast<ElementT *>(_e); }
    ElementT &operator*() const		{ return const_cast<ElementT &>(*_e); }
  private:
    iterator()				: const_iterator() { }
    iterator(RouterT *r, int ei)	: const_iterator(r, ei) { }
    friend class RouterT;
};

class RouterT::const_type_iterator { public:
    operator bool() const		{ return _e; }
    int eindex() const			{ return _e->eindex(); }
    inline void operator++();
    inline void operator++(int);
    operator const ElementT *() const	{ return _e; }
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
    operator ElementT *() const		{ return const_cast<ElementT *>(_e); }
    ElementT *operator->() const	{ return const_cast<ElementT *>(_e); }
    ElementT &operator*() const		{ return const_cast<ElementT &>(*_e); }
  private:
    type_iterator()			: const_type_iterator() { }
    type_iterator(RouterT *r, ElementClassT *t, int ei)	: const_type_iterator(r, t, ei) { }
    friend class RouterT;
};

class RouterT::conn_iterator { public:
    inline conn_iterator()			: _conn(0), _by(0) { }
    inline void operator++();
    inline void operator++(int);
    operator const ConnectionT &() const	{ return *_conn; }
    const ConnectionT *operator->() const	{ return _conn; }
    const ConnectionT &operator*() const	{ return *_conn; }
  private:
    const ConnectionT *_conn;
    int _by;
    inline conn_iterator(const ConnectionT *conn, int by);
    void complex_step(const RouterT *);
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
RouterT::conn_iterator::conn_iterator(const ConnectionT *conn, int by)
    : _conn(conn), _by(by)
{
    if (_conn && _conn->dead())
	(*this)++;
}

inline RouterT::conn_iterator
RouterT::begin_connections() const
{
    if (_conn.size())
	return conn_iterator(_conn.begin(), 0);
    else
	return conn_iterator();
}

inline RouterT::conn_iterator
RouterT::end_connections() const
{
    return conn_iterator();
}

inline RouterT::conn_iterator RouterT::begin_connections_touching(const PortT &port, bool isoutput) const
{
    assert(port.router() == this);
    return begin_connections_touching(port.eindex(), port.port, isoutput);
}

inline RouterT::conn_iterator RouterT::begin_connections_touching(ElementT *e, bool isoutput) const
{
    assert(e->router() == this);
    return begin_connections_touching(e->eindex(), -1, isoutput);
}

inline RouterT::conn_iterator RouterT::begin_connections_from(const PortT &port) const
{
    return begin_connections_touching(port, end_from);
}

inline RouterT::conn_iterator RouterT::begin_connections_from(ElementT *e) const
{
    return begin_connections_touching(e, end_from);
}

inline RouterT::conn_iterator RouterT::begin_connections_to(const PortT &port) const
{
    return begin_connections_touching(port, end_to);
}

inline RouterT::conn_iterator RouterT::begin_connections_to(ElementT *e) const
{
    return begin_connections_touching(e, end_to);
}

inline RouterT::conn_iterator
RouterT::find_connection(int c) const
{
    if (c < 0 || c >= _conn.size())
	return end_connections();
    else
	return conn_iterator(&_conn[c], 0);
}

inline void
RouterT::conn_iterator::operator++()
{
    if (_conn) {
	const RouterT *r = _conn->router();
      again:
	if (_by == 0)
	    ++_conn;
	else
	    complex_step(r);
	if (_conn == r->_conn.end())
	    _conn = 0;
	else if (_conn && !_conn->live())
	    goto again;
    }
}

inline void
RouterT::conn_iterator::operator++(int)
{
    ++(*this);
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
RouterT::has_connection(const PortT &hfrom, const PortT &hto) const
{
    return find_connection(hfrom, hto) >= 0;
}

inline const PortT &
RouterT::find_connection_from(const PortT &h) const
{
    int c = find_connection_id_from(h);
    return (c >= 0 ? _conn[c].to() : PortT::null_port);
}

inline const PortT &
RouterT::find_connection_to(const PortT &h) const
{
    int c = find_connection_id_to(h);
    return (c >= 0 ? _conn[c].from() : PortT::null_port);
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
RouterT::define(const String &name, const String &value, bool isformal)
{
    assert(!isformal || _nformals == _scope.size());
    bool retval = _scope.define(name, value, false);
    if (isformal)
	_nformals = _scope.size();
    return retval;
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
    return i.operator->() == j.operator->();
}

inline bool
operator!=(const RouterT::const_iterator &i, const RouterT::const_iterator &j)
{
    return i.operator->() != j.operator->();
}

inline bool
operator==(const RouterT::const_type_iterator &i, const RouterT::const_type_iterator &j)
{
    return i.operator->() == j.operator->();
}

inline bool
operator!=(const RouterT::const_type_iterator &i, const RouterT::const_type_iterator &j)
{
    return i.operator->() != j.operator->();
}

inline bool
operator==(const RouterT::const_type_iterator &i, const RouterT::const_iterator &j)
{
    return i.operator->() == j.operator->();
}

inline bool
operator!=(const RouterT::const_type_iterator &i, const RouterT::const_iterator &j)
{
    return i.operator->() != j.operator->();
}

inline bool
operator==(const RouterT::conn_iterator &i, const RouterT::conn_iterator &j)
{
    return i.operator->() == j.operator->();
}

inline bool
operator!=(const RouterT::conn_iterator &i, const RouterT::conn_iterator &j)
{
    return i.operator->() != j.operator->();
}

#endif
