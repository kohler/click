#ifndef CHECKICMPHEADER_HH
#define CHECKICMPHEADER_HH

/*
 * =c
 * CheckICMPHeader()
 *
 * =s ICMP, checking
 * checks ICMP header on ICMP packets
 *
 * =d
 *
 * Expects ICMP packets as input. Checks that the packet's length is sensible
 * and that its checksum field is valid. Pushes invalid packets out on output
 * 1, unless output 1 was unused; if so, drops invalid packets.
 *
 * =a CheckIPHeader, CheckUDPHeader, MarkIPHeader */

#include <click/element.hh>
#include <click/glue.hh>

class CheckICMPHeader : public Element {

  int _drops;
  
 public:
  
  CheckICMPHeader();
  ~CheckICMPHeader();
  
  const char *class_name() const		{ return "CheckICMPHeader"; }
  const char *processing() const		{ return "a/ah"; }
  CheckICMPHeader *clone() const		{ return new CheckICMPHeader; }
  void notify_noutputs(int);
  
  int drops() const				{ return _drops; }
  
  void add_handlers();

  Packet *simple_action(Packet *);

};

#endif
