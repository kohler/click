#ifndef REGION_HH
#define REGION_HH

#include <click/string.hh>
CLICK_DECLS

class RectRegion {

public:

  // x_bl, y_bl are coords of bottom left.  region extends width to
  // the right and heigh up from (x_bl, y_bl)
  RectRegion(double x_bl, double y_bl, double width, double height) :
    _x(x_bl), _y(y_bl), _w(width), _h(height)
  { assert(width >= 0); assert(height >= 0); }

  // create a region from a point
  RectRegion(double x, double y) : _x(x), _y(y), _w(0), _h(0) { }

  // create a square region of width l centered around a point
  RectRegion(double x, double y, double l) : _x(x-l/2), _y(y-l/2), _w(l), _h(l) { }

  // create an empty region -- this is not the same as an infinitely
  // thin region
  RectRegion() : _x(0), _y(0), _w(-1), _h(-1) { }

  // resulting region may be empty!
  RectRegion intersect(RectRegion &r);

  RectRegion expand(double l);

  bool contains(double x, double y)
    { return (x >= _x) && (x <= _x + _w) && (y >= _y) && (y <= _y + _h); }

  bool empty() { return _w < 0 || _h < 0; }

  double x() { return _x; };
  double y() { return _y; };
  double w() { return _w; };
  double h() { return _h; };

  double center_x() { return _x + _w/2; }
  double center_y() { return _y + _h/2; }

  String s();

private:

  double _x;
  double _y;
  // empty region implied by negative height or width (_h, _w)
  double _w;
  double _h;
};

CLICK_ENDDECLS
#endif
