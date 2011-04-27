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
 *  three possibly static member functions.
 *
 *  <dl>
 *  <dt><strong>P::value_type</strong></dt>
 *  <dd>The EWMA's value type.  Example: <code>unsigned</code>.</dd>
 *
 *  <dt><strong>P::signed_value_type</strong></dt>
 *  <dd>The signed version of <code>P::value_type</code>.  Used internally.
 *  Example: <code>int</code>.</dd>
 *
 *  <dt><strong>unsigned P::stability_shift()</strong></dt>
 *  <dd>This function should return this EWMA's stability shift
 *  (see above).</dd>
 *
 *  <dt><strong>unsigned P::scale()</strong></dt>
 *  <dd>This function should return this EWMA's scaling factor
 *  (see above).</dd>
 *
 *  <dt><strong>unsigned P::compensation()</strong></dt>
 *  <dd>This function should return this EWMA's stability compensation,
 *  which normally equals 1 @<@< (stability_shift - 1).</dd>
 *  </dl>
 *
 *  Since DirectEWMAX inherits from an object of type P, these members are
 *  also directly available to callers.
 *
 *  The FixedEWMAXParameters and StabilityEWMAXParameters types are good
 *  template arguments for DirectEWMAX.
 *
 *  @sa RateEWMAX
 */
template <typename P>
class DirectEWMAX : public P { public:

    typedef typename P::value_type value_type;

    /** @brief Construct a EWMA with initial average 0. */
    DirectEWMAX()
	: _avg(0) {
    }

    /** @brief Construct a EWMA with initial scaled average @a scaled_value. */
    DirectEWMAX(value_type scaled_value)
	: _avg(scaled_value) {
    }

    /** @brief Return the current scaled moving average.
     *  @note The returned value has scale() bits of fraction. */
    value_type scaled_average() const {
	return _avg;
    }

    /** @brief Return the current moving average, rounded up.
     *  @note The returned value is unscaled (has zero bits of fraction). */
    value_type unscaled_average() const {
	return (_avg + (P::scaled_one() >> 1)) >> P::scale();
    }

    /** @brief Reset the EWMA to value 0. */
    void clear() {
	_avg = 0;
    }

    /** @brief  Assign the EWMA to scaled average @a scaled_value. */
    inline void assign(value_type scaled_value) {
	_avg = scaled_value;
    }

    /** @brief  Update the moving average with a new observation.
     *  @param  x  the observation (unscaled) */
    inline void update(value_type x);

    /** @brief  Update the moving average with @a n identical observations.
     *  @param  x  the observation (unscaled)
     *  @param  n  number of observations
     *  @note   This may be faster than calling update(@a x) @a n
     *  times. */
    void update_n(value_type x, unsigned n);

    /** @brief  Unparse the current average into a String.
     *  @note   The returned value is unscaled, but may contain a fractional
     *  part. */
    String unparse() const;

    /** @brief  Update the moving average with a new observation (deprecated).
     *  @param  x  the observation (unscaled)
     *  @deprecated  Use update() instead. */
    inline void update_with(value_type x) CLICK_DEPRECATED;

  private:

    value_type _avg;

};

template <typename P>
inline void
DirectEWMAX<P>::update(value_type x)
{
    value_type x_scaled = (x << P::scale()) + P::compensation();
    unsigned stability = P::stability_shift();
#if HAVE_ARITHMETIC_RIGHT_SHIFT
    _avg += static_cast<typename P::signed_value_type>(x_scaled - _avg) >> stability;
#else
    if (x_scaled < _avg)
	_avg -= (_avg - x_scaled) >> stability;
    else
	_avg += (x_scaled - _avg) >> stability;
#endif
}

template <typename P>
void
DirectEWMAX<P>::update_n(value_type x, unsigned n)
{
    // XXX use table lookup
    value_type x_scaled = x << P::scale();
    if (n >= 100)
	_avg = x_scaled;
    else {
	x_scaled += P::compensation();
	unsigned stability = P::stability_shift();
#if HAVE_ARITHMETIC_RIGHT_SHIFT
	for (; n > 0; n--)
	    _avg += static_cast<typename P::signed_value_type>(x_scaled - _avg) >> stability;
#else
	if (x_scaled < _avg)
	    for (; n > 0; n--)
		_avg -= (_avg - x_scaled) >> stability;
	else
	    for (; n > 0; n--)
		_avg += (x_scaled - _avg) >> stability;
#endif
    }
}

template <typename P>
inline String
DirectEWMAX<P>::unparse() const
{
    return cp_unparse_real2(scaled_average(), P::scale());
}

template <typename P>
inline void
DirectEWMAX<P>::update_with(value_type x)
{
    update(x);
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

    /** @brief  Return this EWMA's stability shift.
     *  @return  the 1st template parameter */
    static unsigned stability_shift() {
	return STABILITY;
    }

    /** @brief  Return this EWMA's scaling factor (bits of fraction).
     *  @return  the 2nd template parameter */
    static unsigned scale() {
	static_assert(SCALE < sizeof(T) * 8, "SCALE too big for EWMA type.");
	return SCALE;
    }

    /** @brief  Return this EWMA's scaled value for one. */
    static value_type scaled_one() {
	return (value_type) 1 << SCALE;
    }

    /** @brief  Return this EWMA's compensation.
     *  @return  1 << (stability_shift() - 1) */
    static unsigned compensation() {
	return 1 << (STABILITY - 1);
    }

};

/** @brief A DirectEWMAX with stability shift 4 (alpha 1/16), scaling factor
 *  10 (10 bits of fraction), and underlying type <code>unsigned</code>. */
typedef DirectEWMAX<FixedEWMAXParameters<4, 10> > DirectEWMA;

/** @brief A DirectEWMAX with stability shift 3 (alpha 1/8), scaling factor
 *  10 (10 bits of fraction), and underlying type <code>unsigned</code>. */
typedef DirectEWMAX<FixedEWMAXParameters<3, 10> > FastDirectEWMA;


/** @class StabilityEWMAXParameters include/click/ewma.hh <click/ewma.hh>
 *  @brief  Parameters for a EWMA with constant scaling factor
 *	    and user-settable alpha.
 *
 *  The StabilityEWMAXParameters template class is used as a template argument
 *  to DirectEWMAX.  It defines a EWMA with fixed constant scaling factor.
 *  StabilityEWMAXParameters's first template argument is the EWMA's scaling
 *  factor, its second template argument is the EWMA's value type, and the
 *  third template argument is the EWMA's signed value type.
 *
 *  Example: <code>DirectEWMAX@<StabilityEWMAXParameters@<10, unsigned, int@>
 *  @></code> defines a EWMA with user-settable alpha (stability shift)
 *  initially equal to 1/16, scaling factor 10, and value type unsigned.
 *
 *  A <code>DirectEWMAX@<StabilityEWMAXParameters@<...@> @></code> object has
 *  stability_shift() and set_stability_shift() methods.
 */
template <unsigned SCALE, typename T = unsigned, typename U = int>
class StabilityEWMAXParameters { public:

    typedef T value_type;
    typedef U signed_value_type;

    /** @brief  Construct a StabilityEWMAXParameters with initial alpha 1/16. */
    StabilityEWMAXParameters()
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

    /** @brief  Return this EWMA's scaling factor (bits of fraction).
     *  @return  the 1st template parameter */
    static unsigned scale() {
	return SCALE;
    }

    /** @brief  Return this EWMA's scaled value for one. */
    static value_type scaled_one() {
	return (value_type) 1 << SCALE;
    }

    /** @brief  Return this EWMA's compensation.
     *  @return  1 << (stability_shift() - 1) */
    unsigned compensation() const {
	return 1 << (stability_shift() - 1);
    }

  private:

    unsigned _stability;

};



/** @class RateEWMAX include/click/ewma.hh <click/ewma.hh>
 *  @brief  An exponentially weighted moving average used to measure a rate.
 *
 *  The RateEWMAX template class represents an exponentially weighted moving
 *  average that measures a <em>rate</em>: a count of events per unit time.
 *  The average starts out with value 0.
 *
 *  RateEWMAX adds to DirectEWMAX a concept of epochs, which are periods of
 *  time.  A RateEWMAX object collects samples over the current epoch.  When
 *  the epoch closes, the collected sample count is used to update the moving
 *  average.  Thus, the moving average is measured in samples per epoch.  The
 *  rate() and unparse_rate() member functions return the rate in samples per
 *  <em>second</em>, rather than per epoch.  These functions use the epoch
 *  frequency to translate between epochs and seconds.
 *
 *  Note that it often makes sense to call update() before calling
 *  scaled_average(), rate(), or unparse_rate(), in case an epoch or two has
 *  passed and the average should take account of passing time.
 *
 *  The template parameter P defines the EWMA parameters required by
 *  DirectEWMAX, and three others: a rate count, an epoch measurement, and an
 *  epoch frequency.
 *
 *  The rate count is the number of rates measured per object.  Usually it is
 *  1.
 *
 *  The epoch measurement is a function that returns the current epoch as an
 *  unsigned number.  Epochs should increase monotonically.
 *
 *  The epoch frequency is the number of epochs per second, and is only used
 *  by rate() and unparse_rate().
 *
 *  These are defined by:
 *
 *  <dl>
 *  <dt><strong>P::rate_count</strong></dt>
 *  <dd>The rate count, as a static constant (for example, defined by an
 *  enum).</dd>
 *
 *  <dt><strong>unsigned P::epoch()</strong></dt>
 *  <dd>This function returns the current epoch number.</dd>
 *
 *  <dt><strong>unsigned P::epoch_frequency()</strong></dt>
 *  <dd>This function returns the number of epochs per second.</dd>
 *  </dl>
 *
 *  Since RateEWMAX inherits from an object of type P, these members are
 *  also directly available to callers.
 *
 *  The RateEWMAXParameters type is a good template argument for RateEWMAX.
 *
 *  @sa DirectEWMAX
 */
template <typename P>
class RateEWMAX : public P { public:

    typedef typename P::value_type value_type;
    typedef typename P::signed_value_type signed_value_type;

    /** @brief  Create a rate EWMA with initial value(s) 0. */
    RateEWMAX() {
	_current_epoch = P::epoch();
	for (unsigned i = 0; i < P::rate_count; i++)
	    _current[i] = 0;
    }

    /** @brief  Return the current scaled moving average.
     *  @param  ratenum  rate index (0 <= ratenum < rate_count)
     *  @note   The returned value has scale() bits of fraction.
     *  @note   scaled_average() does not check the current epoch.
     *		If an epoch might have passed since the last update(), you
     *		should call update(0, @a ratenum) before calling this
     *		function. */
    signed_value_type scaled_average(unsigned ratenum = 0) const {
	// note: return type must be signed!
	return _avg[ratenum].scaled_average();
    }

    /** @brief  Returns one of the average's scaling factors (bits of
     *		fraction). */
    unsigned scale(unsigned ratenum = 0) const {
	return _avg[ratenum].scale();
    }

    /** @brief  Return the current rate in samples per second.
     *  @param  ratenum  rate index (0 <= ratenum < rate_count)
     *  @note   The returned value is unscaled.
     *  @note   rate() does not check the current epoch.
     *		If an epoch might have passed since the last update(), you
     *		should call update(0, @a ratenum) before calling this
     *		function. */
    inline int rate(unsigned ratenum = 0) const;

    /** @brief  Update the sample count for the current epoch.
     *  @param  delta    increment for current epoch sample count
     *  @param  ratenum  rate index (0 <= ratenum < rate_count)
     *  @note   If the epoch has changed since the last update(),
     *		this function applies the last epoch's sample count (if any)
     *		to the relevant moving average, accounts for any passage of
     *		time (in case one or more epochs have passed with no samples),
     *		and clears the sample count for	the new epoch. */
    inline void update(signed_value_type delta, unsigned ratenum = 0);

    /** @brief  Unparse the current average into a String.
     *  @param  ratenum  rate index (0 <= ratenum < rate_count)
     *  @note   The returned value is unscaled, but may contain a fractional
     *  part.
     *  @note   unparse_rate() does not check the current epoch.
     *		If an epoch might have passed since the last update(), you
     *		should call update(0, @a ratenum) before calling this
     *		function. */
    String unparse_rate(unsigned ratenum = 0) const;

  private:

    unsigned _current_epoch;
    value_type _current[P::rate_count];
    DirectEWMAX<P> _avg[P::rate_count];

    inline void update_time(unsigned now);

};

/** @class RateEWMAXParameters include/click/ewma.hh <click/ewma.hh>
 *  @brief  Parameters for a RateEWMA with constant scaling factor
 *	    and alpha, one rate count, and epochs of jiffies.
 *
 *  The RateEWMAXParameters template class is used as a template argument
 *  to RateEWMAX.  It defines a EWMA with fixed constant scaling factor and
 *  alpha and one rate count.  The EWMA uses jiffies as epochs.  Template
 *  parameters are as for DirectEWMAXParameters.
 *
 *  Example: <code>RateEWMAX@<RateEWMAXParameters@<4, 10, unsigned, int@>
 *  @></code> defines a rate EWMA with user-settable alpha (stability shift)
 *  initially equal to 1/16, scaling factor 10, and value type unsigned.
 */
template <unsigned STABILITY, unsigned SCALE, typename T = unsigned, typename U = int>
class RateEWMAXParameters : public FixedEWMAXParameters<STABILITY, SCALE, T, U> { public:
    enum {
	rate_count = 1
    };

    /** @brief  Return the current epoch number.
     *  @note   RateEWMAXParameters measures epochs in jiffies. */
    static unsigned epoch() {
	return click_jiffies();
    }

    /** @brief  Return the number of epochs (jiffies) per second. */
    static unsigned epoch_frequency() {
	return CLICK_HZ;
    }
};

/** @brief A RateEWMAX with stability shift 4 (alpha 1/16), scaling factor 10
 *  (10 bits of fraction), one rate, and underlying type <code>unsigned</code>
 *  that measures epochs in jiffies. */
typedef RateEWMAX<RateEWMAXParameters<4, 10> > RateEWMA;


template <typename P>
inline void
RateEWMAX<P>::update_time(unsigned now)
{
    unsigned jj = _current_epoch;
    if (now != jj) {
	for (unsigned i = 0; i < P::rate_count; i++) {
	    // adjust the average rate using the last measured packets
	    _avg[i].update(_current[i]);

	    // adjust for time w/ no packets
	    if (jj + 1 != now)
		_avg[i].update_n(0, now - jj - 1);
	    _current[i] = 0;
	}
	_current_epoch = now;
    }
}

template <typename P>
inline void
RateEWMAX<P>::update(signed_value_type delta, unsigned ratenum)
{
    update_time(P::epoch());
    _current[ratenum] += delta;
}

template <typename P>
inline int
RateEWMAX<P>::rate(unsigned ratenum) const
{
    return (scaled_average(ratenum) * P::epoch_frequency()) >> _avg[ratenum].scale();
}

template <typename P>
inline String
RateEWMAX<P>::unparse_rate(unsigned ratenum) const
{
    return cp_unparse_real2(scaled_average(ratenum) * P::epoch_frequency(), _avg[ratenum].scale());
}

CLICK_ENDDECLS
#endif
