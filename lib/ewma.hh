#ifndef EWMA_HH
#define EWMA_HH
#include "glue.hh"

class EWMA {
  
  int _now_jiffies;
  int _now;
  int _avg;
  int _stability_shift;

  static const int METER_SCALE = 10;
  
 public:

  EWMA();

  int average() const			{ return _avg; }
  int stability_shift() const		{ return _stability_shift; }
  int scale() const			{ return METER_SCALE; }

  int set_stability_shift(int);

  void initialize();
  
  inline void update_with(int);
  void update_zero_period(int);
  inline void update_time();
  inline void update_now(int delta)	{ _now += delta; }
  inline void update(int delta);
  
};


inline void
EWMA::initialize()
{
  _now_jiffies = click_jiffies();
  _avg = 0;
  _now = 0;
}

inline void
EWMA::update_with(int val)
{
  int val_scaled = val << METER_SCALE;
  int compensation = 1 << (_stability_shift - 1); // round off
  _avg += (val_scaled - _avg + compensation) >> _stability_shift;
}

inline void
EWMA::update_time()
{
  int j = click_jiffies();
  int jj = _now_jiffies;
  if (j != jj) {
    // adjust the average rate using the last measured packets
    int now_scaled = _now << METER_SCALE;
    int compensation = 1 << (_stability_shift - 1); // round off
    _avg += (now_scaled - _avg + compensation) >> _stability_shift;

    // adjust for time w/ no packets (XXX: should use table)
    // (inline copy of update_zero_period)
    for (int t = jj + 1; t != j; t++)
      _avg += (-_avg + compensation) >> _stability_shift;
    
    _now_jiffies = j;
    _now = 0;
  }
}

inline void
EWMA::update(int delta)
{
  update_time();
  update_now(delta);
}

#endif
