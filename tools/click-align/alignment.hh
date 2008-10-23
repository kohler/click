#ifndef ALIGNMENT_HH
#define ALIGNMENT_HH
#include <click/string.hh>

class Alignment { public:

    Alignment()
	: _modulus(0), _offset(0) {
    }
    Alignment(int m, int o)
	: _modulus(m), _offset(o) {
	assert(m > 0 && o >= 0 && o < m);
    }

    int modulus() const {
	return (_modulus > 1 ? _modulus : 1);
    }
    int offset() const {
	return _offset;
    }
    bool bad() const {
	return _modulus < 0;
    }
    bool empty() const {
	return _modulus == 0;
    }

    bool operator==(const Alignment &) const;
    bool operator!=(const Alignment &) const;
    bool operator<=(const Alignment &) const;

    Alignment &operator+=(int);
    Alignment &operator-=(int);
    Alignment &operator|=(const Alignment &);
    Alignment &operator&=(const Alignment &);

    String unparse() const;

  private:

    int _modulus;
    int _offset;

    Alignment(int m, int o, int)
	: _modulus(m), _offset(o) {
    }

};

inline bool
Alignment::operator==(const Alignment &o) const
{
    return _modulus == o._modulus && _offset == o._offset;
}

inline bool
Alignment::operator!=(const Alignment &o) const
{
    return _modulus != o._modulus || _offset != o._offset;
}

inline Alignment &
Alignment::operator-=(int off)
{
    return *this += (-off);
}

inline Alignment
operator+(Alignment a, int off)
{
    return a += off;
}

inline Alignment
operator-(Alignment a, int off)
{
    return a += (-off);
}

inline Alignment
operator|(Alignment a, const Alignment &b)
{
    return a |= b;
}

inline Alignment
operator&(Alignment a, const Alignment &b)
{
    return a &= b;
}

#endif
