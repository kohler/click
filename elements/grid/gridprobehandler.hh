#ifndef GRIDPROBEHANDLER_HH
#define GRIDPROBEHANDLER_HH

/*
 * =c
 * GridProbeHandler(E, I)
 * =s Grid
 * Handles Grid route probes, producing probe replies
 * =d
 * 
 * E and I are this nodes's ethernet and IP addresses, respectively.
 * When a Grid probe is received on its input, pushes a probe reply
 * out of its second output, and pushes the probe out of the first
 * output (if the probe should be forwarded).
 *
 * =a GridProbeSender, GridProbeReplyReceiver */


#include <click/element.hh>
#include <click/etheraddress.hh>
#include <click/ipaddress.hh>
#include <click/vector.hh>
#include <click/bighashmap.hh>
#include <click/timer.hh>

class GridProbeHandler : public Element {

 public:
  GridProbeHandler();
  ~GridProbeHandler();
  
  const char *class_name() const		{ return "GridProbeHandler"; }
  const char *processing() const		{ return PUSH; }
  GridProbeHandler *clone() const;
  int configure(const Vector<String> &, ErrorHandler *);
  int initialize(ErrorHandler *);

  void push(int, Packet *);

private:
  EtherAddress _eth;
  IPAddress _ip;
};

#endif
