#ifndef CLICK_PRINTOLD_HH
#define CLICK_PRINTOLD_HH
#include <click/element.hh>
#include <click/string.hh>
CLICK_DECLS

/*
=c

PrintOld([LABEL, AGE, MAXLENGTH])

=s debugging

conditionally prints packet contents

=d

Prints up to MAXLENGTH bytes of data from each packet, in hex, preceded
by the LABEL text, if more than AGE milliseconds have elapsed since
the packet was timestamped.  Default AGE is 5 milliseconds.
Default MAXLENGTH is 24.


=a

IPPrint
Print

*/

class PrintOld : public Element { public:

  PrintOld() CLICK_COLD;
  ~PrintOld() CLICK_COLD;

  const char *class_name() const		{ return "PrintOld"; }
  const char *port_count() const		{ return PORTS_1_1; }

  int configure(Vector<String> &, ErrorHandler *) CLICK_COLD;

  Packet *simple_action(Packet *);

 private:

  String _label;
  int _bytes;			// How many bytes of a packet to print
  int _thresh;
};

CLICK_ENDDECLS
#endif
