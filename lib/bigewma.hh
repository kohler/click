#ifndef BIGEWMA_HH
#define BIGEWMA_HH
#include <click/glue.hh>
#include <click/ewma.hh>
CLICK_DECLS

template <unsigned Stability_shift, unsigned Scale>
class DirectBigEWMAX {
  
  uint64_t _avg;
  
 public:

  DirectBigEWMAX()			{ _avg = 0; }

  uint64_t average() const		{ return _avg; }
  static const unsigned stability_shift = Stability_shift;
  static const unsigned scale = Scale;
  
  void clear()				{ _avg = 0; }
  
  inline void update_with(uint64_t);
  void update_zero_period(unsigned);
};

template <unsigned Stability_shift, unsigned Scale, unsigned N, class Timer>
class RateBigEWMAX {
  
  unsigned _now_time;
  uint64_t _total[N];
  DirectBigEWMAX<Stability_shift, Scale> _avg[N];
  
 public:

  RateBigEWMAX()				{ }

  uint64_t average(unsigned which = 0) const	
  					{ return _avg[which].average(); }
  static const int stability_shift = Stability_shift;
  static const int scale = Scale;
  static unsigned now()			{ return Timer::now(); }
  static unsigned freq()		{ return Timer::freq(); }
  
  void initialize();
 
  inline void update_time(unsigned now);
  inline void update_time();
  inline void update_now(int64_t delta, unsigned which = 0);
  inline void update(int64_t delta, unsigned which = 0);
};

typedef DirectBigEWMAX<4, 10> DirectBigEWMA;
typedef RateBigEWMAX<4, 10, 1, JiffiesTimer> RateBigEWMA;

template <unsigned stability_shift, unsigned scale>
inline void
DirectBigEWMAX<stability_shift, scale>::update_with(uint64_t val)
{
  int val_scaled = val << scale;
  int compensation = 1 << (stability_shift - 1); // round off
  _avg += static_cast<int64_t>
    (val_scaled - _avg + compensation) >> stability_shift;
  // XXX implementation-defined right shift behavior
}

template <unsigned stability_shift, unsigned scale, unsigned n, class Timer>
inline void
RateBigEWMAX<stability_shift, scale, n, Timer>::initialize()
{
  _now_time = now();
  for (unsigned i = 0; i < n; i++) {
    _total[i] = 0;
    _avg[i].clear();
  }
}

template <unsigned stability_shift, unsigned scale, unsigned n, class Timer>
inline void
RateBigEWMAX<stability_shift, scale, n, Timer>::update_time(unsigned now)
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
RateBigEWMAX<stability_shift, scale, n, Timer>::update_now(int64_t delta, 
                                                           unsigned which)
{ 
  _total[which] += delta; 
}

template <unsigned stability_shift, unsigned scale, unsigned n, class Timer>
inline void
RateBigEWMAX<stability_shift, scale, n, Timer>::update_time()
{
  update_time(now());
}

template <unsigned stability_shift, unsigned scale, unsigned n, class Timer>
inline void
RateBigEWMAX<stability_shift, scale, n, Timer>::update(int64_t delta, 
                                                       unsigned which)
{
  update_time();
  update_now(delta, which);
}

CLICK_ENDDECLS
#endif
