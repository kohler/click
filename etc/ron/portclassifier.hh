#ifndef PORTCLASSIFIER_HH
#define PORTCLASSIFIER_HH
#include <click/element.hh>

/*
=c

PortClassifier(BASE, STEPPING)

Classifies TCP packets by source port. Port regions are defined starting
at BASE, each with size STEPPING. All pkts with source port in 
[BASE, BASE+STEPPING) goes to port 0, etc. 

=s classification


=d

=a 

 */

class PortClassifier : public Element { public:

  PortClassifier();
  ~PortClassifier();
  
  const char *class_name() const		{ return "PortClassifier"; }
  const char *processing() const		{ return PUSH; }
  
  PortClassifier *clone() const;
  void notify_noutputs(int);
  int configure(const Vector<String> &, ErrorHandler *);
  
  void push(int, Packet *);
  
 private:
  unsigned int _noutputs;
  unsigned int _base, _stepping;
  int _src;
};

#endif
