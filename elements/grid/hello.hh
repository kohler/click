#ifndef Hello_HH
#define Hello_HH

/*
 * =c
 * Hello(T, eth)
 * =d
 * Every T seconds,
 * sends a Grid HELLO packet from eth
 *
 * =a Neighbor */

#include "element.hh"
#include "timer.hh"
#include "etheraddress.hh"

class Hello : public Element {
  
public:
  
  Hello();
  ~Hello();
  
  const char *class_name() const		{ return "Hello"; }
  const char *processing() const		{ return PUSH; }
  Hello *clone() const;
  
  int configure(const Vector<String> &, ErrorHandler *);
  int initialize(ErrorHandler *);
  
  Packet *make_hello();
  
  void run_scheduled();
  
private:
  
  EtherAddress _from;
  int _period;
  Timer _timer;
  
};

#endif
