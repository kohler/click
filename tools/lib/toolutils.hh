#ifndef TOOLUTILS_HH
#define TOOLUTILS_HH
#include "archive.hh"
#include <stdio.h>
class RouterT;
class ErrorHandler;
class ArchiveElement;

String file_string(FILE *, ErrorHandler * = 0);
String file_string(const char *, ErrorHandler * = 0);
RouterT *read_router_file(const char *, ErrorHandler * = 0, RouterT * = 0);
void write_router_file(RouterT *, FILE *, ErrorHandler * = 0);
int write_router_file(RouterT *, const char *, ErrorHandler * = 0);

String unique_tmpnam(const String &, ErrorHandler * = 0);
void remove_file_on_exit(const String &);
String clickpath_find_file(const String &filename, const char *subdir,
			   String default_path, ErrorHandler * = 0);
String click_mktmpdir(ErrorHandler * = 0);

void parse_tabbed_lines(const String &, bool allow_spaces, int, ...);

ArchiveElement init_archive_element(const String &, int);


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

  void add(const String &, const String &, String, String);
  void remove(int);
  void remove(const String &n)			{ remove(find(n)); }

  void parse(const String &);
  String unparse() const;
  
};

#endif
