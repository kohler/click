#ifndef GRIDLOCATIONINFO2_HH
#define GRIDLOCATIONINFO2_HH
#include <click/element.hh>
#include <click/timer.hh>
#include "gridgenericlocinfo.hh"
CLICK_DECLS

/*
 * =c
 * GridLocationInfo2(LATITUDE, LONGITUDE [, HEIGHT, I<KEYWORDS>])
 * =s Grid
 * =io
 * None
 * =d
 *
 * This element implements the GridGenericLocInfo interface.  Unlike
 * GridLocationInfo, this element can work in the kernel.
 *
 * LATITUDE and LONGITUDE are in milliseconds.  Positive is North and
 * East, negative is South and West.  HEIGHT is in millimetres,
 * positive is up.  All are integers.
 *
 * Keyword arguments are:
 *
 * =over 8
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
 * FixSrcLoc, GridLocationInfo */

class GridLocationInfo2 : public GridGenericLocInfo {

public:
  GridLocationInfo2();
  ~GridLocationInfo2();

  const char *class_name() const { return "GridLocationInfo2"; }

  int configure(Vector<String> &, ErrorHandler *) CLICK_COLD;
  bool can_live_reconfigure() const { return true; }
  void *cast(const char *);

  // seq_no is incremented when location changes ``enough to make a
  // difference''
  grid_location get_current_location(unsigned int *seq_no = 0);

  void add_handlers() CLICK_COLD;
  int read_args(const Vector<String> &conf, ErrorHandler *errh);

  unsigned int seq_no() { return _seq_no; }
  bool loc_good() { return _loc_good; }
  unsigned short loc_err() { return _loc_err; }

  unsigned int _seq_no;
  String _tag;

protected:

  bool _loc_good; // if false, don't believe loc
  unsigned short _loc_err; // error radius in metres
  grid_location _loc;
};

CLICK_ENDDECLS
#endif
