#ifndef SETIPDSCP_HH
#define SETIPDSCP_HH

/*
 * =c
 * SetIPDSCP(VAL)
 * =s IP
 * sets IP packets' DSCP fields
 * =d
 * Expects IP packets as input and
 * sets their Differential Services Code Point to VAL.
 * Then it incrementally recalculates the IP checksum
 * and passes the packet to output 0.
 * The DSCP is the upper 6 bits of the IP TOS field.
 */

#include <click/element.hh>
#include <click/glue.hh>

class SetIPDSCP : public Element {

  unsigned char _dscp;
  
 public:
  
  SetIPDSCP();
  ~SetIPDSCP();
  
  const char *class_name() const		{ return "SetIPDSCP"; }
  const char *processing() const		{ return AGNOSTIC; }

  unsigned char dscp() const			{ return _dscp; }
  
  SetIPDSCP *clone() const;
  int configure(const Vector<String> &, ErrorHandler *);
  bool can_live_reconfigure() const		{ return true; }
  void add_handlers();

  inline Packet *smaction(Packet *);
  void push(int, Packet *p);
  Packet *pull(int);
  
};

#endif
