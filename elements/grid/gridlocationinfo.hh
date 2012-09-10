#ifndef GRIDLOCATIONINFO_HH
#define GRIDLOCATIONINFO_HH
#include <click/element.hh>
#include <click/timer.hh>
#include "gridgenericlocinfo.hh"
CLICK_DECLS

/*
 * =c
 * GridLocationInfo(LATITUDE, LONGITUDE [, HEIGHT, I<KEYWORDS>])
 * =s Grid
 * =io
 * None
 * =d
 *
 * This element implements the GridGenericLocInfo interface.
 *
 * LATITUDE and LONGITUDE are in decimal degrees (Real).  Positive is
 * North and East, negative is South and West.  HEIGHT is in metres,
 * positive is up.
 *
 * Only 5 fractional digits are used for latitude and longitude, the
 * rest are ignored and rounding is not performed on the remaining
 * digits.  This should provide about 1 metre of precision near the
 * equator.  Only 3 fractional digits are used for the height, giving
 * 1 millimetre precision in height.
 *
 *
 * Keyword arguments are:
 *
 * =over 8
 *
 * =item MOVESIM
 *
 * Integer.  If the optional move parameter is 1, the node will move
 * randomly at a few meters per second.
 *
 * If the optional move parameter is 2, the node will accept external
 * ``set_new_dest'' directives for setting its speed etc.
 *
 *
 * =item LOC_GOOD
 *
 * Boolean.  If true, it means the location information (including
 * error radius) can be believed.  If false, don't believe the hype,
 * it's a sequel, i.e. it's all bogus.
 *
 * =item ERR_RADIUS
 *
 * Unsigned short.  The error radius in metres.  The node's actual
 * location is within a circle of ERR_RADIUS metres, centered at the
 * supplied location.
 *
 * =item TAG
 *
 * String.  A symbolic name used to identify this node's location,
 * e.g. "ne43-521b", or "Robert's office".
 *
 * =h loc read/write
 *
 * When reading, returns the element's location information, in this
 * format: ``lat, lon, height (err=<err-radius> good=<good?> seq=<seq>)''.
 *
 *  <err-radius> is in metres, <good?> is "yes" or "no", indicating
 *  whether the location information is at all valid (i.e. don't
 *  believe any of it unless <good?> is "yes"), and <seq> is the
 *  location sequence number; it changes every time the location or
 *  other parameters change.
 *
 * When writing, use the same syntax as the configuration arguments.
 *
 * =h tag read/write
 *
 * Read/write the location tag.  Read format is: ``tag=<tag>''.
 *
 * =a
 * FixSrcLoc, GridLocationInfo2 */

class GridLocationInfo : public GridGenericLocInfo {

public:
  GridLocationInfo() CLICK_COLD;
  ~GridLocationInfo() CLICK_COLD;

  const char *class_name() const { return "GridLocationInfo"; }

  int configure(Vector<String> &, ErrorHandler *) CLICK_COLD;
  bool can_live_reconfigure() const { return true; }
  void *cast(const char *);

  // seq_no is incremented when location changes ``enough to make a
  // difference''
  grid_location get_current_location(unsigned int *seq_no = 0);

  void add_handlers() CLICK_COLD;
  int read_args(const Vector<String> &conf, ErrorHandler *errh);

  void set_new_dest(double v_lat, double v_lon);

  unsigned int seq_no() { return _seq_no; }
  bool loc_good() { return _loc_good; }
  unsigned short loc_err() { return _loc_err; }

  unsigned int _seq_no;
  String _tag;

protected:

  bool _loc_good; // if false, don't believe loc
  unsigned short _loc_err; // error radius in metres

  int _move;    // Should we move?
  double _lat0; // Where we started.
  double _lon0;
  double _h0;
  double _t0;   // When we started.
  double _t1;   // When we're to pick new velocities.
  double _vlat; // Latitude velocity (in degrees).
  double _vlon; // Longitude velocity.

  double now();
  double xlat();
  double xlon();
  double uniform();
  virtual void choose_new_leg(double *, double *, double *);

  ErrorHandler *_extended_logging_errh;
  Timer _logging_timer;
  static void logging_hook(Timer *, void *);

};

CLICK_ENDDECLS
#endif
