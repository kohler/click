#ifndef GAPRATE_HH
#define GAPRATE_HH
#include <click/glue.hh>
class ErrorHandler;

class GapRate {

  static const unsigned UGAP_SHIFT = 12;

  unsigned _ugap;
  int _sec_count;
  int _tv_sec;
  unsigned _rate;
#if DEBUG_GAPRATE
  struct timeval _last;
#endif

 public:

  static const unsigned MAX_RATE = 1000000 << UGAP_SHIFT;

  GapRate();
  GapRate(unsigned);

  unsigned rate() const				{ return _rate; }
  
  void set_rate(unsigned);
  void set_rate(unsigned, ErrorHandler *);
  void reset();

  bool need_update(const struct timeval &);
  void update()					{ _sec_count++; }
  void update_with(int incr)			{ _sec_count += incr; }

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
    _sec_count = (now.tv_usec << UGAP_SHIFT) / _ugap;
  } else if (now.tv_sec > _tv_sec) {
    _tv_sec = now.tv_sec;
    if (_sec_count > 0)
      _sec_count -= _rate;
  }

  unsigned need = (now.tv_usec << UGAP_SHIFT) / _ugap;
  if ((int)need > _sec_count) {
#if DEBUG_GAPRATE
    if (_last.tv_sec) {
      struct timeval diff;
      timersub(&now, &_last, &diff);
      click_chatter("%d.%06d  (%d)", diff.tv_sec, diff.tv_usec, now.tv_sec);
    }
    _last = now;
#endif
    return true;
  } else
    return false;
}

#endif
