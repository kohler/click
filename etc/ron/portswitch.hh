#ifndef PORTSWITCH_HH
#define PORTSWITCH_HH
#include <click/element.hh>

/*
=c

PortSwitch()

=s classification


=d

=a 

 */

class PortSwitch : public Element { public:

  PortSwitch();
  ~PortSwitch();
  
  const char *class_name() const		{ return "PortSwitch"; }
  const char *processing() const		{ return PUSH; }
  
  PortSwitch *clone() const;
  void notify_noutputs(int);
  int configure(const Vector<String> &, ErrorHandler *);
  
  void push(int, Packet *);
  
 private:
  int _noutputs;

};

#endif
