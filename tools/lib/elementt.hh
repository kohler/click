#ifndef ELEMENTT_HH
#define ELEMENTT_HH
#include "string.hh"
#include <stddef.h>
#include "vector.hh"
class RouterT;
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
  
};

struct Hookup {
  
  int idx;
  int port;
  
  Hookup()				: idx(-1) { }
  Hookup(int i, int p)			: idx(i), port(p) { }

  int index_in(const Vector<Hookup> &, int start = 0) const;
  int force_index_in(Vector<Hookup> &, int start = 0) const;

  static int sorter(const void *, const void *);
  static void sort(Vector<Hookup> &);
  
};

class ElementClassT {

  int _use_count;
  
 public:

  ElementClassT();

  void use()				{ _use_count++; }
  void unuse()				{ if (--_use_count <= 0) delete this; }
  
  virtual bool expand_compound(ElementT &, RouterT *, ErrorHandler *);
  virtual void compound_declaration_string(StringAccum &, const String &, const String &);

  virtual RouterT *cast_router()	{ return 0; }
  
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
