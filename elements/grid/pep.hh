#ifndef pep_hh
#define pep_hh

/*
 * =c
 * PEP(IP, [lat, lon])
 * =d
 * Run the Grid Position Estimation Protocol.
 *
 * Produces packets with just the PEP payload. The Grid configuration
 * must arrange to encapsulate them appropriately. I expect this
 * means UDP/IP/Ether.
 *
 * Also expects packets with just PEP payload.
 */

#include "element.hh"
#include "timer.hh"
#include "ipaddress.hh"
#include "pep_proto.hh"
#include "grid.hh"

class PEP : public Element {
  
public:
  
  PEP();
  ~PEP();
  
  const char *class_name() const		{ return "PEP"; }
  const char *processing() const		{ return PUSH; }
  PEP *clone() const;
  
  int configure(const Vector<String> &, ErrorHandler *);
  int initialize(ErrorHandler *);
  
  Packet *make_PEP();
  Packet *simple_action(Packet *p);
  
  void run_scheduled();
  
private:
  
  IPAddress _my_ip;
  Timer _timer;
  int _period;  // Milliseconds between updates we send.

  bool _fixed;  // We have a static, known location in _lat / _lon.
  float _lat;
  float _lon;

  struct Entry {
    bool _valid;
    unsigned long _id;
    grid_location _loc;
    unsigned _d; // Estimated meters to _loc. In host byte order.
  };
#define PEPMaxEntries 10
  Entry _entries[PEPMaxEntries];
  Entry *findEntry(unsigned id, int allocate);
};

#endif
