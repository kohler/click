#ifndef NULLELEMENT_HH
#define NULLELEMENT_HH
#include <click/element.hh>

/*
=c
Null

=s
passes packets unchanged

=d
Just passes packets along without doing anything else.

=a
PushNull, PullNull
*/

class NullElement : public Element { public:
  
  NullElement();
  ~NullElement();
  
  const char *class_name() const	{ return "Null"; }
  const char *processing() const	{ return AGNOSTIC; }
  NullElement *clone() const		{ return new NullElement; }
  
  Packet *simple_action(Packet *);
  
};

/*
=c
PushNull

=s
passes packets unchanged

=d
Responds to each pushed packet by pushing it unchanged out its first output.

=a
Null, PullNull
*/

class PushNullElement : public Element { public:
  
  PushNullElement();
  ~PushNullElement();
  
  const char *class_name() const	{ return "PushNull"; }
  const char *processing() const	{ return PUSH; }
  PushNullElement *clone() const	{ return new PushNullElement; }
  
  void push(int, Packet *);
  
};

/*
=c
PullNull

=s
passes packets unchanged

=d
Responds to each pull request by pulling a packet from its input and returning
that packet unchanged.

=a
Null, PushNull */

class PullNullElement : public Element { public:
  
  PullNullElement();
  ~PullNullElement();
  
  const char *class_name() const	{ return "PullNull"; }
  const char *processing() const	{ return PULL; }
  PullNullElement *clone() const	{ return new PullNullElement; }
  
  Packet *pull(int);
  
};

#endif
