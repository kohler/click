#ifndef EWMA_HH
#define EWMA_HH
#include <click/glue.hh>
#include <click/confparse.hh>

template <unsigned Stability_shift, unsigned Scale>
class DirectEWMAX {
  
  unsigned _avg;
  
 public:

  DirectEWMAX()				{ _avg = 0; }

  unsigned average() const		{ return _avg; }
  static const unsigned stability_shift = Stability_shift;
  static const unsigned scale = Scale;
  
  void clear()				{ _avg = 0; }
  
  inline void update_with(unsigned);
  void update_zero_period(unsigned);
  
};

template <unsigned Stability_shift, unsigned Scale, unsigned N, class Timer>
class RateEWMAX {
  
  unsigned _now_time;
  unsigned _total[N];
  DirectEWMAX<Stability_shift, Scale> _avg[N];
  
 public:

  RateEWMAX()				{ }

  // note: must be 'signed int'
  int average(unsigned which = 0) const	{ return _avg[which].average(); }
  int rate(unsigned which = 0) const;
  
  static const int stability_shift = Stability_shift;
  static const int scale = Scale;
  static unsigned now()			{ return Timer::now(); }
  static unsigned freq()		{ return Timer::freq(); }

  String unparse(unsigned which = 0) const;
  void initialize();
 
  inline void update_time(unsigned now);
  inline void update_time();
  inline void update_now(int delta, unsigned which = 0);
  inline void update(int delta, unsigned which = 0);
  
};

struct JiffiesTimer {
  static unsigned now()			{ return click_jiffies(); }
  static unsigned freq()                { return CLICK_HZ; }
};

typedef DirectEWMAX<4, 10> DirectEWMA;
typedef DirectEWMAX<3, 10> FastDirectEWMA;
typedef RateEWMAX<4, 10, 1, JiffiesTimer> RateEWMA;

template <unsigned stability_shift, unsigned scale>
inline void
DirectEWMAX<stability_shift, scale>::update_with(unsigned val)
{
  int val_scaled = val << scale;
  int compensation = 1 << (stability_shift - 1); // round off
  _avg += static_cast<int>(val_scaled - _avg + compensation) >> stability_shift;
  // XXX implementation-defined right shift behavior
}

template <unsigned stability_shift, unsigned scale, unsigned n, class Timer>
inline void
RateEWMAX<stability_shift, scale, n, Timer>::initialize()
{
  _now_time = now();
  for (unsigned i = 0; i < n; i++) {
    _total[i] = 0;
    _avg[i].clear();
  }
}

template <unsigned stability_shift, unsigned scale, unsigned n, class Timer>
inline void
RateEWMAX<stability_shift, scale, n, Timer>::update_time(unsigned now)
{
  unsigned jj = _now_time;
  if (now != jj) {
    for (unsigned i = 0; i < n; i++) {
      // adjust the average rate using the last measured packets
      _avg[i].update_with(_total[i]);

      // adjust for time w/ no packets
      if (jj + 1 != now)
        _avg[i].update_zero_period(now - jj - 1);
      _total[i] = 0;
    }
    _now_time = now;
  }
}

template <unsigned stability_shift, unsigned scale, unsigned n, class Timer>
inline void 
RateEWMAX<stability_shift, scale, n, Timer>::update_now(int delta, 
                                                        unsigned which)
{ 
  _total[which] += delta; 
}

template <unsigned stability_shift, unsigned scale, unsigned n, class Timer>
inline void
RateEWMAX<stability_shift, scale, n, Timer>::update_time()
{
  update_time(now());
}

template <unsigned stability_shift, unsigned scale, unsigned n, class Timer>
inline void
RateEWMAX<stability_shift, scale, n, Timer>::update(int delta, unsigned which)
{
  update_time();
  update_now(delta, which);
}

template <unsigned stability_shift, unsigned scale, unsigned n, class Timer>
inline int
RateEWMAX<stability_shift, scale, n, Timer>::rate(unsigned which) const
{
  return (average(which) * Timer::freq()) >> scale;
}

template <unsigned stability_shift, unsigned scale, unsigned n, class Timer>
inline String
RateEWMAX<stability_shift, scale, n, Timer>::unparse(unsigned which) const
{
  return cp_unparse_real(average(which) * Timer::freq(), scale);
}

#endif
