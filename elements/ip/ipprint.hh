#ifndef IPPRINT_HH
#define IPPRINT_HH
#include "element.hh"
#include "string.hh"

/*
 * =c
 * IPPrint(TAG [, {true, false} [, bytes ] ])
 * =d
 * Expects IP packets as input.  Should be placed downstream of a 
 * CheckIPHeader or equivalent element.
 *
 * Prints out IP packets in a human-readable tcpdump-like format, 
 * preceded by the TAG text.
 * If the second optional argument is "true", also dumps the entire
 * packet contents in hex, like tcpdump -x.
 * The third optional argument determines the number of bytes to dump
 * as hex.  The default value is 1500.
 *
 * =a Print
 * =a CheckIPHeader
 *
 */

class IPPrint : public Element {
  
  String _label;
  char* _buf;			// To hold hex dump message
  bool _hex;			// Whether to dump packet contents
  unsigned _bytes;		// Number of byutes to dump

 public:
  
  IPPrint();
  ~IPPrint();
  
  const char *class_name() const		{ return "IPPrint"; }
  const char *processing() const		{ return AGNOSTIC; }
  
  IPPrint *clone() const;
  int configure(const Vector<String> &, ErrorHandler *);
  
  Packet *simple_action(Packet *);
  
};

#endif
