// -*- c-basic-offset: 4; related-file-name: "../../lib/gaprate.cc" -*-
#ifndef CLICK_GAPRATE_HH
#define CLICK_GAPRATE_HH
#include <click/glue.hh>
CLICK_DECLS
class ErrorHandler;

class GapRate { public:

    inline GapRate();
    inline GapRate(unsigned);

    unsigned rate() const			{ return _rate; }
  
    inline void set_rate(unsigned);
    void set_rate(unsigned, ErrorHandler *);
    inline void reset();

    inline bool need_update(const struct timeval &);
    void update()				{ _sec_count++; }
    void update_with(int incr)			{ _sec_count += incr; }

    enum { UGAP_SHIFT = 12 };
    enum { MAX_RATE = 1000000U << UGAP_SHIFT };

  private:
  
    unsigned _ugap;
    int _sec_count;
    int _tv_sec;
    unsigned _rate;
#if DEBUG_GAPRATE
    struct timeval _last;
#endif

};

inline void
GapRate::reset()
{
    _tv_sec = -1;
#if DEBUG_GAPRATE
    _last.tv_sec = 0;
#endif
}

inline void
GapRate::set_rate(unsigned rate)
{
    if (rate > MAX_RATE)
	rate = MAX_RATE;
    _rate = rate;
    _ugap = (rate == 0 ? MAX_RATE + 1 : MAX_RATE / rate);
#if DEBUG_GAPRATE
    click_chatter("ugap: %u", _ugap);
#endif
    reset();
}

inline
GapRate::GapRate()
{
    set_rate(0);
}

inline
GapRate::GapRate(unsigned rate)
{
    set_rate(rate);
}

inline bool
GapRate::need_update(const struct timeval &now)
{
    if (_tv_sec < 0) {
	_tv_sec = now.tv_sec;
	_sec_count = ((now.tv_usec << UGAP_SHIFT) / _ugap) + 1;
    } else if (now.tv_sec > _tv_sec) {
	_tv_sec = now.tv_sec;
	if (_sec_count > 0)
	    _sec_count -= _rate;
    }

    unsigned need = (now.tv_usec << UGAP_SHIFT) / _ugap;
#if DEBUG_GAPRATE
    click_chatter("%lu.%06lu -> %u @ %u [%d]", now.tv_sec, now.tv_usec, need, _sec_count, (int)need >= _sec_count);
#endif
    return ((int)need >= _sec_count);
}

CLICK_ENDDECLS
#endif
