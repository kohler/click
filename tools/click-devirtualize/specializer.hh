#ifndef SPECIALIZER_HH
#define SPECIALIZER_HH
#include "cxxclass.hh"
class RouterT;
class ErrorHandler;
class ElementMap;
class Signatures;

String click_to_cxx_name(const String &);
String specialized_click_name(RouterT *, int);

struct ElementTypeInfo {
  String click_name;
  String cxx_name;
  String header_file;
  String source_directory;
  String found_header_file;
  String includes;
  bool read_source;

  ElementTypeInfo();
  void locate_header_file(RouterT *, ErrorHandler *);
};

struct SpecializedClass {
  String old_click_name;
  String click_name;
  String cxx_name;
  CxxClass *cxxc;
  int eindex;

  SpecializedClass() : cxxc(0), eindex(-3)	{ }
  bool special() const				{ return cxxc != 0; }
};

class Specializer { public:

  Specializer(RouterT *, const ElementMap &);

  ElementTypeInfo &type_info(const String &);
  const ElementTypeInfo &type_info(const String &) const;
  ElementTypeInfo &etype_info(int);
  const ElementTypeInfo &etype_info(int) const;
  void add_type_info(const String &click_name, const String &cxx_name,
		     const String &header_file, const String &source_dir);

  void specialize(const Signatures &, ErrorHandler *);
  void fix_elements();

  int nspecials() const				{ return _specials.size(); }
  const SpecializedClass &special(int i) const	{ return _specials[i]; }
  
  void output(StringAccum &);
  void output_package(const String &, StringAccum &);
  void output_new_elementmap(const ElementMap &, ElementMap &, const String &,
			     const String &requirements) const;
  
 private:

  static const int SPCE_NOT_DONE = -2;
  static const int SPCE_NOT_SPECIAL = -1;
  
  RouterT *_router;
  int _nelements;
  Vector<int> _ninputs;
  Vector<int> _noutputs;
  Vector<int> _specialize;

  HashMap<String, int> _etinfo_map;
  Vector<ElementTypeInfo> _etinfo;
  HashMap<String, int> _header_file_map;
  HashMap<String, int> _parsed_sources;

  Vector<SpecializedClass> _specials;

  CxxInfo _cxxinfo;

  const String &enew_cxx_type(int) const;
  
  void parse_source_file(ElementTypeInfo &, bool, String *);
  void read_source(ElementTypeInfo &, ErrorHandler *);
  void check_specialize(int, ErrorHandler *);
  bool create_class(SpecializedClass &);
  void do_simple_action(SpecializedClass &);
  void create_connector_methods(SpecializedClass &);

  void output_includes(const ElementTypeInfo &, StringAccum &);

};

inline
ElementTypeInfo::ElementTypeInfo()
  : read_source(false)
{
}

inline ElementTypeInfo &
Specializer::type_info(const String &name)
{
  return _etinfo[_etinfo_map[name]];
}

inline const ElementTypeInfo &
Specializer::type_info(const String &name) const
{
  return _etinfo[_etinfo_map[name]];
}

#endif
