#ifndef REGION_HH
#define REGION_HH

#include <assert.h>
#include "string.hh"

class RectRegion {

public:

  // x_bl, y_bl are coords of bottom left.  region extends width to
  // the right and heigh up from (x_bl, y_bl)
  RectRegion(double x_bl, double y_bl, double width, double height) :
    _x(x_bl), _y(y_bl), _w(width), _h(height)
  { assert(width >= 0); assert(height >= 0); }

  // create an empty region -- this is not the same as an infinitely
  // thin region
  RectRegion() : _w(-1), _h(-1) { }

  // resulting region may be empty!
  RectRegion intersect(RectRegion &r);

  bool contains(double x, double y)
    { return (x >= _x) && (x <= _x + _w) && (y >= _y) && (y <= _y + _h); }

  bool empty() { return _w < 0 || _h < 0; }

  double x() { return _x; };
  double y() { return _y; };
  double w() { return _w; };
  double h() { return _h; };

  String s();

private:
 
  double _x;
  double _y;
  // empty region implied by negative height or width (_h, _w)
  double _w;
  double _h;
};


#endif
