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

template <int Stability_shift, int Scale, class Timer>
class RateEWMAX {
  
  unsigned _now_time;
  int _now;
  DirectEWMAX<Stability_shift, Scale> _avg;
  
 public:

  RateEWMAX()				{ }

  int average() const			{ return _avg.average(); }
  static const int stability_shift = Stability_shift;
  static const int scale = Scale;
  static unsigned now()			{ return Timer::now(); }
  static unsigned freq()		{ return Timer::freq(); }
  
  void initialize();
  void initialize(unsigned now);
  
  inline void update_time(unsigned now);
  inline void update_now(int delta)	{ _now += delta; }
  inline void update(unsigned now, int delta);
  
  inline void update_time();
  inline void update(int delta);
  
};

struct JiffiesTimer {
  static unsigned now()			{ return click_jiffies(); }
  static unsigned freq()                { return CLICK_HZ; }
};

typedef DirectEWMAX<4, 10> DirectEWMA;
typedef RateEWMAX<4, 10, JiffiesTimer> RateEWMA;

template <int stability_shift, int scale>
inline void
DirectEWMAX<stability_shift, scale>::update_with(int val)
{
  int val_scaled = val << scale;
  int compensation = 1 << (stability_shift - 1); // round off
  _avg += (val_scaled - _avg + compensation) >> stability_shift;
  // XXX implementation-defined right shift behavior
}

template <int stability_shift, int scale, class Timer>
inline void
RateEWMAX<stability_shift, scale, Timer>::initialize()
{
  _now_time = now();
  _now = 0;
  _avg.clear();
}

template <int stability_shift, int scale, class Timer>
inline void
RateEWMAX<stability_shift, scale, Timer>::update_time(unsigned now)
{
  unsigned jj = _now_time;
  if (now != jj) {
    // adjust the average rate using the last measured packets
    _avg.update_with(_now);

    // adjust for time w/ no packets
    if (jj + 1 != now)
      _avg.update_zero_period(now - jj - 1);
    
    _now_time = now;
    _now = 0;
  }
}

template <int stability_shift, int scale, class Timer>
inline void
RateEWMAX<stability_shift, scale, Timer>::update(unsigned now, int delta)
{
  update_time(now);
  update_now(delta);
}

template <int stability_shift, int scale, class Timer>
inline void
RateEWMAX<stability_shift, scale, Timer>::update_time()
{
  update_time(now());
}

template <int stability_shift, int scale, class Timer>
inline void
RateEWMAX<stability_shift, scale, Timer>::update(int delta)
{
  update(now(), delta);
}

#endif
