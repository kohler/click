#ifndef EWMA2_HH
#define EWMA2_HH
#include "glue.hh"

class EWMA2 {
  
  int _now_jiffies;
  int _now;
  int _avg;
  int _stability_shift;

  static const int METER_SCALE = 12;
  
  inline int set_stability_shift(int);
  inline void update_time();
  inline void update_now(int delta)	{ _now += delta; }
  
 public:

  EWMA2() {}

  int average() const			{ return _avg; }
  int stability_shift() const 		{ return _stability_shift; }
  int scale() const			{ return METER_SCALE; }

  void initialize(int seconds);
  inline void update(int delta);
};

inline void
EWMA2::update_time()
{
  int j = click_jiffies();
  int jj = _now_jiffies;
  if (j >= jj+CLICK_HZ) {
    // adjust the average rate using the last measured packets
    int now_scaled = _now << METER_SCALE;
    int compensation = 1 << (_stability_shift - 1); // round off
    _avg += (now_scaled - _avg + compensation) >> _stability_shift;

    // adjust for time w/ no packets (XXX: should use table)
    // (inline copy of update_zero_period)
    for (int t = jj + CLICK_HZ; t < j; t+= CLICK_HZ)
      _avg += (-_avg + compensation) >> _stability_shift;
    
    _now_jiffies = j;
    _now = 0;
  }
}

inline void
EWMA2::update(int delta)
{
  update_time();
  update_now(delta);
}
  
inline int 
EWMA2::set_stability_shift(int shift)
{
  click_chatter("setting stability_shift to %d\n", shift);
  if (shift > METER_SCALE-1)
    return -1;
  else {
    _stability_shift = shift;
    return 0;
  }
}

inline void
EWMA2::initialize(int seconds)
{
  _now_jiffies = click_jiffies();
  _avg = 0;
  _now = 0;
  for (int j=0; j<METER_SCALE; j++) {
    if ((1<<j) > ((seconds+1)/2)) break;
  }
  if (j == METER_SCALE) j--;
  set_stability_shift(j);
}

#endif
