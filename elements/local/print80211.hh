#ifndef CLICK_PRINT80211_HH
#define CLICK_PRINT80211_HH
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

Print, IPPrint, PrintAiro */

class Print80211 : public Element { public:

  Print80211();
  ~Print80211();
  
  const char *class_name() const		{ return "Print80211"; }
  const char *processing() const		{ return AGNOSTIC; }
  
  Print80211 *clone() const;
  int configure(Vector<String> &, ErrorHandler *);
  bool can_live_reconfigure() const		{ return true; }
  
  Packet *simple_action(Packet *);
  
 private:
  
  String _label;
  bool _timestamp;
  bool _verbose;
};

CLICK_ENDDECLS
#endif
