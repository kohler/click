#ifndef STRIP_HH
#define STRIP_HH
#include "element.hh"

/*
 * =c
 * Strip(N)
 * =d
 * Deletes the first N bytes from each packet.
 * =e
 * Use this to get rid of the Ethernet header:
 *
 * = Strip(14)
 * =a EtherEncap
 * =a IPEncap
 */

class Strip : public Element {
  
  unsigned _nbytes;
  
 public:
  
  Strip(unsigned nbytes = 0);
  ~Strip();
  
  const char *class_name() const		{ return "Strip"; }
  const char *processing() const	{ return AGNOSTIC; }
  
  Strip *clone() const;
  int configure(const String &, ErrorHandler *);

  Packet *simple_action(Packet *);
  /* inline void smaction(Packet *);
     void push(int, Packet *p);
     Packet *pull(int); */
  
};

#endif
