#ifndef SETIPDSCP_HH
#define SETIPDSCP_HH

/*
 * =c
 * SetIPDSCP(x)
 * =d
 * Expects IP packet as input and
 * sets its Differential Services Code Point to x.
 * Then it incrementally recalculates the IP checksum
 * and passes the packet to output 0.
 * The DSCP is the upper 6 bits of the IP TOS field.
 */

#include "element.hh"
#include "glue.hh"

class SetIPDSCP : public Element {

  unsigned char _dscp;
  
 public:
  
  SetIPDSCP();
  
  const char *class_name() const		{ return "SetIPDSCP"; }
  const char *processing() const		{ return AGNOSTIC; }

  unsigned char dscp() const			{ return _dscp; }
  
  SetIPDSCP *clone() const;
  int configure(const String &, ErrorHandler *);
  bool can_live_reconfigure() const		{ return true; }
  void add_handlers();

  inline Packet *smaction(Packet *);
  void push(int, Packet *p);
  Packet *pull(int);
  
};

#endif
