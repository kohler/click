#ifndef FORCETCP_HH
#define FORCETCP_HH

/*
 * =c
 * ForceTCP([DPORT, [FLAGS]])
 * =s TCP
 * sets TCP packet fields
 * =d
 * Set the checksum and some other fields to try to make a
 * packet look like TCP. If DPORT is specified and not -1, forces
 * the destination port to be DPORT. If FLAGS is specified and
 * not -1, set the TCP flags field to FLAGS.
 */

#include <click/element.hh>
#include <click/glue.hh>

class ForceTCP : public Element {
public:
  ForceTCP();
  ~ForceTCP();
  
  const char *class_name() const		{ return "ForceTCP"; }
  const char *processing() const		{ return AGNOSTIC; }
  ForceTCP *clone() const;
  int configure(const Vector<String> &conf, ErrorHandler *errh);

  Packet *simple_action(Packet *);

private:
  int _count;
  int _dport;
  int _flags;
};

#endif
