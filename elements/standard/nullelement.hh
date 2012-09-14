#ifndef CLICK_NULLELEMENT_HH
#define CLICK_NULLELEMENT_HH
#include <click/element.hh>
CLICK_DECLS

/*
=c
Null

=s basictransfer
null element: passes packets unchanged

=d
Emits input packets unchanged.

=n
Click supports several null-type elements useful for different purposes.

I<Placeholder elements> help make configurations easier to read by allowing a
more natural declaration order. For example, you might say:

  join_point :: Null;
  // packet sources
  src0 :: ...;
  src0 -> join_point;
  src1 :: ...;
  src1 -> join_point;
  // packet sinks
  join_point -> c :: Classifier -> ...;

Null is a reasonable class for placeholder elements, but an empty compound
element serves the same purpose without any runtime overhead.

  join_point :: {->};

PushNull and PullNull can be used to force an agnostic configuration to be
push or pull, respectively.

Null itself is most useful for benchmarking.

=a
PushNull, PullNull
*/

class NullElement : public Element { public:

  NullElement() CLICK_COLD;

  const char *class_name() const	{ return "Null"; }
  const char *port_count() const	{ return PORTS_1_1; }

  Packet *simple_action(Packet *);

};

/*
=c
PushNull

=s basictransfer
push-only null element

=d
Responds to each pushed packet by pushing it unchanged out its first output.

=a
Null, PullNull
*/

class PushNullElement : public Element { public:

  PushNullElement() CLICK_COLD;

  const char *class_name() const	{ return "PushNull"; }
  const char *port_count() const	{ return PORTS_1_1; }
  const char *processing() const	{ return PUSH; }

  void push(int, Packet *);

};

/*
=c
PullNull

=s basictransfer
pull-only null element

=d
Responds to each pull request by pulling a packet from its input and returning
that packet unchanged.

=a
Null, PushNull */

class PullNullElement : public Element { public:

  PullNullElement() CLICK_COLD;

  const char *class_name() const	{ return "PullNull"; }
  const char *port_count() const	{ return PORTS_1_1; }
  const char *processing() const	{ return PULL; }

  Packet *pull(int);

};

CLICK_ENDDECLS
#endif
