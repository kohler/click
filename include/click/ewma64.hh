// -*- mode: c++; c-basic-offset: 4 -*-
#ifndef CLICK_EWMA64_HH
#define CLICK_EWMA64_HH
#include <click/confparse.hh>

class DirectEWMA64 { public:

    DirectEWMA64()			{ _avg = 0; _ss = 10; }

    uint64_t average() const		{ return _avg; }
    unsigned stability_shift() const	{ return _ss; }
    void set_stability_shift(unsigned ss) { _ss = ss; }
    int64_t compensation() const	{ return ((int64_t)1) << (_ss - 1); }

    void clear()			{ _avg = 0; }

    inline void update_with(uint64_t);
    void update_zero_period(unsigned);

    String unparse() const;

  private:
    
    uint64_t _avg;
    unsigned _ss;

};


inline void
DirectEWMA64::update_with(uint64_t val)
{
    _avg += static_cast<int64_t>(val - _avg + compensation()) >> stability_shift();
    // XXX implementation-defined right shift behavior
}

#endif
