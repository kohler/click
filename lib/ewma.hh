#ifndef EWMA_HH
#define EWMA_HH
#include "glue.hh"

template <int Stability_shift, int Scale>
class DirectEWMAX {
  
  int _avg;
  
 public:

  DirectEWMAX()				{ _avg = 0; }

  int average() const			{ return _avg; }
  static const int stability_shift = Stability_shift;
  static const int scale = Scale;
  
  void clear()				{ _avg = 0; }
  
  inline void update_with(int);
  void update_zero_period(unsigned);
  
};

template <int Stability_shift, int Scale, int N, class Timer>
class RateEWMAX {
  
  unsigned _now_time;
  int _total[N];
  DirectEWMAX<Stability_shift, Scale> _avg[N];
  
 public:

  RateEWMAX()				{ }

  int average(unsigned which = 0) const	{ return _avg[which].average(); }
  static const int stability_shift = Stability_shift;
  static const int scale = Scale;
  static unsigned now()			{ return Timer::now(); }
  static unsigned freq()		{ return Timer::freq(); }
  
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
typedef RateEWMAX<4, 10, 1, JiffiesTimer> RateEWMA;

template <int stability_shift, int scale>
inline void
DirectEWMAX<stability_shift, scale>::update_with(int val)
{
  int val_scaled = val << scale;
  int compensation = 1 << (stability_shift - 1); // round off
  _avg += (val_scaled - _avg + compensation) >> stability_shift;
  // XXX implementation-defined right shift behavior
}

template <int stability_shift, int scale, int n, class Timer>
inline void
RateEWMAX<stability_shift, scale, n, Timer>::initialize()
{
  _now_time = now();
  for (int i = 0; i < n; i++) {
    _total[i] = 0;
    _avg[i].clear();
  }
}

template <int stability_shift, int scale, int n, class Timer>
inline void
RateEWMAX<stability_shift, scale, n, Timer>::update_time(unsigned now)
{
  unsigned jj = _now_time;
  if (now != jj) {
    for (int i = 0; i < n; i++) {
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
  
template <int stability_shift, int scale, int n, class Timer>
inline void 
RateEWMAX<stability_shift, scale, n, Timer>::update_now(int delta, 
                                                        unsigned which)
{ 
  _total[which] += delta; 
}

template <int stability_shift, int scale, int n, class Timer>
inline void
RateEWMAX<stability_shift, scale, n, Timer>::update_time()
{
  update_time(now());
}

template <int stability_shift, int scale, int n, class Timer>
inline void
RateEWMAX<stability_shift, scale, n, Timer>::update(int delta, unsigned which)
{
  update_time();
  update_now(delta, which);
}

#endif
