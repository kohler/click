#ifndef CLICK_PRINTAIRO_HH
#define CLICK_PRINTAIRO_HH
#include <click/element.hh>
#include <click/string.hh>
CLICK_DECLS

/*
=c

PrintAiro([TAG, I<KEYWORDS>])

=s debugging

Prints raw Aironet header contents including 802.11 PLCP information,
returns the underlying 802.11 packet.

=d


Keyword arguments are:

=over 8

=item TIMESTAMP

Boolean.  Determines whether to print each packet's timestamp in seconds since
1970.  Default is false.

=item QUIET

Boolean.  If true, don't print any Aironet header info, just
decapsulate the 802.11 packet and pass it through.  Default is false.

=item VERBOSE

Boolean.  If true, print some extra information from the PLCP header.
Default is false.  This argument has no effect if QUIET is true.

=back

=a

Print, IPPrint, Print80211 */

class PrintAiro : public Element { public:

  PrintAiro() CLICK_COLD;
  ~PrintAiro() CLICK_COLD;

  const char *class_name() const		{ return "PrintAiro"; }
  const char *port_count() const		{ return PORTS_1_1; }

  int configure(Vector<String> &, ErrorHandler *) CLICK_COLD;
  bool can_live_reconfigure() const		{ return true; }

  Packet *simple_action(Packet *);

 private:

  String _label;
  bool _timestamp;
  bool _quiet;
  bool _verbose;
};

CLICK_ENDDECLS
#endif
