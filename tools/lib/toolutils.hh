#ifndef TOOLUTILS_HH
#define TOOLUTILS_HH
#include "userutils.hh"
class Bitvector;
class RouterT;

extern bool ignore_line_directives;
RouterT *read_router_file(const char *, ErrorHandler *);
RouterT *read_router_file(const char *, bool, ErrorHandler *);
RouterT *read_router_file(const char *, RouterT *, ErrorHandler *);
RouterT *read_router_file(const char *, bool, RouterT *, ErrorHandler *);
void write_router_file(RouterT *, FILE *, ErrorHandler * = 0);
int write_router_file(RouterT *, const char *, ErrorHandler * = 0);

class ElementMap {

  Vector<String> _name;
  Vector<String> _cxx;
  Vector<String> _header_file;
  Vector<String> _processing_code;
  Vector<String> _requirements;
  Vector<String> _provisions;
  Vector<int> _driver;

  HashMap<String, int> _name_map;
  HashMap<String, int> _cxx_map;
  Vector<int> _name_next;
  Vector<int> _cxx_next;

  void set_driver(int, const String &);
  
 public:

  static const int DRIVER_LINUXMODULE = 0;
  static const int DRIVER_USERLEVEL = 1;
  static const int NDRIVERS = 2;

  ElementMap();
  ElementMap(const String &);

  int size() const				{ return _name.size(); }
  const String &name(int i) const		{ return _name[i]; }
  const String &cxx(int i) const		{ return _cxx[i]; }
  const String &header_file(int i) const	{ return _header_file[i]; }
  const String &processing_code(int i) const	{ return _processing_code[i]; }
  const String &requirements(int i) const	{ return _requirements[i]; }
  const String &provisions(int i) const		{ return _provisions[i]; }
  int name_next(int i) const			{ return _name_next[i]; }
  int cxx_next(int i) const			{ return _cxx_next[i]; }
  String processing_code(const String &) const;

  int find(const String &n) const		{ return _name_map[n]; }
  int find_cxx(const String &n) const		{ return _cxx_map[n]; }

  int add(const String &name, const String &cxx, const String &header_file,
	  const String &processing_code,
	  const String &requirements, const String &provisions);
  int add(const String &name, const String &cxx, const String &header_file,
	  const String &processing_code);
  void remove(int);
  void remove(const String &n)			{ remove(find(n)); }

  void parse(const String &);
  String unparse() const;
  
  void map_indexes(const RouterT *, Vector<int> &, ErrorHandler *) const;
  
  bool driver_indifferent(const Vector<int> &map_indexes) const;
  bool driver_compatible(const Vector<int> &map_indexes, int driver) const;
  void limit_driver(int driver);
  
};

#endif
