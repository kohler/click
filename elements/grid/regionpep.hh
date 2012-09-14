#ifndef rgnpep_hh
#define rgnpep_hh

/*
 * =c
 * EstimateRouterRegion(IP, [FIXED, LATITUDE, LONGITUDE])
 * =s Grid
 * =d
 * Run the Region-based Grid Position Estimation Protocol. Subtypes
 * GridLocationInfo, and can be used in its place.
 *
 * =a PEP, GridLocationInfo */

#include <click/element.hh>
#include <click/timer.hh>
#include <click/ipaddress.hh>
#include "pep_proto.hh"
#include "grid.hh"
#include "elements/grid/gridlocationinfo.hh"
#include "region.hh"
CLICK_DECLS

class EstimateRouterRegion : public GridLocationInfo {

public:

  EstimateRouterRegion() CLICK_COLD;
  ~EstimateRouterRegion() CLICK_COLD;

  const char *class_name() const		{ return "EstimateRouterRegion"; }
  const char *port_count() const		{ return PORTS_1_1; }
  const char *processing() const		{ return PUSH; }

  int configure(Vector<String> &, ErrorHandler *) CLICK_COLD;
  int initialize(ErrorHandler *) CLICK_COLD;
  virtual void *cast(const char *);
  void run_timer(Timer *);
  void add_handlers() CLICK_COLD;
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
    Timestamp _when; // When we last updated this entry.
    pep_rgn_fix _fix;
  };
  Vector<Entry> _entries; // hops stored and transmitted are hops to us
  int findEntry(unsigned id, bool allocate);

  RectRegion build_region();

  void purge_old();
  void sort_entries();
  bool sendable(Entry);
  void externalize(pep_rgn_fix *);
  void internalize(pep_rgn_fix *);
  Packet *make_PEP();
  double radio_range(grid_location);
};

CLICK_ENDDECLS
#endif
