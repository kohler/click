#ifndef ELEMENTT_HH
#define ELEMENTT_HH
#include <click/string.hh>
#include <stddef.h>
#include <click/vector.hh>
class RouterT;
class RouterScope;
class ErrorHandler;
class StringAccum;

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

class ElementClassT {

  int _use_count;
  
 public:

  ElementClassT();
  virtual ~ElementClassT()		{ }

  void use()				{ _use_count++; }
  void unuse()				{ if (--_use_count <= 0) delete this; }

  static int simple_expand_into(RouterT *, int, RouterT *,
				const RouterScope &, ErrorHandler *);
  virtual int expand_into(RouterT *, int, RouterT *,
			  const RouterScope &, ErrorHandler *);
  virtual void compound_declaration_string(StringAccum &, const String &, const String &);

  virtual bool expands_away() const	{ return false; }
  virtual RouterT *cast_router()	{ return 0; }
  
};

class SynonymElementClassT : public ElementClassT {

  String _name;
  ElementClassT *_eclass;
  
 public:

  SynonymElementClassT(const String &, ElementClassT *);

  int expand_into(RouterT *, int, RouterT *,
		  const RouterScope &, ErrorHandler *);
  void compound_declaration_string(StringAccum &, const String &,
				   const String &);

  bool expands_away() const		{ return true; }
  
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
