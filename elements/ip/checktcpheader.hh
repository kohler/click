#ifndef CHECKTCPHEADER_HH
#define CHECKTCPHEADER_HH

/*
 * =c
 * CheckTCPHeader
 * =d
 * Expects TCP/IP packets as input.
 * Checks that the TCP header length and checksum fields are valid.
 * Pushes invalid packets out on output 1, unless output 1 was unused;
 * if so, drops invalid packets.
 *
 * =a CheckIPHeader
 * =a CheckUDPHeader
 * =a MarkIPHeader
 */

#include "element.hh"
#include "glue.hh"

class CheckTCPHeader : public Element {

  int _drops;
  
 public:
  
  CheckTCPHeader();
  
  const char *class_name() const		{ return "CheckTCPHeader"; }
  void notify_noutputs(int);
  void processing_vector(Vector<int> &, int, Vector<int> &, int) const;
  
  int drops() const				{ return _drops; }
  
  CheckTCPHeader *clone() const;
  void add_handlers();

  Packet *simple_action(Packet *);
  /* inline Packet *smaction(Packet *);
     void push(int, Packet *p);
     Packet *pull(int); */

};

#endif
