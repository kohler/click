#ifndef CLICK_PEEKHANDLERS_HH
#define CLICK_PEEKHANDLERS_HH
#include <click/element.hh>

/*
=c

PeekHandlers(...)

=s debugging

calls read handlers at specified times

=deprecated PokeHandlers

=io

None

=d

PeekHandlers has been deprecated. Use PokeHandlers instead, with `read
HANDLER' arguments.

=a

PokeHandlers */

class PeekHandlers : public Element { public:

  PeekHandlers();
  ~PeekHandlers();

  const char *class_name() const		{ return "PeekHandlers"; }
  PeekHandlers *clone() const			{ return new PeekHandlers; }
  
  int configure(const Vector<String> &, ErrorHandler *);

};

#endif
