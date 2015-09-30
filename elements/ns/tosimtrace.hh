#ifndef CLICK_TONSTRACE_HH
#define CLICK_TONSTRACE_HH

#include <click/element.hh>
#include <click/router.hh>
#include "simpacketanalyzer.hh"
CLICK_DECLS

/*
=c
ToSimTrace()

=s traces

adds trace entries to an ns2 trace file

=io
One input, one output

=d

This element allows you to add trace entries to an ns2 trace file. The event
id is used to set "r" (receive), "f" (forward), "D" (drop), "s" (send) or
any other id. The timestamp is set to the current time. The packet length
that is traced is obtained from packet->length(). Additional info is added
behind the entry. This is not ns2 default behaviour, but since the packets
are traced as raw at the ns2 level, ToSimTrace traces the packets as raw
as well, but to distinguish you can add the packet type in additional info or
use a SimPacketAnalyzer).

=a
SimPacketAnalyzer
*/

class ToSimTrace:public Element{
public:
  ToSimTrace() CLICK_COLD;
  ~ToSimTrace() CLICK_COLD;

  const char* class_name() const { return "ToSimTrace"; }
  const char* processing() const { return PUSH; }
  const char* port_count() const { return PORTS_1_1; }

  int configure(Vector<String> &conf, ErrorHandler *errh) CLICK_COLD;
  void push(int, Packet *packet);

private:
  String	event_;
  String	additional_info_;
  SimPacketAnalyzer *_packetAnalyzer;
  String	_encap;
  int		_offset;

};

CLICK_ENDDECLS
#endif
