#ifndef EWMA2_HH
#define EWMA2_HH
#include "glue.hh"

/*
 * Exponential Weighted Moving Average: unlike EWMA, EWMA2 class uses long
 * long type and has a METER SCALE of 30. This implies that the stability
 * factor, which correlates to time period of the average, can be in order or
 * minutes, and values kept track can be large (e.g. bytes/second).
 *
 * The formula to compute EWMA time period is 
 *
 *     2 / (T + 1) = p
 *
 * where p is used in the following calculation:
 *
 *     EWMA@T = p * Value@T + (1 - p) * EWMA@T-1
 *
 * since linux kernel does not have long long multiply and divide (part of
 * libgcc), we uses shifts instead of multiplier p:
 *
 *     EWMA@T = (Value@T - EWMA@T-1) / x + EWMA@T-1
 *
 * where x is a power of 2. It happens that 1/x = p, or x = (T+1)/2.  The
 * stability factor is log_2(x).
 *
 * Note: since we use shifts instead of mul/div for dividing x, rates become
 * less accurate as desired time period becomes higher: rate over the past hr,
 * for example, uses the same stability factor as rate over the past 1.5 hrs.
 * The smaller the time period, the more accurate EWMA2 can be.
 */

class EWMA2 {
  
  int _now_jiffies;
  int _stability_shift;
  long long _now;
  long long _avg;

  static const int METER_SCALE = 30;
  static const int FRAC_BITS   = 10;
  
  inline int set_stability_shift(int);
  inline void update_time();
  inline void update_now(int delta)	{ _now += delta; }
  
 public:

  int average() const	{ return (int) (_avg >> (METER_SCALE-FRAC_BITS)); }
  int stability_shift() const 		{ return _stability_shift; }
  int scale() const			{ return FRAC_BITS; }
  int now() const			{ return _now_jiffies; }

  void initialize(int seconds);
  inline void update(int delta);
};

inline void
EWMA2::update_time()
{
  int j = click_jiffies();
  int jj = _now_jiffies;
  if (j != jj) {
    // adjust the average rate using the last measured packets
    long long now_scaled = _now << METER_SCALE;
    int compensation = 1 << (_stability_shift - 1); // round off
    _avg += (now_scaled - _avg + compensation) >> _stability_shift;

    // adjust for time w/ no packets
    for (int t = jj + 1; t != j; t++)
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
  int j;
  for (j=0; j<METER_SCALE; j++) {
    if ((1<<j) > ((seconds*CLICK_HZ+1)/2)) break;
  }
  if (j == METER_SCALE) j--;
  set_stability_shift(j);
}

#endif
