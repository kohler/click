#ifndef CXXCLASS_HH
#define CXXCLASS_HH
#include <click/string.hh>
#include <click/vector.hh>
#include <click/hashtable.hh>
class StringAccum;

String compile_pattern(const String &);

class CxxFunction {

  String _name;
  bool _in_header;
  bool _from_header_file;
  bool _alive;
  String _ret_type;
  String _args;
  String _body;
  String _clean_body;

  bool find_expr(const String &, int *, int *, int[10], int[10]) const;

 public:

  static bool parsing_header_file;

  CxxFunction()				: _alive(false) { }
  CxxFunction(const String &, bool, const String &, const String &,
	      const String &, const String &);

  String name() const			{ return _name; }
  bool alive() const			{ return _alive; }
  bool in_header() const		{ return _in_header; }
  bool from_header_file() const		{ return _from_header_file; }
  const String &ret_type() const	{ return _ret_type; }
  const String &args() const		{ return _args; }
  const String &body() const		{ return _body; }
  const String &clean_body() const	{ return _clean_body; }

  void set_body(const String &b)	{ _body = b; _clean_body = String(); }
  void kill()				{ _alive = false; }
  void unkill()				{ _alive = true; }

  bool find_expr(const String &) const;
  bool replace_expr(const String &, const String &);

};

class CxxClass {

  String _name;
  Vector<CxxClass *> _parents;

  HashTable<String, int> _fn_map;
  Vector<CxxFunction> _functions;
  Vector<int> _has_push;
  Vector<int> _has_pull;
  Vector<int> _should_rewrite;

  bool reach(int, Vector<int> &);

 public:

  CxxClass(const String &);

  const String &name() const		{ return _name; }
  int nparents() const			{ return _parents.size(); }
  CxxClass *parent(int i) const		{ return _parents[i]; }

  int nfunctions() const		{ return _functions.size(); }
  CxxFunction *find(const String &);
  CxxFunction &function(int i)		{ return _functions[i]; }

  CxxFunction &defun(const CxxFunction &);
  void add_parent(CxxClass *);

  bool find_should_rewrite();
  bool should_rewrite(int i) const	{ return _should_rewrite[i]; }

  void header_text(StringAccum &) const;
  void source_text(StringAccum &) const;

};

class CxxInfo { public:

  CxxInfo();
  ~CxxInfo();

  void parse_file(const String &, bool header, String * = 0);

  CxxClass *find_class(const String &) const;
  CxxClass *make_class(const String &);

 private:

  HashTable<String, int> _class_map;
  Vector<CxxClass *> _classes;

  CxxInfo(const CxxInfo &);
  CxxInfo &operator=(const CxxInfo &);

  int parse_function_definition(const String &text, int fn_start_p,
				int paren_p, const String &original,
				CxxClass *cxx_class);
  int parse_class_definition(const String &, int, const String &);
  int parse_class(const String &text, int p, const String &original,
		  CxxClass *cxx_class);

};


inline CxxFunction *
CxxClass::find(const String &name)
{
    int which = _fn_map.get(name);
    return (which >= 0 ? &_functions[which] : 0);
}

inline CxxClass *
CxxInfo::find_class(const String &name) const
{
    int which = _class_map.get(name);
    return (which >= 0 ? _classes[which] : 0);
}

#endif
