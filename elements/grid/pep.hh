#ifndef pep_hh
#define pep_hh

/*
 * =c
 * PEP(IP, [fixed?, fix-lat, fix-lon])
 * =s Grid
 * runs the Grid Position Estimation Protocol
 * =d
 * Run the Grid Position Estimation Protocol. Subtypes GridLocationInfo,
 * and can be used in its place.
 *
 * Produces packets with just the PEP payload. The Grid configuration
 * must arrange to encapsulate them appropriately. I expect this
 * means UDP/IP/Ether. Expects packets with just PEP payload.
 *
 * The status handler prints out node id, estimated position,
 * and the table of nearby nodes with known locations.
 *
 * A node that knows its location originates a PEP "fix" every
 * second. Each fix update has a new sequence number.
 *
 * All nodes remember all the fixes they've heard about in the
 * last 100 seconds, along with the number of hops to the fix.
 *
 * A node accepts a new update if the sequence number is
 * higher than remembered, or if seq is equal and hop count
 * is less. The node remembers the time-stamp of the last
 * time it accepted each fix.
 *
 * Every second, a node broadcasts the nearest fixes it knows about.
 * Only fixes heard about in the last 5 seconds are broadcast.
 * Also only fixes with hop count < 10.
 * 
 * There's a problem that an update with a new sequence number
 * may move quickly along a long path, and supersede an older
 * slower update that moved along the minimum length path.
 *
 * Here's how the PEP protocol handles some interesting cases:
 *
 * Crash: after about five seconds, nearby nodes will stop propagating
 * a crashed fix's updates. But they remember the fix for 100 seconds,
 * so won't accept looped updates with high hop counts. The 100
 * seconds has to be longer than 5 seconds plus the maximum
 * allowed hop count; otherwise updates may loop forever.
 *
 * Re-start: a fix node is only allowed to re-start if it has stayed
 * down for >= 100 seconds, long enough for nodes to flush their entries.
 *
 * Move closer: if a fix moves closer to you, you'll see (and
 * believe) smaller hop counts.
 *
 * Move farther: if a fix moves farther from you, you'll see
 * higher hop counts but you'll believe them because the sequence
 * number is larger.
 */

#include <click/element.hh>
#include <click/timer.hh>
#include <click/ipaddress.hh>
#include "pep_proto.hh"
#include "grid.hh"
#include "elements/grid/gridlocationinfo.hh"
CLICK_DECLS

class PEP : public GridLocationInfo {
  
public:
  
  PEP();
  ~PEP();
  
  const char *class_name() const		{ return "PEP"; }
  const char *processing() const		{ return PUSH; }
  PEP *clone() const;
  
  int configure(Vector<String> &, ErrorHandler *);
  int initialize(ErrorHandler *);
  virtual void *cast(const char *);
  void run_scheduled();
  void add_handlers();
  Packet *simple_action(Packet *p);
  
  grid_location get_current_location(void);
  String s();

  bool _debug;
  
private:
  
  IPAddress _my_ip;
  Timer _timer;

  bool _fixed;  // We have a static, known location in _lat / _lon.
  float _lat;
  float _lon;
  int _seq;

  struct Entry {
    struct timeval _when; // When we last updated this entry.
    pep_fix _fix;
  };
  Vector<Entry> _entries;
  int findEntry(unsigned id, bool allocate);

  void purge_old();
  void sort_entries();
  bool sendable(Entry);
  void externalize(pep_fix *);
  void internalize(pep_fix *);
  Packet *make_PEP();
  grid_location algorithm1();
  grid_location algorithm2();
};

CLICK_ENDDECLS
#endif
