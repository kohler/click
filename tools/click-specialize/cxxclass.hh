#ifndef CXXCLASS_HH
#define CXXCLASS_HH
#include "string.hh"
#include "vector.hh"
#include "hashmap.hh"

String compile_pattern(const String &);

class CxxFunction {

  String _name;
  bool _in_header;
  String _ret_type;
  String _args;
  String _body;
  String _clean_body;

  bool find_expr(const String &, int *, int *, int[10], int[10]) const;

 public:

  CxxFunction()				{ }
  CxxFunction(const String &, bool, const String &, const String &,
	      const String &, const String &);

  String name() const			{ return _name; }
  const String &clean_body() const	{ return _clean_body; }
  
  bool find_expr(const String &) const;
  bool replace_expr(const String &, const String &);
  
};

class CxxClass {

  String _name;

  HashMap<String, int> _fn_map;
  Vector<CxxFunction> _functions;
  Vector<int> _rewritable;
  Vector<int> _reachable_rewritable;

  bool reach(int, Vector<int> &);

 public:

  CxxClass(const String &);

  int nfunctions() const		{ return _functions.size(); }
  CxxFunction *find(const String &);
  CxxFunction *function(int i)		{ return &_functions[i]; }
  
  void defun(const CxxFunction &);

  void mark_reachable_rewritable();

};

class CxxInfo {

  HashMap<String, int> _class_map;
  Vector<CxxClass *> _classes;

  CxxInfo(const CxxInfo &);
  CxxInfo &operator=(const CxxInfo &);

  CxxClass *make_class(const String &);
  
  int parse_function_definition(const String &text, int fn_start_p,
				int paren_p, const String &original,
				CxxClass *cxx_class);
  int parse_class_definition(const String &, int, const String &);
  int parse_class(const String &text, int p, const String &original,
		  CxxClass *cxx_class);
  
 public:

  CxxInfo();
  ~CxxInfo();
  
  void parse_file(const String &);

  CxxClass *find_class(const String &) const;
  
};


inline CxxFunction *
CxxClass::find(const String &name)
{
  int which = _fn_map[name];
  return (which >= 0 ? &_functions[which] : 0);
}

inline CxxClass *
CxxInfo::find_class(const String &name) const
{
  int which = _class_map[name];
  return (which >= 0 ? _classes[which] : 0);
}

#endif
