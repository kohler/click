#ifndef PRINTOLD_HH
#define PRINTOLD_HH
#include <click/element.hh>
#include <click/string.hh>

/*
=c

PrintOld([TAG, THRESH, NBYTES])

=s debugging

conditionally prints packet contents 

=d

Prints up to NBYTES bytes of data from each packet, in hex, preceded
by the TAG text, if more than THRESH milliseconds have elapsed since
the packet was timestamped.  Default THRESH is 5 milliseconds.
Default NBYTES is 24.


=a

IPPrint 
Print

*/

class PrintOld : public Element { public:

  PrintOld();
  ~PrintOld();
  
  const char *class_name() const		{ return "PrintOld"; }
  const char *processing() const		{ return AGNOSTIC; }
  
  PrintOld *clone() const;
  int configure(Vector<String> &, ErrorHandler *);
  
  Packet *simple_action(Packet *);
  
 private:
  
  String _label;
  unsigned _bytes;		// How many bytes of a packet to print
  int _thresh;
};

#endif
