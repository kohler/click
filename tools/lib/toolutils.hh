#ifndef TOOLUTILS_HH
#define TOOLUTILS_HH
#include <click/userutils.hh>
class Bitvector;
class RouterT;

extern bool ignore_line_directives;
RouterT *read_router_file(const char *, ErrorHandler *);
RouterT *read_router_file(const char *, bool, ErrorHandler *);
RouterT *read_router_file(const char *, RouterT *, ErrorHandler *);
RouterT *read_router_file(const char *, bool, RouterT *, ErrorHandler *);
void write_router_file(RouterT *, FILE *, ErrorHandler * = 0);
int write_router_file(RouterT *, const char *, ErrorHandler * = 0);

class ElementMap { public:

  struct Elt {
    String name;
    String cxx;
    String header_file;
    String source_file;
    String processing_code;
    String flow_code;
    String flags;
    String requirements;
    String provisions;
    int def_index;
    int driver;
    int name_next;
    int cxx_next;

    Elt();
    String *component(int);
  };

  static const int DRIVER_LINUXMODULE = 0;
  static const int DRIVER_USERLEVEL = 1;
  static const int NDRIVERS = 2;
  static const char *driver_name(int);
  static const char *driver_requirement(int);
  
  ElementMap();
  ElementMap(const String &);

  int size() const				{ return _e.size(); }
  bool empty() const				{ return _e.size() == 1; }
  
  const Elt &elt(int i) const			{ return _e[i]; }
  const String &name(int i) const		{ return _e[i].name; }
  const String &cxx(int i) const		{ return _e[i].cxx; }
  const String &header_file(int i) const	{ return _e[i].header_file; }
  const String &source_file(int i) const	{ return _e[i].source_file; }
  const String &source_directory(int i) const;
  const String &processing_code(int i) const	{return _e[i].processing_code;}
  const String &flow_code(int i) const		{ return _e[i].flow_code; }
  const String &flags(int i) const		{ return _e[i].flags; }
  const String &requirements(int i) const	{ return _e[i].requirements; }
  bool requires(int i, const String &) const;
  const String &provisions(int i) const		{ return _e[i].provisions; }
  bool provides(int i, const String &) const;
  const String &package(int i) const;
  int name_next(int i) const			{ return _e[i].name_next; }
  int cxx_next(int i) const			{ return _e[i].cxx_next; }
  int definition_index(int i) const		{ return _e[i].def_index; }
  const String &processing_code(const String &) const;
  const String &flow_code(const String &) const;
  int flag_value(int i, int flag) const;
  int flag_value(const String &, int flag) const;

  int find(const String &n) const		{ return _name_map[n]; }
  int find_cxx(const String &n) const		{ return _cxx_map[n]; }
  HashMap<String, int>::Iterator first() const	{ return _name_map.first(); }

  int add(const Elt &);
  int add(const String &name, const String &cxx, const String &header_file,
	  const String &processing_code, const String &flow_code,
	  const String &flags,
	  const String &requirements, const String &provisions);
  int add(const String &name, const String &cxx, const String &header_file,
	  const String &processing_code, const String &flow_code);
  void remove(int);
  void remove(const String &n)			{ remove(find(n)); }

  const String &def_source_directory(int i) const { return _def_srcdir[i]; }
  const String &def_compile_flags(int i) const { return _def_compile_flags[i];}
  
  void parse(const String &data);
  void parse(const String &data, const String &package_name);
  bool parse_default_file(const String &default_path, ErrorHandler *);
  bool parse_requirement_files(RouterT *, const String &default_path, ErrorHandler *, String *not_found = 0);
  bool parse_all_files(RouterT *, const String &default_path, ErrorHandler *);
  static void report_file_not_found(String default_path, bool found_default, ErrorHandler *);
  String unparse() const;
  
  void map_indexes(const RouterT *, Vector<int> &, ErrorHandler * = 0) const;
  
  bool driver_indifferent(const Vector<int> &map_indexes) const;
  bool driver_compatible(const Vector<int> &map_indexes, int driver) const;
  bool driver_compatible(const RouterT *, int driver, ErrorHandler * =0) const;
  void limit_driver(int driver);
  
 private:

  Vector<Elt> _e;
  HashMap<String, int> _name_map;
  HashMap<String, int> _cxx_map;

  Vector<String> _def_srcdir;
  Vector<String> _def_compile_flags;
  Vector<String> _def_package;

  int get_driver(const String &);

  enum {
    D_NONE,
    D_CLASS, D_CXX_CLASS, D_HEADER_FILE, D_PROCESSING,
    D_FLOW_CODE, D_FLAGS, D_REQUIREMENTS, D_PROVISIONS,
    D_SOURCE_FILE,
    D_FIRST_DEFAULT = D_CLASS, D_LAST_DEFAULT = D_PROVISIONS
  };
  static int parse_component(const String &);
  
};

inline
ElementMap::Elt::Elt()
  : def_index(0), driver(-1), name_next(0), cxx_next(0)
{
}

#endif
