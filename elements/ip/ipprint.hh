#ifndef IPPRINT_HH
#define IPPRINT_HH
#include "element.hh"
#include "string.hh"

/*
 * =c
 * IPPrint(TAG [, CONTENTS [, NBYTES]])
 * =d
 * Expects IP packets as input.  Should be placed downstream of a 
 * CheckIPHeader or equivalent element.
 *
 * Prints out IP packets in a human-readable tcpdump-like format, preceded by
 * the TAG text.
 *
 * The CONTENTS argument determines whether the packet data is printed. It may
 * be `false' (do not print packet data), `hex' (print packet data in
 * hexadecimal), or `ascii' (print packet data in plaintext). Default is
 * `false'. The NBYTES argument determines the number of bytes to dump as hex.
 * The default value is 1500.
 *
 * =a Print, CheckIPHeader
 * */

class IPPrint : public Element {
  
  String _label;
  char* _buf;			// To hold hex dump message
  int _contents;		// Whether to dump packet contents
  unsigned _bytes;		// Number of byutes to dump

 public:
  
  IPPrint();
  ~IPPrint();
  
  const char *class_name() const		{ return "IPPrint"; }
  const char *processing() const		{ return AGNOSTIC; }
  
  IPPrint *clone() const;
  int configure(const Vector<String> &, ErrorHandler *);
  void uninitialize();
  
  Packet *simple_action(Packet *);
  
};

#endif
