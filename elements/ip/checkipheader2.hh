#ifndef CLICK_CHECKIPHEADER2_HH
#define CLICK_CHECKIPHEADER2_HH
#include "elements/ip/checkipheader.hh"
CLICK_DECLS

/*
=c

CheckIPHeader2([OFFSET, I<keywords>])

=s ip

checks IP header, no checksum

=d

This element behaves exactly like CheckIPHeader, except that it does not by
default check packets' IP checksums.

=a CheckIPHeader, StripIPHeader, MarkIPHeader */

class CheckIPHeader2 : public CheckIPHeader { public:

  CheckIPHeader2();
  ~CheckIPHeader2();

  const char *class_name() const		{ return "CheckIPHeader2"; }

};

CLICK_ENDDECLS
#endif
