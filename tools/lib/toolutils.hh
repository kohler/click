#ifndef TOOLUTILS_HH
#define TOOLUTILS_HH
#include "userutils.hh"
class Bitvector;
class RouterT;

extern bool ignore_line_directives;
RouterT *read_router_file(const char *, ErrorHandler * = 0, RouterT * = 0);
void write_router_file(RouterT *, FILE *, ErrorHandler * = 0);
int write_router_file(RouterT *, const char *, ErrorHandler * = 0);

class ElementMap {

  Vector<String> _name;
  Vector<String> _cxx_name;
  Vector<String> _header_file;
  Vector<String> _processing_code;
  Vector<Bitvector *> _requirements;
  Vector<Bitvector *> _provisions;

  HashMap<String, int> _name_map;
  HashMap<String, int> _cxx_name_map;
  Vector<int> _name_next;
  Vector<int> _cxx_name_next;

  HashMap<String, int> _requirement_map;
  Vector<String> _requirement_name;
  int _nrequirements;

  void unparse_requirements(Bitvector *, StringAccum &) const;
  
 public:

  ElementMap();
  ElementMap(const String &);
  ~ElementMap();

  int size() const				{ return _name.size(); }
  const String &click_name(int i) const		{ return _name[i]; }
  const String &cxx_name(int i) const		{ return _cxx_name[i]; }
  const String &header_file(int i) const	{ return _header_file[i]; }
  const String &processing_code(int i) const	{ return _processing_code[i]; }
  const Bitvector *requirements(int i) const	{ return _requirements[i]; }
  const Bitvector *provisions(int i) const	{ return _provisions[i]; }
  String processing_code(const String &) const;

  int find(const String &n) const		{ return _name_map[n]; }
  int find_cxx(const String &n) const		{ return _cxx_name_map[n]; }

  int add(const String &, String, String, String);
  void add_requirement(int, int);
  void add_provision(int, int);
  void remove(int);
  void remove(const String &n)			{ remove(find(n)); }

  int requirement(const String &);
  
  void parse(const String &);
  String unparse() const;
  
};

#endif
