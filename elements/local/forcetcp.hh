#ifndef FORCETCP_HH
#define FORCETCP_HH

/*
 * =c
 * ForceTCP()
 * =s sets TCP packet fields
 * =d
 * Set the checksum and some other fields to try to make a
 * packet look like TCP.
 */

#include "element.hh"
#include "glue.hh"

class ForceTCP : public Element {
public:
  ForceTCP();
  ~ForceTCP();
  
  const char *class_name() const		{ return "ForceTCP"; }
  const char *processing() const		{ return AGNOSTIC; }
  ForceTCP *clone() const;

  Packet *simple_action(Packet *);

private:
  int _count;
};

#endif
