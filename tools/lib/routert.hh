#ifndef ROUTERT_HH
#define ROUTERT_HH
#include "elementt.hh"
#include "error.hh"
#include "hashmap.hh"
#include "archive.hh"

class RouterT : public ElementClassT {

  struct Pair {
    int from;
    int to;
    Pair() : from(-1), to(-1) { }
    Pair(int f, int t) : from(f), to(t) { }
  };

  RouterT *_enclosing_scope;
  Vector<String> _formals;
  
  HashMap<String, int> _element_type_map;
  Vector<String> _element_type_names;
  Vector<ElementClassT *> _element_classes;
  
  HashMap<String, int> _element_name_map;
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

  HashMap<String, int> _require_map;

  HashMap<String, int> _archive_map;
  Vector<ArchiveElement> _archive;

  int add_element(const ElementT &);
  int prev_connection_from(int, int) const;
  int prev_connection_to(int, int) const;
  void finish_remove_elements(Vector<int> &, ErrorHandler *);
  void finish_free_elements(Vector<int> &);
  void finish_remove_element_types(Vector<int> &);
  void expand_tunnel(Vector<Hookup> *, bool is_input, int magice, int which,
		     Vector<Hookup> &results) const;
  String interpolate_arguments(const String &, const Vector<String> &) const;

 public:

  static const int TUNNEL_TYPE = 0;
  static const int UPREF_TYPE = 1;
  
  RouterT(RouterT * = 0);
  RouterT(const RouterT &);
  virtual ~RouterT();

  void add_formal(const String &n)	{ _formals.push_back(n); }
  
  int ntypes() const			{ return _element_classes.size(); }
  const String &type_name(int i) const	{ return _element_type_names[i]; }
  ElementClassT *type_class(int i) const { return _element_classes[i]; }
  int type_index(const String &s) const { return _element_type_map[s]; }
  ElementClassT *find_type_class(const String &) const;
  int get_type_index(const String &);
  int get_type_index(const String &, ElementClassT *);
  int set_type_index(const String &, ElementClassT *);
  int get_anon_type_index(const String &, ElementClassT *);
  void get_types_from(const RouterT *);
  int unify_type_indexes(const RouterT *);

  int nelements() const			{ return _elements.size(); }
  int real_element_count() const	{ return _real_ecount; }
  int eindex(const String &s) const	{ return _element_name_map[s]; }
  const ElementT &element(int i) const	{ return _elements[i]; }
  ElementT &element(int i)		{ return _elements[i]; }
  String ename(int) const;
  String ename_upref(int) const;
  int etype(int) const;
  String etype_name(int) const;
  ElementClassT *etype_class(int) const;
  const String &econfiguration(int) const;
  String &econfiguration(int i)		{ return _elements[i].configuration; }
  int eflags(int i) const		{ return _elements[i].flags; }
  const String &elandmark(int i) const	{ return _elements[i].landmark; }
  
  int get_eindex(const String &name, int etype_index, const String &configuration, const String &landmark);
  int get_anon_eindex(const String &name, int ftype_index, const String &configuration = String(), const String &landmark = String());
  int get_anon_eindex(int ftype_index, const String &configuration = String(), const String &landmark = String());

  void set_new_eindex_collector(Vector<int> *v) { _new_eindex_collector = v; }
  
  int nhookup() const				{ return _hookup_from.size(); }
  const Vector<Hookup> &hookup_from() const	{ return _hookup_from; }
  const Vector<Hookup> &hookup_to() const	{ return _hookup_to; }
  const String &hookup_landmark(int i) const	{ return _hookup_landmark[i]; }
 
  void add_tunnel(String, String, const String &, ErrorHandler *);
  
  bool add_connection(const Hookup &, const Hookup &, const String &landmark = String());
  bool add_connection(int fidx, int fport, int tport, int tidx);
  void kill_connection(int);
  void compact_connections();
  void change_connection_to(int, const Hookup &);
  void change_connection_from(int, const Hookup &);

  void add_requirement(const String &);
  void remove_requirement(const String &);
  const HashMap<String, int> &requirement_map() const { return _require_map; }

  void add_archive(const ArchiveElement &);
  int narchive() const				{ return _archive.size(); }
  int archive_index(const String &s) const	{ return _archive_map[s]; }
  const Vector<ArchiveElement> &archive() const	{ return _archive; }
  ArchiveElement &archive(int i)		{ return _archive[i]; }
  ArchiveElement &archive(const String &s);
  
  bool has_connection(const Hookup &, const Hookup &) const;
  void find_connections_from(const Hookup &, Vector<Hookup> &) const;
  void find_connections_to(const Hookup &, Vector<Hookup> &) const;
  void find_connection_vector_from(int, Vector<int> &) const;
  void find_connection_vector_to(int, Vector<int> &) const;
  void count_ports(Vector<int> &, Vector<int> &) const;

  bool insert_before(int fidx, const Hookup &);
  bool insert_after(int fidx, const Hookup &);
  
  void add_components_to(RouterT *, const String &prefix = String()) const;

  int expand_into(RouterT *, int, RouterT *, const RouterScope &, ErrorHandler *);
  
  void check() const;
  void remove_unused_element_types();
  
  void remove_duplicate_connections();
  
  void free_blank_elements();
  void remove_blank_elements(ErrorHandler * = 0);
  
  void remove_compound_elements(ErrorHandler *);
  void remove_tunnels();
  void remove_unresolved_uprefs(ErrorHandler *);

  void flatten(ErrorHandler *);

  void compound_declaration_string(StringAccum &, const String &, const String &);
  void configuration_string(StringAccum &, const String & = String()) const;
  String configuration_string() const;

  RouterT *cast_router()		{ return this; }

};

class RouterScope {

  String _prefix;
  Vector<String> _formals;
  Vector<String> _values;

 public:
  
  RouterScope()				{ }
  RouterScope(const RouterScope &, const String &suffix);

  operator bool() const			{ return _formals.size() != 0; }
  const String &prefix() const		{ return _prefix; }
  
  void combine(const Vector<String> &, const Vector<String> &);
  
  String interpolate(const String &) const;
  
};


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

inline ElementClassT *
RouterT::etype_class(int idx) const
{
  return type_class(_elements[idx].type);
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

inline ArchiveElement &
RouterT::archive(const String &name)
{
  return _archive[_archive_map[name]];
}

#endif
