#ifndef CLICK_PRINTAIRO_HH
#define CLICK_PRINTAIRO_HH
#include <click/element.hh>
#include <click/string.hh>
CLICK_DECLS

/*
=c

Print([TAG, I<KEYWORDS>])

=s debugging

prints Aironet header contents

=d


Keyword arguments are:

=over 8

=item TIMESTAMP

Boolean. Determines whether to print each packet's timestamp in seconds since
1970. Default is false.

=back

=a

Print, IPPrint */

class PrintAiro : public Element { public:

  PrintAiro();
  ~PrintAiro();
  
  const char *class_name() const		{ return "PrintAiro"; }
  const char *processing() const		{ return AGNOSTIC; }
  
  PrintAiro *clone() const;
  int configure(Vector<String> &, ErrorHandler *);
  bool can_live_reconfigure() const		{ return true; }
  
  Packet *simple_action(Packet *);
  
 private:
  
  String _label;
  bool _timestamp;
};

CLICK_ENDDECLS
#endif
