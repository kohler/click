#ifndef TOOLUTILS_HH
#define TOOLUTILS_HH
#include "userutils.hh"

extern bool ignore_line_directives;
RouterT *read_router_file(const char *, ErrorHandler * = 0, RouterT * = 0);
void write_router_file(RouterT *, FILE *, ErrorHandler * = 0);
int write_router_file(RouterT *, const char *, ErrorHandler * = 0);

class ElementMap {

  HashMap<String, int> _name_map;
  mutable HashMap<String, int> _cxx_name_map;
  Vector<String> _name;
  Vector<String> _cxx_name;
  Vector<String> _header_file;
  Vector<String> _processing_code;

  void create_cxx_name_map(int) const;
  
 public:

  ElementMap();
  ElementMap(const String &);

  int size() const				{ return _name.size(); }
  const String &click_name(int i) const		{ return _name[i]; }
  const String &cxx_name(int i) const		{ return _cxx_name[i]; }
  const String &header_file(int i) const	{ return _header_file[i]; }
  const String &processing_code(int i) const	{ return _processing_code[i]; }
  String processing_code(const String &) const;

  int find(const String &n) const		{ return _name_map[n]; }
  int find_cxx(const String &n) const		{ return _cxx_name_map[n]; }

  void add(const String &, String, String, String);
  void remove(int);
  void remove(const String &n)			{ remove(find(n)); }

  void parse(const String &);
  String unparse() const;
  
};

#endif
