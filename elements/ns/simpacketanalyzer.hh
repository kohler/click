#ifndef CLICK_SIMPACKETANALYZER_HH
#define CLICK_SIMPACKETANALYZER_HH
#include <click/element.hh>
CLICK_DECLS

/* =c
 *
 * SimPacketAnalyzer()
 *
 * =s traces
 *
 * superclass for entering packet analyses in an ns2 trace
 *
 * =d
 *
 * Implement this interface for specific protocols to allow analysis of
 * packets in ToSimTrace.
 *
 * =a
 * ToSimTrace
 */


class SimPacketAnalyzer : public Element{
public:
  SimPacketAnalyzer() CLICK_COLD;
  ~SimPacketAnalyzer() CLICK_COLD;

  const char *class_name() const  { return "SimPacketAnalyzer"; }
  const char *processing() const  { return PUSH; }
  const char *port_count() const  { return PORTS_0_0; }

  virtual String analyze(Packet*, int offset) = 0;

private:

};

CLICK_ENDDECLS

#endif

