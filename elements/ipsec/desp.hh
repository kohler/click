#ifndef IPSEC_DESP_HH
#define IPSEC_DESP_HH

/*
 * IPsec_DESP
 * 
 * De-encapsulate a packet using ESP per RFC2406.
 */


#include "element.hh"
#include "glue.hh"

class Address;

class DeEsp : public Element {
public:
  DeEsp();
  ~DeEsp();
  
  const char *class_name() const		{ return "IPsecESPUnencap"; }
  Processing default_processing() const	{ return AGNOSTIC; }
  
  DeEsp *clone() const;
  int configure(const String &, Router *, ErrorHandler *);
  int initialize(Router *, ErrorHandler *);
  
  Packet *simple_action(Packet *);
  
private:


};

#endif



