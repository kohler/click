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

    int nelements() const		{ return _elements.size(); }
    int real_element_count() const	{ return _real_ecount; }
    int eindex(const String &s) const	{ return _element_name_map[s]; }
    const ElementT &element(int i) const{ return _elements[i]; }
    ElementT &element(int i)		{ return _elements[i]; }
    bool elive(int ei) const		{ return _elements[ei].live(); }
    bool edead(int ei) const		{ return _elements[ei].dead(); }
    String ename(int) const;
    ElementClassT *etype(int) const;
    int etype_uid(int) const;
    String etype_name(int) const;
    String edeclaration(int) const;
    const String &econfiguration(int) const;
    String &econfiguration(int i)	{ return _elements[i].configuration();}
    int eflags(int i) const		{ return _elements[i].flags; }
    const String &elandmark(int i) const{ return _elements[i].landmark(); }
    int eninputs(int i) const		{ return _elements[i].ninputs(); }
    int enoutputs(int i) const		{ return _elements[i].noutputs(); }

    int get_eindex(const String &name, ElementClassT *, const String &configuration, const String &landmark);
    int get_anon_eindex(const String &name, ElementClassT *, const String &configuration = String(), const String &landmark = String());
    int get_anon_eindex(ElementClassT *, const String &configuration = String(), const String &landmark = String());
    void change_ename(int, const String &);
    void free_element(int);
    void kill_element(int i)		{ _elements[i].kill(); }
    void free_dead_elements();

    void set_new_eindex_collector(Vector<int> *v) { _new_eindex_collector=v; }
  
    int nhookup() const				{ return _hookup_from.size(); }
    const Vector<Hookup> &hookup_from() const	{ return _hookup_from; }
    const Hookup &hookup_from(int c) const	{ return _hookup_from[c]; }
    const Vector<Hookup> &hookup_to() const	{ return _hookup_to; }
    const Hookup &hookup_to(int c) const	{ return _hookup_to[c]; }
    const Vector<String> &hookup_landmark() const { return _hookup_landmark; }
    const String &hookup_landmark(int c) const	{ return _hookup_landmark[c]; }
    bool hookup_live(int c) const	{ return _hookup_from[c].live(); }

    void add_tunnel(String, String, const String &, ErrorHandler *);
  
    bool add_connection(Hookup, Hookup, const String &landmark = String());
    bool add_connection(int fidx, int fport, int tport, int tidx);
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

    bool has_connection(const Hookup &, const Hookup &) const;
    int find_connection(const Hookup &, const Hookup &) const;
    void change_connection_to(int, Hookup);
    void change_connection_from(int, Hookup);
    bool find_connection_from(const Hookup &, Hookup &) const;
    void find_connections_from(const Hookup &, Vector<Hookup> &) const;
    void find_connections_from(const Hookup &, Vector<int> &) const;
    void find_connections_to(const Hookup &, Vector<Hookup> &) const;
    void find_connections_to(const Hookup &, Vector<int> &) const;
    void find_connection_vector_from(int, Vector<int> &) const;
    void find_connection_vector_to(int, Vector<int> &) const;
    void count_ports(Vector<int> &, Vector<int> &) const;

    bool insert_before(const Hookup &, const Hookup &);
    bool insert_after(const Hookup &, const Hookup &);
    bool insert_before(int e, const Hookup &h)	{ return insert_before(Hookup(e, 0), h); }
    bool insert_after(int e, const Hookup &h)	{ return insert_after(Hookup(e, 0), h); }

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
    Vector<ElementT> _elements;	// contains types
    int _free_element;
    int _real_ecount;
    Vector<int> *_new_eindex_collector;

    Vector<Hookup> _hookup_from;
    Vector<Hookup> _hookup_to;
    Vector<String> _hookup_landmark;
    Vector<Pair> _hookup_next;
    Vector<Pair> _hookup_first;
    int _free_hookup;

    Vector<String> _requirements;

    StringMap _archive_map;
    Vector<ArchiveElement> _archive;

    RouterT(const RouterT &);
    
    ElementClassT *get_type(const String &, int scope_cookie) const;
    void update_noutputs(int);
    void update_ninputs(int);
    int add_element(const ElementT &);
    void free_connection(int ci);
    void unlink_connection_from(int ci);
    void unlink_connection_to(int ci);
    void finish_remove_elements(Vector<int> &, ErrorHandler *);
    void finish_free_elements(Vector<int> &);
    void expand_tunnel(Vector<Hookup> *port_expansions, const Vector<Hookup> &ports, bool is_output, int which, ErrorHandler *) const;
    String interpolate_arguments(const String &, const Vector<String> &) const;

};


inline String
RouterT::ename(int e) const
{
    return _elements[e].name;
}

inline ElementClassT *
RouterT::etype(int e) const
{
    return _elements[e].type();
}

inline int
RouterT::etype_uid(int e) const
{
    return _elements[e].type_uid();
}

inline String
RouterT::etype_name(int e) const
{
    return _elements[e].type()->name();
}

inline String
RouterT::edeclaration(int e) const
{
    return _elements[e].declaration();
}

inline const String &
RouterT::econfiguration(int e) const
{
    return _elements[e].configuration();
}

inline bool
RouterT::add_connection(int fidx, int fport, int tport, int tidx)
{
    return add_connection(Hookup(fidx, fport), Hookup(tidx, tport));
}

inline bool
RouterT::has_connection(const Hookup &hfrom, const Hookup &hto) const
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
