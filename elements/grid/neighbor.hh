#ifndef NEIGHBOR_HH
#define NEIGHBOR_HH

// XXX this is probably already implemented in some click element?  i
// don't know...

#include "element.hh"
#include "glue.hh"
#include "hashmap.hh"
#include "etheraddress.hh"

class Neighbor : public Element {

public:

  HashMap<EtherAddress, int> _addresses;

  Neighbor();
  ~Neighbor();

  const char *class_name() const		{ return "Neighbor"; }
  void *cast(const char *);
  const char *processing() const		{ return AGNOSTIC; }
  Neighbor *clone() const;
  
  int configure(const Vector<String> &, ErrorHandler *);
  int initialize(ErrorHandler *);

  void add_handlers();
  
  void run_scheduled();

  void push(int port, Packet *);

private:
  
};

#endif
