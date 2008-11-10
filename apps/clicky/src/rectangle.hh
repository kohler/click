#ifndef CLICKY_RECTANGLE_HH
#define CLICKY_RECTANGLE_HH 1
#include <algorithm>
#include <math.h>

struct point {
    double _x;
    double _y;

    point() {
    }

    point(double x, double y)
	: _x(x), _y(y) {
    }

    typedef double (point::*unspecified_bool_type)() const;

    operator unspecified_bool_type() const {
	return (_x || _y ? &point::x : 0);
    }

    bool operator!() const {
	return !_x && !_y;
    }

    double x() const {
	return _x;
    }

    double y() const {
	return _y;
    }

    double length() const {
	return sqrt(_x*_x + _y*_y);
    }

    double squared_length() const {
	return _x*_x + _y*_y;
    }

    double angle() const {
	return atan2(_y, _x);
    }

    void shift(double dx, double dy) {
	_x += dx;
	_y += dy;
    }

    void shift(const point &p) {
	shift(p._x, p._y);
    }

    void scale(double s) {
	_x *= s;
	_y *= s;
    }

    void rotate(double angle) {
	if (angle == 0)
	    /* do nothing */;
	else if (angle == M_PI_2) {
	    double t = _x;
	    _x = -_y;
	    _y = t;
	} else if (angle == M_PI) {
	    _x = -_x;
	    _y = -_y;
	} else if (angle == -M_PI_2) {
	    double t = _x;
	    _x = _y;
	    _y = -t;
	} else {
	    double r = length();
	    double theta = this->angle() + angle;
	    _x = r * cos(theta);
	    _y = r * sin(theta);
	}
    }

    point shifted(const point &p) const {
	point x(*this);
	x.shift(p);
	return x;
    }

    point scaled(double s) const {
	point x(*this);
	x.scale(s);
	return x;
    }

    point rotated(double angle) const {
	point x(*this);
	x.rotate(angle);
	return x;
    }

    static inline bool close(const point &a, const point &b, double d);
};

struct rectangle {
    double _x;			// order required
    double _y;
    double _width;
    double _height;

    enum {
	side_top = 0,
	side_right = 1,
	side_bottom = 2,
	side_left = 3
    };

    rectangle() {
    }

    rectangle(double x, double y, double width, double height)
	: _x(x), _y(y), _width(width), _height(height) {
    }

    double x() const {
	return _x;
    }

    double y() const {
	return _y;
    }

    double width() const {
	return _width;
    }

    double height() const {
	return _height;
    }

    double x1() const {
	return _x;
    }

    double x2() const {
	return _x + _width;
    }

    double y1() const {
	return _y;
    }

    double y2() const {
	return _y + _height;
    }

    static bool side_horizontal(int s) {
	assert(s >= side_top && s <= side_left);
	return (s & side_right);
    }

    static bool side_vertical(int s) {
	assert(s >= side_top && s <= side_left);
	return !(s & side_right);
    }

    static bool side_greater(int s) {
	assert(s >= side_top && s <= side_left);
	return (s + (s >> 1)) & 1;
    }

    double side(int s) const {
	double p = (&_x)[side_vertical(s)];
	if (side_greater(s))
	    return p + (&_width)[side_vertical(s)];
	else
	    return p;
    }

    double side_length(int s) const {
	return (&_width)[side_vertical(s)];
    }

    double center_x() const {
	return _x + _width / 2;
    }

    double center_y() const {
	return _y + _height / 2;
    }

    point origin() const {
	return point(_x, _y);
    }

    typedef point (rectangle::*unspecified_bool_type)() const;

    operator unspecified_bool_type() const {
	return (_width > 0 && _height > 0 ? &rectangle::origin : 0);
    }

    void assign(double x, double y, double width, double height) {
	_x = x;
	_y = y;
	_width = width;
	_height = height;
    }

    void assign(const rectangle &r) {
	_x = r.x();
	_y = r.y();
	_width = r.width();
	_height = r.height();
    }

    rectangle normalize() const {
	if (_width >= 0 && _height >= 0)
	    return *this;
	else if (_width >= 0)
	    return rectangle(_x, _y + _height, _width, -_height);
	else if (_height >= 0)
	    return rectangle(_x + _width, _y, -_width, _height);
	return rectangle(_x + _width, _y + _height, -_width, -_height);
    }

    void set_origin(const point &p) {
	_x = p.x();
	_y = p.y();
    }

    void set_origin(double x, double y) {
	_x = x;
	_y = y;
    }

    void set_size(double width, double height) {
	_width = width;
	_height = height;
    }

    void shift(double dx, double dy) {
	_x += dx;
	_y += dy;
    }

    void shift(const point &p) {
	shift(p._x, p._y);
    }

    void expand(double d) {
	_x -= d;
	_y -= d;
	_width += 2 * d;
	_height += 2 * d;
    }

    void expand(double dtop, double dright, double dbottom, double dleft) {
	_x -= dleft;
	_y -= dtop;
	_width += dleft + dright;
	_height += dtop + dbottom;
    }

    void scale(double s) {
	_x *= s;
	_y *= s;
	_width *= s;
	_height *= s;
    }

    bool contains(double x, double y) const {
	return (x >= _x && x <= _x + _width && y >= _y && y <= _y + _height);
    }

    bool contains(const point &p) const {
	return contains(p._x, p._y);
    }

    bool operator&(const rectangle &o) const {
	return ((_x + _width >= o._x && _x <= o._x + o._width)
		&& (_y + _height >= o._y && _y <= o._y + o._height));
    }

    rectangle intersect(const rectangle &o) const {
	double x1 = std::max(_x, o._x);
	double y1 = std::max(_y, o._y);
	double x2 = std::min(_x + _width, o._x + o._width);
	double y2 = std::min(_y + _height, o._y + o._height);
	return rectangle(x1, y1, std::max(x2 - x1, 0.), std::max(y2 - y1, 0.));
    }

    double area() const {
	return _width * _height;
    }

    rectangle &operator|=(const rectangle &o) {
	if (!*this)
	    *this = o;
	if (_x > o._x) {
	    _width += _x - o._x;
	    _x = o._x;
	}
	if (_y > o._y) {
	    _height += _y - o._y;
	    _y = o._y;
	}
	if (_x + _width < o._x + o._width)
	    _width = o._x + o._width - _x;
	if (_y + _height < o._y + o._height)
	    _height = o._y + o._height - _y;
	return *this;
    }

    rectangle &operator|=(const point &p) {
	if (!*this) {
	    _x = p.x();
	    _y = p.y();
	    _width = _height = 0;
	}
	if (_x > p.x()) {
	    _width += _x - p.x();
	    _x = p.x();
	}
	if (_y > p.y()) {
	    _height += _y - p.y();
	    _y = p.y();
	}
	if (_x + _width < p.x())
	    _width = p.x() - _x;
	if (_y + _height < p.y())
	    _height = p.y() - _y;
	return *this;
    }

    void integer_align() {
	double x2_ = x2(), y2_ = y2();
	_x = floor(_x);
	_y = floor(_y);
	_width = ceil(x2_) - _x;
	_height = ceil(y2_) - _y;
    }
};

inline bool operator==(const point &a, const point &b) {
    return a.x() == b.x() && a.y() == b.y();
}

inline bool operator!=(const point &a, const point &b) {
    return a.x() != b.x() || a.y() != b.y();
}

inline point operator+(const point &a, const point &b) {
    return point(a.x() + b.x(), a.y() + b.y());
}

inline point operator-(const point &a, const point &b) {
    return point(a.x() - b.x(), a.y() - b.y());
}

inline point operator*(const point &a, double b) {
    return point(a.x() * b, a.y() * b);
}

inline point operator*(double a, const point &b) {
    return point(a * b.x(), a * b.y());
}

inline point operator*(const point &a, const point &b) {
    return point(a.x() * b.x(), a.y() * b.y());
}

inline point operator/(const point &a, double b) {
    return point(a.x() / b, a.y() / b);
}

inline point operator/(const point &a, const point &b) {
    return point(a.x() / b.x(), a.y() / b.y());
}

inline bool point::close(const point &a, const point &b, double d) {
    return (a - b).length() <= d;
}

#endif
