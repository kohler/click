#ifndef CHECKUDPHEADER_HH
#define CHECKUDPHEADER_HH

/*
 * =c
 * CheckUDPHeader()
 * =s
 * checks UDP header on UDP/IP packets
 * V<checking>
 * =d
 * Expects UDP/IP packets as input.
 * Checks that the UDP header length and checksum fields are valid.
 * Pushes invalid packets out on output 1, unless output 1 was unused;
 * if so, drops invalid packets.
 *
 * =a CheckIPHeader, CheckTCPHeader, MarkIPHeader
 */

#include <click/element.hh>
#include <click/glue.hh>

class CheckUDPHeader : public Element {

  int _drops;
  
 public:
  
  CheckUDPHeader();
  
  const char *class_name() const		{ return "CheckUDPHeader"; }
  const char *processing() const		{ return "a/ah"; }
  void notify_noutputs(int);
  
  int drops() const				{ return _drops; }
  
  CheckUDPHeader *clone() const;
  void add_handlers();

  Packet *simple_action(Packet *);
  /* inline Packet *smaction(Packet *);
     void push(int, Packet *p);
     Packet *pull(int); */

};

#endif
