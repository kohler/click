#ifndef FORCEUDP_HH
#define FORCEUDP_HH

/*
 * =c
 * ForceUDP([DPORT])
 * =s UDP
 * sets UDP packet fields
 * =d
 * Set the checksum and some other fields to try to make a
 * packet look like UDP. If DPORT is specified and not -1, forces
 * the destination port to be DPORT.
 */

#include <click/element.hh>
#include <click/glue.hh>

class ForceUDP : public Element {
public:
  ForceUDP();
  ~ForceUDP();
  
  const char *class_name() const		{ return "ForceUDP"; }
  const char *processing() const		{ return AGNOSTIC; }
  ForceUDP *clone() const;
  int configure(Vector<String> &conf, ErrorHandler *errh);

  Packet *simple_action(Packet *);

private:
  int _count;
  int _dport;
};

#endif
