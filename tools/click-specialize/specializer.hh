#ifndef SPECIALIZER_HH
#define SPECIALIZER_HH
#include "cxxclass.hh"
class RouterT;
class ErrorHandler;

String click_to_cxx_name(const String &);
String specialized_click_name(RouterT *, int);

struct ElementTypeInfo {
  String click_name;
  String cxx_name;
  String header_file;
  String includes;
  bool read_source;

  ElementTypeInfo();
};

struct SpecializedClass {
  String click_name;
  String cxx_name;
  CxxClass *cxxc;
  int eindex;
};

class Specializer {

  RouterT *_router;
  int _nelements;
  Vector<int> _ninputs;
  Vector<int> _noutputs;
  Vector<int> _specialize;

  HashMap<String, int> _specializing_classes;
  Vector<int> _specialize_like;
  
  HashMap<String, int> _etinfo_map;
  Vector<ElementTypeInfo> _etinfo;
  HashMap<String, int> _header_file_map;

  Vector<SpecializedClass> _specials;

  Vector<String> _specfunction_names;
  Vector<String> _specfunction_symbols;
  
  CxxInfo _cxxinfo;

  const String &enew_click_type(int) const;
  const String &enew_cxx_type(int) const;
  
  void read_source(ElementTypeInfo &, ErrorHandler *);
  int check_specialize(int, ErrorHandler *);
  void create_class(SpecializedClass &);
  void do_simple_action(SpecializedClass &);
  void create_connector_methods(SpecializedClass &);
  String emangle(int, bool) const;

  void output_includes(const ElementTypeInfo &, StringAccum &);

 public:

  Specializer(RouterT *);

  ElementTypeInfo &type_info(const String &);
  const ElementTypeInfo &type_info(const String &) const;
  ElementTypeInfo &etype_info(int);
  const ElementTypeInfo &etype_info(int) const;
  void add_type_info(const String &, const String &, const String & =String());
  void parse_elementmap(const String &);

  void set_specializing_classes(const HashMap<String, int> &);
  int set_specialize_like(String, String, ErrorHandler *);

  void specialize(ErrorHandler *);
  void fix_elements();
  
  void output(StringAccum &);
  void output_package(const String &, StringAccum &);
  
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
