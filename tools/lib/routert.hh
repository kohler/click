#ifndef ROUTERT_HH
#define ROUTERT_HH
#include "elementt.hh"
#include <click/error.hh>
#include <click/hashmap.hh>
#include <click/archive.hh>
typedef HashMap<String, int> StringMap;

class RouterT { public:

  enum { TUNNEL_TYPE = 0, FIRST_REAL_TYPE = 1 };
  
  RouterT(RouterT * = 0);
  RouterT(const RouterT &);
  virtual ~RouterT();

  void use()				{ _use_count++; }
  void unuse()				{ if (--_use_count <= 0) delete this; }
  
  void check() const;
  bool is_flat() const;
  
  int ntypes() const			{ return _element_classes.size(); }
  const String &type_name(int i) const	{ return _element_type_names[i]; }
  ElementClassT *type_class(int i) const { return _element_classes[i]; }
  ElementClassT *type_class(const String &) const;
  int type_index(const String &s) const { return _element_type_map[s]; }
  int get_type_index(const String &, ElementClassT * = 0);
  int add_type_index(const String &, ElementClassT *);
  int get_anon_type_index(const String &, ElementClassT *);
  void get_types_from(const RouterT *);
  int unify_type_indexes(const RouterT *);

  int nelements() const			{ return _elements.size(); }
  int real_element_count() const	{ return _real_ecount; }
  int eindex(const String &s) const	{ return _element_name_map[s]; }
  const ElementT &element(int i) const	{ return _elements[i]; }
  ElementT &element(int i)		{ return _elements[i]; }
  bool elive(int ei) const		{ return _elements[ei].live(); }
  bool edead(int ei) const		{ return _elements[ei].dead(); }
  String ename(int) const;
  int etype(int) const;
  String etype_name(int) const;
  String edeclaration(int) const;
  String edeclaration(const ElementT &) const;
  ElementClassT *etype_class(int) const;
  const String &econfiguration(int) const;
  String &econfiguration(int i)		{ return _elements[i].configuration; }
  int eflags(int i) const		{ return _elements[i].flags; }
  const String &elandmark(int i) const	{ return _elements[i].landmark; }
  
  int get_eindex(const String &name, int etype_index, const String &configuration, const String &landmark);
  int get_anon_eindex(const String &name, int ftype_index, const String &configuration = String(), const String &landmark = String());
  int get_anon_eindex(int ftype_index, const String &configuration = String(), const String &landmark = String());
  void change_ename(int, const String &);
  void free_element(int);
  void kill_element(int i)			{ _elements[i].type = -1; }
  void free_dead_elements();

  void set_new_eindex_collector(Vector<int> *v) { _new_eindex_collector = v; }
  
  int nhookup() const				{ return _hookup_from.size(); }
  const Vector<Hookup> &hookup_from() const	{ return _hookup_from; }
  const Hookup &hookup_from(int i) const	{ return _hookup_from[i]; }
  const Vector<Hookup> &hookup_to() const	{ return _hookup_to; }
  const Hookup &hookup_to(int i) const		{ return _hookup_to[i]; }
  const Vector<String> &hookup_landmark() const	{ return _hookup_landmark; }
  const String &hookup_landmark(int i) const	{ return _hookup_landmark[i]; }
  bool hookup_live(int i) const		{ return _hookup_from[i].live(); }
 
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
  int narchive() const				{ return _archive.size(); }
  int archive_index(const String &s) const	{ return _archive_map[s]; }
  const Vector<ArchiveElement> &archive() const	{ return _archive; }
  ArchiveElement &archive(int i)		{ return _archive[i]; }
  ArchiveElement &archive(const String &s);
  
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
  int ninputs(int) const;
  int noutputs(int) const;

  bool insert_before(const Hookup &, const Hookup &);
  bool insert_after(const Hookup &, const Hookup &);
  bool insert_before(int e, const Hookup &h)	{ return insert_before(Hookup(e, 0), h); }
  bool insert_after(int e, const Hookup &h)	{ return insert_after(Hookup(e, 0), h); }
  
  void add_components_to(RouterT *, const String &prefix = String()) const;

  void remove_unused_element_types();
  void remove_duplicate_connections();
  void remove_dead_elements(ErrorHandler * = 0);
  
  void remove_compound_elements(ErrorHandler *);
  void remove_tunnels(ErrorHandler * = 0);

  void expand_into(RouterT *, const VariableEnvironment &, ErrorHandler *);
  void flatten(ErrorHandler *);

  void configuration_string(StringAccum &, const String & = String()) const;
  String configuration_string() const;

  RouterT *cast_router()		{ return this; }

 private:
  
  struct Pair {
    int from;
    int to;
    Pair() : from(-1), to(-1) { }
    Pair(int f, int t) : from(f), to(t) { }
  };

  int _use_count;
  
  StringMap _element_type_map;
  Vector<String> _element_type_names;
  Vector<ElementClassT *> _element_classes;
  
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

  int add_element(const ElementT &);
  int prev_connection_from(int, int) const;
  int prev_connection_to(int, int) const;
  void finish_remove_elements(Vector<int> &, ErrorHandler *);
  void finish_free_elements(Vector<int> &);
  void finish_remove_element_types(Vector<int> &);
  void expand_tunnel(Vector<Hookup> *port_expansions, const Vector<Hookup> &ports, bool is_output, int which, ErrorHandler *) const;
  String interpolate_arguments(const String &, const Vector<String> &) const;

};


inline ElementClassT *
RouterT::type_class(const String &n) const
{
  int i = type_index(n);
  return (i < 0 ? 0 : _element_classes[i]);
}

inline String
RouterT::ename(int idx) const
{
  return _elements[idx].name;
}

inline int
RouterT::etype(int idx) const
{
  return _elements[idx].type;
}

inline String
RouterT::etype_name(int idx) const
{
  return type_name(_elements[idx].type);
}

inline String
RouterT::edeclaration(int idx) const
{
  return ename(idx) + " :: " + etype_name(idx);
}

inline String
RouterT::edeclaration(const ElementT &e) const
{
  return e.name + " :: " + type_name(e.type);
}

inline ElementClassT *
RouterT::etype_class(int idx) const
{
  int t = _elements[idx].type;
  return (t >= 0 ? _element_classes[t] : 0);
}

inline const String &
RouterT::econfiguration(int i) const
{
  return _elements[i].configuration;
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

#endif
