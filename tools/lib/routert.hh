// -*- c-basic-offset: 4 -*-
#ifndef CLICK_ROUTERT_HH
#define CLICK_ROUTERT_HH
#include "elementt.hh"
#include "eclasst.hh"
#include <click/error.hh>
#include <click/hashmap.hh>
#include <click/archive.hh>
typedef HashMap<String, int> StringMap;

class RouterT { public:

    class iterator;
    class live_iterator;
    class type_iterator;

    RouterT(RouterT * = 0);
    virtual ~RouterT();

    void use()				{ _use_count++; }
    void unuse()			{ if (--_use_count <= 0) delete this; }
  
    void check() const;
    bool is_flat() const;

    ElementClassT *try_type(const String &) const;
    ElementClassT *get_type(const String &);
    ElementClassT *get_type(ElementClassT *, bool install_name = false);
    void collect_primitive_classes(HashMap<String, int> &) const;
    void collect_active_types(Vector<ElementClassT *> &) const;

    iterator first_element();
    live_iterator first_live_element();
    type_iterator first_element(ElementClassT *);
    
    int nelements() const		{ return _elements.size(); }
    int real_element_count() const	{ return _real_ecount; }
    int eindex(const String &s) const	{ return _element_name_map[s]; }
    const ElementT *element(int i) const{ return _elements[i]; }
    const ElementT *elt(int i) const	{ return _elements[i]; }
    const ElementT *e(int i) const	{ return _elements[i]; }
    ElementT *element(int i)		{ return _elements[i]; }
    ElementT *elt(int i)		{ return _elements[i]; }
    ElementT *e(int i)			{ return _elements[i]; }
    bool elive(int ei) const		{ return _elements[ei]->live(); }
    bool edead(int ei) const		{ return _elements[ei]->dead(); }
    String ename(int) const;
    ElementClassT *etype(int) const;
    int etype_uid(int) const;
    String etype_name(int) const;
    String edeclaration(int) const;
    const String &econfiguration(int) const;
    String &econfiguration(int i)	{return _elements[i]->configuration();}
    int eflags(int i) const		{ return _elements[i]->flags; }
    const String &elandmark(int i) const{ return _elements[i]->landmark(); }

    int get_eindex(const String &name, ElementClassT *, const String &configuration, const String &landmark);
    int get_anon_eindex(const String &name, ElementClassT *, const String &configuration = String(), const String &landmark = String());
    int get_anon_eindex(ElementClassT *, const String &configuration = String(), const String &landmark = String());
    void change_ename(int, const String &);
    void free_element(int);
    void free_dead_elements();

    void set_new_eindex_collector(Vector<int> *v) { _new_eindex_collector=v; }

    int nconnections() const			{ return _conn.size(); }
    const Vector<ConnectionT> &connections() const { return _conn; }
    bool connection_live(int c) const		{ return _conn[c].live(); }
    const Hookup &hookup_from(int c) const	{ return _conn[c].from(); }
    const Hookup &hookup_to(int c) const	{ return _conn[c].to(); }
    const String &hookup_landmark(int c) const	{ return _conn[c].landmark(); }

    void add_tunnel(String, String, const String &, ErrorHandler *);

    bool add_connection(const HookupI &, const HookupI &, const String &landmark = String());
    bool add_connection(int, int, int, int, const String &landmark = String());
    void kill_connection(int);
    void kill_bad_connections();
    void compact_connections();

    void add_requirement(const String &);
    void remove_requirement(const String &);
    const Vector<String> &requirements() const	{ return _requirements; }

    void add_archive(const ArchiveElement &);
    int narchive() const			{ return _archive.size(); }
    int archive_index(const String &s) const	{ return _archive_map[s]; }
    const Vector<ArchiveElement> &archive() const{ return _archive; }
    ArchiveElement &archive(int i)		{ return _archive[i]; }
    const ArchiveElement &archive(int i) const	{ return _archive[i]; }
    ArchiveElement &archive(const String &s);
    const ArchiveElement &archive(const String &s) const;

    bool has_connection(const HookupI &, const HookupI &) const;
    int find_connection(const HookupI &, const HookupI &) const;
    void change_connection_to(int, HookupI);
    void change_connection_from(int, HookupI);
    bool find_connection_from(const HookupI &, HookupI &) const;
    void find_connections_from(const HookupI &, Vector<HookupI> &) const;
    void find_connections_from(const HookupI &, Vector<int> &) const;
    void find_connections_to(const HookupI &, Vector<HookupI> &) const;
    void find_connections_to(const HookupI &, Vector<int> &) const;
    void find_connection_vector_from(int, Vector<int> &) const;
    void find_connection_vector_to(int, Vector<int> &) const;
    void count_ports(Vector<int> &, Vector<int> &) const;

    bool insert_before(const HookupI &, const HookupI &);
    bool insert_after(const HookupI &, const HookupI &);
    bool insert_before(int e, const HookupI &h)	{ return insert_before(HookupI(e, 0), h); }
    bool insert_after(int e, const HookupI &h)	{ return insert_after(HookupI(e, 0), h); }

    void add_components_to(RouterT *, const String &prefix = String()) const;

    void remove_duplicate_connections();
    void remove_dead_elements(ErrorHandler * = 0);

    void remove_compound_elements(ErrorHandler *);
    void remove_tunnels(ErrorHandler * = 0);

    void expand_into(RouterT *, const VariableEnvironment &, ErrorHandler *);
    void flatten(ErrorHandler *);

    void unparse(StringAccum &, const String & = String()) const;
    void unparse_requirements(StringAccum &, const String & = String()) const;
    void unparse_classes(StringAccum &, const String & = String()) const;
    void unparse_declarations(StringAccum &, const String & = String()) const;
    void unparse_connections(StringAccum &, const String & = String()) const;
    String configuration_string() const;

    RouterT *cast_router()		{ return this; }

  private:
  
    struct Pair {
	int from;
	int to;
	Pair() : from(-1), to(-1) { }
	Pair(int f, int t) : from(f), to(t) { }
    };

    struct ElementType {
	ElementClassT *eclass;
	int scope_cookie;
	int prev_name;
	ElementType() : eclass(0) { }
	ElementType(ElementClassT *c, int sc, int pn) : eclass(c), scope_cookie(sc), prev_name(pn) { }
	const String &name() const { return eclass->name(); }
    };

    int _use_count;

    RouterT *_enclosing_scope;
    int _enclosing_scope_cookie;
    int _scope_cookie;
    
    StringMap _etype_map;
    Vector<ElementType> _etypes;

    StringMap _element_name_map;
    Vector<ElementT *> _elements;
    ElementT *_free_element;
    int _real_ecount;
    Vector<int> *_new_eindex_collector;

    Vector<ConnectionT> _conn;
    Vector<Pair> _first_conn;
    int _free_conn;

    Vector<String> _requirements;

    StringMap _archive_map;
    Vector<ArchiveElement> _archive;

    RouterT(const RouterT &);
    RouterT &operator=(const RouterT &);
    
    ElementClassT *get_type(const String &, int scope_cookie) const;
    void update_noutputs(int);
    void update_ninputs(int);
    int add_element(const ElementT &);
    void free_connection(int ci);
    void unlink_connection_from(int ci);
    void unlink_connection_to(int ci);
    void finish_free_elements(Vector<int> &);
    void expand_tunnel(Vector<HookupI> *port_expansions, const Vector<HookupI> &ports, bool is_output, int which, ErrorHandler *) const;
    String interpolate_arguments(const String &, const Vector<String> &) const;

};

class RouterT::iterator { public:
    iterator(RouterT *r)		: _router(r), _idx(0) { }
    operator bool() const		{ return _idx < _router->nelements(); }
    int idx() const			{ return _idx; }
    void operator++(int = 0)		{ _idx++; }
    operator ElementT *() const		{ return _router->element(_idx); }
    ElementT *operator->() const	{ return _router->element(_idx); }
  private:
    RouterT *_router;
    int _idx;
};

class RouterT::live_iterator {
    void step()	{ while (_idx < _router->nelements() && _router->element(_idx)->dead()) _idx++; }
  public:
    live_iterator(RouterT *r)		: _router(r), _idx(0) { step(); }
    operator bool() const		{ return _idx < _router->nelements(); }
    int idx() const			{ return _idx; }
    void operator++(int = 0)		{ _idx++; step(); }
    operator ElementT *() const		{ return _router->element(_idx); }
    ElementT *operator->() const	{ return _router->element(_idx); }
  public:
    RouterT *_router;
    int _idx;
};

class RouterT::type_iterator {
    void step()	{ while (_idx < _router->nelements() && _router->element(_idx)->type() != _type) _idx++; }
  public:
    type_iterator(RouterT *r, ElementClassT *t)	: _router(r), _idx(0), _type(t) { step(); }
    operator bool() const		{ return _idx < _router->nelements(); }
    int idx() const			{ return _idx; }
    void operator++(int = 0)		{ _idx++; step(); }
    operator ElementT *() const		{ return _router->element(_idx); }
    ElementT *operator->() const	{ return _router->element(_idx); }
  public:
    RouterT *_router;
    int _idx;
    ElementClassT *_type;
};


inline RouterT::iterator
RouterT::first_element()
{
    return iterator(this);
}

inline RouterT::live_iterator
RouterT::first_live_element()
{
    return live_iterator(this);
}

inline RouterT::type_iterator
RouterT::first_element(ElementClassT *t)
{
    return type_iterator(this, t);
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

inline int
RouterT::etype_uid(int e) const
{
    return _elements[e]->type_uid();
}

inline String
RouterT::etype_name(int e) const
{
    return _elements[e]->type()->name();
}

inline String
RouterT::edeclaration(int e) const
{
    return _elements[e]->declaration();
}

inline const String &
RouterT::econfiguration(int e) const
{
    return _elements[e]->configuration();
}

inline bool
RouterT::add_connection(int from_idx, int from_port, int to_idx, int to_port,
			const String &landmark)
{
    return add_connection(HookupI(from_idx, from_port), HookupI(to_idx, to_port), landmark);
}

inline bool
RouterT::has_connection(const HookupI &hfrom, const HookupI &hto) const
{
    return find_connection(hfrom, hto) >= 0;
}

inline ArchiveElement &
RouterT::archive(const String &name)
{
    return _archive[_archive_map[name]];
}

inline const ArchiveElement &
RouterT::archive(const String &name) const
{
    return _archive[_archive_map[name]];
}

#endif
