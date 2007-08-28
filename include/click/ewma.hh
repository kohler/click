#ifndef CLICK_EWMA_HH
#define CLICK_EWMA_HH
#include <click/glue.hh>
#include <click/confparse.hh>
CLICK_DECLS

/** @file <click/ewma.hh>
 *  @brief  Click's classes for supporting exponentially weighted moving
 *  averages.
 */

/** @class DirectEWMAX include/click/ewma.hh <click/ewma.hh>
 *  @brief  An exponentially weighted moving average.
 *
 *  The DirectEWMAX template class represents a simple exponentially weighted
 *  moving average.  The average starts out with value 0.  The update()
 *  function adds a new observation to the average.
 *
 *  The template parameter P defines three EWMA parameters: value type,
 *  stability shift, and scale factor.
 *
 *  The value type is simply the unsigned integral type used to store the
 *  average.  It is also the type of each observation.  <code>unsigned</code>
 *  and <code>uint64_t</code> are examples.
 *
 *  The stability shift specifies alpha, the stability parameter.  Concretely,
 *  alpha = 1. / (2 ** stability_shift).  Thus, a stability shift of 4
 *  corresponds to an alpha of 1/16.
 *
 *  The scaling factor specifies how many bits of fraction are kept per
 *  observation.  Fraction bits are necessary to account for small differences
 *  between observations.  For example, consider a EWMA with value 0, alpha
 *  1/16, and 0 bits of fraction.  Assume the EWMA begins to observe a stream
 *  of observations equal to 1.  Despite these observations, the EWMA's value
 *  will never change from 0, since the 1/16 alpha factor rounds the new
 *  observations down to 0.  At least 4 bits of fraction are required to
 *  account for this difference.  There is a tradeoff: the more bits of
 *  fraction, the more precise the EWMA, but the less bits available to
 *  account for large values.
 *
 *  These EWMA parameters are defined by five of P's members, two typedefs and
 *  three static member functions.
 *
 *  <dl>
 *  <dt><strong>P::value_type</strong></dt>
 *  <dd>The EWMA's value type.  Example: <code>unsigned</code>.</dd>
 *
 *  <dt><strong>P::signed_value_type</strong></dt>
 *  <dd>The signed version of <code>P::value_type</code>.  Used internally.
 *  Example: <code>int</code>.</dd>
 *
 *  <dt><strong>unsigned P::stability_shift(const DirectEWMAX<P> &e)</strong></dt>
 *  <dd>This static function should return this EWMA's stability shift
 *  (see below).</dd>
 *
 *  <dt><strong>unsigned P::scale(const DirectEWMAX<P> &e)</strong></dt>
 *  <dd>This static function should return this EWMA's scaling factor
 *  (see below).</dd>
 *
 *  <dt><strong>unsigned P::compensation(const DirectEWMAX<P> &e)</strong></dt>
 *  <dd>This static function should return this EWMA's stability compensation,
 *  which normally equals 1 @<@< (stability_shift - 1).</dd>
 *  </dl>
 *
 *  The FixedEWMAXParameters type is a good template argument for DirectEWMAX.
 *
 *  @sa StabilityEWMAX, RateEWMAX
 */
template <typename P>
class DirectEWMAX { public:

    typedef typename P::value_type value_type;
    
    /** @brief  Create a EWMA with initial value 0. */
    DirectEWMAX()
	: _avg(0) {
    }

    /** @brief  Return the current scaled moving average.
     *  @note   The returned value is scaled by the factor Scale; that is,
     *		it has Scale bits of fraction. */
    value_type scaled_average() const {
	return _avg;
    }

    /** @brief  Return the current moving average.
     *  @note   The returned value is unscaled. */
    value_type unscaled_average() const {
	return (_avg + P::compensation(*this)) >> P::scale(*this);
    }

    /** @brief  Reset the EWMA to value 0. */
    void clear() {
	_avg = 0;
    }

    /** @brief  Update the moving average with a new observation.
     *  @param  value  the observation (unscaled) */
    inline void update(value_type value);

    /** @brief  Update the moving average with @a count identical observations.
     *  @param  value  the observation (unscaled)
     *  @param  count  number of observations
     *  @note   This may be faster than calling update(@a value) @a count
     *  times. */
    void update_n(value_type value, unsigned count);

    /** @brief  Unparse the current average into a String.
     *  @note   The returned value is unscaled, but may contain a fractional
     *  part. */
    String unparse() const;

    /** @brief  Update the moving average with a new observation (deprecated).
     *  @param  value  the observation (unscaled)
     *  @deprecated  Use update() instead. */
    inline void update_with(value_type value) CLICK_DEPRECATED {
	update(value);
    }

  private:
  
    value_type _avg;
  
};


template <typename P>
inline void
DirectEWMAX<P>::update(value_type val)
{
    value_type val_scaled = (val << P::scale(*this)) + P::compensation(*this);
    unsigned stability = P::stability_shift(*this);
#if HAVE_ARITHMETIC_RIGHT_SHIFT
    _avg += static_cast<typename P::signed_value_type>(val_scaled - _avg) >> stability;
#else
    if (val_scaled < _avg)
	_avg -= (_avg - val_scaled) >> stability;
    else
	_avg += (val_scaled - _avg) >> stability;
#endif
}

template <typename P>
void
DirectEWMAX<P>::update_n(value_type value, unsigned count)
{
    // XXX use table lookup
    value_type val_scaled = value << P::scale(*this);
    if (count >= 100)
	_avg = val_scaled;
    else {
	val_scaled += P::compensation(*this);
	unsigned stability = P::stability_shift(*this);
#if HAVE_ARITHMETIC_RIGHT_SHIFT
	for (; count > 0; count--)
	    _avg += static_cast<typename P::signed_value_type>(val_scaled - _avg) >> stability;
#else
	if (val_scaled < _avg)
	    for (; count > 0; count--)
		_avg -= (_avg - val_scaled) >> stability;
	else
	    for (; count > 0; count--)
		_avg += (val_scaled - _avg) >> stability;
#endif
    }
}

template <typename P>
inline String
DirectEWMAX<P>::unparse() const
{
    return cp_unparse_real2(scaled_average(), P::scale(*this));
}


/** @class FixedEWMAXParameters include/click/ewma.hh <click/ewma.hh>
 *  @brief  Parameters for a EWMA with constant scaling factor and stability
 *  shift.
 *
 *  The FixedEWMAXParameters template class is used as a template argument to
 *  DirectEWMAX.  It defines a EWMA with fixed constant scaling factor and
 *  stability shift.  FixedEWMAXParameters's first template argument is the
 *  EWMA's stability shift, its second template argument is the EWMA's scaling
 *  factor, its third template argument is the EWMA's value type, and the
 *  fourth template argument is the EWMA's signed value type.
 *
 *  Example 1: <code>DirectEWMAX@<FixedEWMAXParameters@<4, 10, unsigned, int@>
 *  @></code> defines a EWMA with alpha 1/16 (stability shift 4), scaling
 *  factor 10, and value type unsigned.  (These are the default parameters
 *  available in the DirectEWMA typedef.)
 *
 *  Example 2: <code>DirectEWMAX@<FixedEWMAXParameters@<3, 10, uint64_t,
 *  int64_t@> @></code> defines a EWMA with alpha 1/8 (stability shift 3),
 *  scaling factor 10, and value type uint64_t.
 */
template <unsigned STABILITY, unsigned SCALE, typename T = unsigned, typename U = int>
class FixedEWMAXParameters { public:

    typedef T value_type;
    typedef U signed_value_type;
    
    static unsigned stability_shift(const DirectEWMAX<FixedEWMAXParameters> &) {
	return STABILITY;
    }

    static unsigned scale(const DirectEWMAX<FixedEWMAXParameters> &) {
	return SCALE;
    }

    static unsigned compensation(const DirectEWMAX<FixedEWMAXParameters> &) {
	return 1 << (STABILITY - 1);
    }
    
};

typedef DirectEWMAX<FixedEWMAXParameters<4, 10> > DirectEWMA;
typedef DirectEWMAX<FixedEWMAXParameters<3, 10> > FastDirectEWMA;



/** @class StabilityEWMAX include/click/ewma.hh <click/ewma.hh>
 *  @brief  An exponentially weighted moving average with user-settable alpha.
 *
 *  The StabilityEWMAX template class is a type of EWMA with a user-settable
 *  alpha value.  The alpha value is defined by a stability shift.
 *  StabilityEWMAX<P> is a subclass of DirectEWMAX<P>.
 *
 *  As in DirectEWMAX, the template parameter P defines the EWMA's value type,
 *  stability shift, and scale factor.  A StabilityEWMAX should use
 *  StabilityEWMAXParameters as its template parameter.
 *
 *  @sa StabilityEWMAXParameters, DirectEWMAX
 */
template <typename P>
class StabilityEWMAX : public DirectEWMAX<P> { public:

    /** @brief  Create a EWMA with initial value 0 and initial alpha 1/16. */
    StabilityEWMAX()
	: _stability(4) {
    }

    /** @brief  Return the current stability shift.
     *
     *  The current alpha equals 1. / (2 ** stability_shift()). */
    unsigned stability_shift() const {
	return _stability;
    }

    /** @brief  Set the current stability shift.
     *  @param  stability_shift  new value */
    void set_stability_shift(unsigned stability_shift) {
	_stability = stability_shift;
    }

  private:

    unsigned _stability;

};

/** @class StabilityEWMAXParameters include/click/ewma.hh <click/ewma.hh>
 *  @brief  Parameters for a StabilityEWMA with constant scaling factor.
 *
 *  The StabilityEWMAXParameters template class is used as a template argument
 *  to StabilityEWMAX.  It defines a EWMA with fixed constant scaling factor.
 *  StabilityEWMAXParameters's first template argument is the EWMA's scaling
 *  factor, its second template argument is the EWMA's value type, and the
 *  third template argument is the EWMA's signed value type.
 *
 *  Example: <code>StabilityEWMAX@<StabilityEWMAXParameters@<10, unsigned,
 *  int@> @></code> defines a EWMA with user-settable alpha (stability shift),
 *  scaling factor 10, and value type unsigned.
 */
template <unsigned SCALE, typename T = unsigned, typename U = int>
class StabilityEWMAXParameters { public:

    typedef T value_type;
    typedef U signed_value_type;

    static unsigned stability_shift(const DirectEWMAX<StabilityEWMAXParameters> &s) {
	return static_cast<const StabilityEWMAX<StabilityEWMAXParameters> &>(s).stability_shift();
    }

    static unsigned scale(const DirectEWMAX<StabilityEWMAXParameters> &) {
	return SCALE;
    }

    static unsigned compensation(const DirectEWMAX<StabilityEWMAXParameters> &s) {
	return 1 << (stability_shift(s) - 1);
    }

};


template <unsigned Stability_shift, unsigned Scale, unsigned N, class Timer>
class RateEWMAX { public:

    enum { stability_shift = Stability_shift,
	   scale = Scale };
    
    RateEWMAX() {
    }

    // note: must be 'signed int'
    int scaled_average(unsigned which = 0) const {
	return _avg[which].scaled_average();
    }
    int rate(unsigned which = 0) const;

    static unsigned now() {
	return Timer::now();
    }
    
    static unsigned freq() {
	return Timer::freq();
    }

    String unparse(unsigned which = 0) const;
    void initialize();
 
    inline void update_time(unsigned now);
    inline void update_time();
    inline void update_now(int delta, unsigned which = 0);
    inline void update(int delta, unsigned which = 0);
  
  private:
  
    unsigned _now_time;
    unsigned _total[N];
    DirectEWMAX<FixedEWMAXParameters<Stability_shift, Scale> > _avg[N];
  
};

struct JiffiesTimer {
    static unsigned now() {
	return click_jiffies();
    }
    
    static unsigned freq() {
	return CLICK_HZ;
    }
};

typedef RateEWMAX<4, 10, 1, JiffiesTimer> RateEWMA;

template <unsigned Stability_shift, unsigned Scale, unsigned N, class Timer>
inline void
RateEWMAX<Stability_shift, Scale, N, Timer>::initialize()
{
  _now_time = now();
  for (unsigned i = 0; i < N; i++) {
    _total[i] = 0;
    _avg[i].clear();
  }
}

template <unsigned Stability_shift, unsigned Scale, unsigned N, class Timer>
inline void
RateEWMAX<Stability_shift, Scale, N, Timer>::update_time(unsigned now)
{
  unsigned jj = _now_time;
  if (now != jj) {
    for (unsigned i = 0; i < N; i++) {
      // adjust the average rate using the last measured packets
      _avg[i].update(_total[i]);

      // adjust for time w/ no packets
      if (jj + 1 != now)
	  _avg[i].update_n(0, now - jj - 1);
      _total[i] = 0;
    }
    _now_time = now;
  }
}

template <unsigned Stability_shift, unsigned Scale, unsigned N, class Timer>
inline void 
RateEWMAX<Stability_shift, Scale, N, Timer>::update_now(int delta, 
                                                        unsigned which)
{ 
  _total[which] += delta; 
}

template <unsigned Stability_shift, unsigned Scale, unsigned N, class Timer>
inline void
RateEWMAX<Stability_shift, Scale, N, Timer>::update_time()
{
  update_time(now());
}

template <unsigned Stability_shift, unsigned Scale, unsigned N, class Timer>
inline void
RateEWMAX<Stability_shift, Scale, N, Timer>::update(int delta, unsigned which)
{
  update_time();
  update_now(delta, which);
}

template <unsigned Stability_shift, unsigned Scale, unsigned N, class Timer>
inline int
RateEWMAX<Stability_shift, Scale, N, Timer>::rate(unsigned which) const
{
    return (scaled_average(which) * Timer::freq()) >> Scale;
}

template <unsigned Stability_shift, unsigned Scale, unsigned N, class Timer>
inline String
RateEWMAX<Stability_shift, Scale, N, Timer>::unparse(unsigned which) const
{
    return cp_unparse_real2(scaled_average(which) * Timer::freq(), Scale);
}

CLICK_ENDDECLS
#endif
