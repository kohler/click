#ifndef CHECKLENGTH_HH
#define CHECKLENGTH_HH
#include <click/element.hh>

/*
 * =c
 * CheckLength(MAX)
 * =s checking
 * drops large packets
 * =d
 * CheckLength checks every packet's length against MAX. If the packet has
 * length MAX or smaller, it is sent to output 0; otherwise, it is sent to output 1 (or dropped if there is no output 1).
 *
 */

class CheckLength : public Element { protected:
  
  unsigned _max;
  
 public:
  
  CheckLength();
  ~CheckLength();
  
  const char *class_name() const		{ return "CheckLength"; }
  const char *processing() const		{ return "a/ah"; }
  void notify_noutputs(int);
  
  CheckLength *clone() const			{ return new CheckLength; }
  int configure(const Vector<String> &, ErrorHandler *);
  
  void push(int, Packet *);
  Packet *pull(int);
  
};

#endif
