#ifndef ELEMENTT_HH
#define ELEMENTT_HH
#include <click/string.hh>
#include <stddef.h>
#include <click/vector.hh>
class RouterT;
class VariableEnvironment;
class ErrorHandler;
class StringAccum;
class CompoundElementClassT;

struct ElementT {
  
  int type;
  String name;
  String configuration;
  int tunnel_input;
  int tunnel_output;
  String landmark;
  int flags;

  ElementT();
  ElementT(const String &, int, const String &, const String & = String());

  bool live() const			{ return type >= 0; }
  bool dead() const			{ return type < 0; }

  void kill()				{ type = -1; }
  
};

struct Hookup {
  
  int idx;
  int port;
  
  Hookup()				: idx(-1) { }
  Hookup(int i, int p)			: idx(i), port(p) { }

  bool live() const			{ return idx >= 0; }
  bool dead() const			{ return idx < 0; }
  
  int index_in(const Vector<Hookup> &, int start = 0) const;
  int force_index_in(Vector<Hookup> &, int start = 0) const;

  static int sorter(const void *, const void *);
  static void sort(Vector<Hookup> &);
  
};

class ElementClassT { public:

  ElementClassT();
  virtual ~ElementClassT()		{ }

  void use()				{ _use_count++; }
  void unuse()				{ if (--_use_count <= 0) delete this; }

  static int expand_element(RouterT *, int, RouterT *, const VariableEnvironment &, ErrorHandler *);
  
  virtual ElementClassT *find_relevant_class(int ninputs, int noutputs, const Vector<String> &);
  virtual void report_signatures(const String &, String, ErrorHandler *);
  virtual int complex_expand_element(RouterT *, int, const String &, Vector<String> &, RouterT *, const VariableEnvironment &, ErrorHandler *);
  
  virtual void unparse_declaration(StringAccum &, const String &, const String &);

  virtual bool direct_expansion() const	{ return true; }
  virtual CompoundElementClassT *cast_compound() { return 0; }
  virtual RouterT *cast_router()	{ return 0; }

  static String signature(const String &, int, int, int);
  
 private:
  
  int _use_count;
  
  static int direct_expand_element(RouterT *, int, ElementClassT *, RouterT *, const VariableEnvironment &, ErrorHandler *);
  
};

class SynonymElementClassT : public ElementClassT {

  String _name;
  ElementClassT *_eclass;
  
 public:

  SynonymElementClassT(const String &, ElementClassT *);

  ElementClassT *find_relevant_class(int ninputs, int noutputs, const Vector<String> &);
  int complex_expand_element(RouterT *, int, const String &, Vector<String> &, RouterT *, const VariableEnvironment &, ErrorHandler *);
  
  void unparse_declaration(StringAccum &, const String &, const String &);

  bool direct_expansion() const		{ return false; }
  CompoundElementClassT *cast_compound();
  RouterT *cast_router();
  
};

class CompoundElementClassT : public ElementClassT {

  String _name;
  String _landmark;
  RouterT *_router;
  int _depth;
  Vector<String> _formals;
  int _ninputs;
  int _noutputs;
  
  ElementClassT *_next;
  
  bool _circularity_flag;
  
  int actual_expand(RouterT *, int, RouterT *, const VariableEnvironment &, ErrorHandler *);
  
 public:

  CompoundElementClassT(RouterT *);
  CompoundElementClassT(const String &name, const String &landmark, RouterT *, ElementClassT *, int depth);
  ~CompoundElementClassT();

  int nformals() const			{ return _formals.size(); }
  void add_formal(const String &n)	{ _formals.push_back(n); }
  void finish(ErrorHandler *);
  void check_duplicates_until(ElementClassT *, ErrorHandler *);
  
  ElementClassT *find_relevant_class(int ninputs, int noutputs, const Vector<String> &);
  void report_signatures(const String &, String, ErrorHandler *);
  int complex_expand_element(RouterT *, int, const String &, Vector<String> &, RouterT *, const VariableEnvironment &, ErrorHandler *);
  
  void unparse_declaration(StringAccum &, const String &, const String &);

  bool direct_expansion() const		{ return false; }
  CompoundElementClassT *cast_compound() { return this; }
  RouterT *cast_router()		{ return _router; }

  String signature() const;
  
};


inline bool
operator==(const Hookup &h1, const Hookup &h2)
{
  return h1.idx == h2.idx && h1.port == h2.port;
}

inline bool
operator!=(const Hookup &h1, const Hookup &h2)
{
  return h1.idx != h2.idx || h1.port != h2.port;
}

inline bool
operator<(const Hookup &h1, const Hookup &h2)
{
  return h1.idx < h2.idx || (h1.idx == h2.idx && h1.port < h2.port);
}

inline bool
operator>(const Hookup &h1, const Hookup &h2)
{
  return h1.idx > h2.idx || (h1.idx == h2.idx && h1.port > h2.port);
}

#endif
