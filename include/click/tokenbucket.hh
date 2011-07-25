// -*- c-basic-offset: 4 -*-
#ifndef CLICK_TOKENBUCKET_HH
#define CLICK_TOKENBUCKET_HH
#include <click/timestamp.hh>
#include <click/bigint.hh>
CLICK_DECLS

/** @file <click/tokenbucket.hh>
 @brief  Token bucket rate limiter templates.

 Related template classes that support token bucket rate limiters.

 The TokenRateX template class represents a token bucket rate: a refill
 I<rate> in tokens per period, plus a I<capacity>, the maximum number of
 tokens allowed to accumulate.

 The TokenCounterX template class represents an active count of tokens.
 Member functions are provided to update the count according to a particular
 TokenRateX argument.  The counter will fill up with tokens according to the
 given rate, to a maximum of the capacity.

 A TokenRateX object's state depends only on the rate and capacity, and thus
 may be shared by several distinct TokenCounterX objects.  But for the common
 case that a single counter is paired with a unique rate, the TokenBucketX
 template class combines a TokenRateX and a TokenCounterX.

 The token bucket templates divide time into discrete units called I<ticks>.
 Token counters are refilled up to once per tick.  A tick may be less than a
 full period.  For example, if periods and ticks are 1 second and 1
 millisecond, respectively, then a TokenCounterX with associated rate 1000
 tokens per second would be refilled at 1 token per millisecond.  The
 TokenRateX template parameter P defines the time tick unit and frequency.
 The provided TokenBucketJiffyParameters class is designed to be used as
 TokenRateX's parameter; it measures ticks in units of jiffies.

 @sa GapRate */

/** @class TokenRateX include/click/tokenbucket.hh <click/tokenbucket.hh>
 @brief  Token bucket rate template.

 The TokenRateX class implements a token bucket rate.  TokenBucketX is
 initialized with a rate, in tokens per second, and a capacity in tokens.
 Associated token buckets fill up with tokens at the given rate, to a maximum
 of the capacity.

 Two special types of rate are supported.  An <em>unlimited</em> TokenRateX
 always refills associated counters to full capacity.  Its capacity() equals
 token_max.  An <em>idle</em> TokenRateX never refills.

 Most users will be satisfied with the TokenRate type, which is equal to
 TokenRateX<TokenBucketJiffyParameters<unsigned> >.

 @sa TokenCounterX, TokenBucketX */

template <typename P> class TokenRateX;

template <typename P>
class TokenRateX : public P { public:

    /** @brief The template parameter type. */
    typedef P parameter_type;

    /** @brief Unsigned type of token counts. */
    typedef typename P::token_type token_type;

    /** @brief Type of time points. */
    typedef typename P::time_point_type time_point_type;

    /** @brief Unsigned type of tick counts (differences between time points). */
    typedef typename make_unsigned<typename P::duration_type>::type ticks_type;

    enum {
	max_tokens = (token_type) -1
    };

    /** @brief Construct an idle token rate. */
    TokenRateX() {
	assign();
    }

    /** @brief Construct an idle or unlimited token rate.
     * @param unlimited idle if false, unlimited if true */
    explicit TokenRateX(bool unlimited) {
	assign(unlimited);
    }

    /** @brief Construct a token rate representing @a rate.
     * @param rate refill rate in tokens per period
     * @param capacity maximum token accumulation
     *
     * The rate is idle if either @a rate or @a capacity is 0.
     *
     * @sa assign(@a rate, @a capacity) */
    TokenRateX(token_type rate, token_type capacity) {
	assign(rate, capacity);
    }

    /** @brief Set the token rate to idle or unlimited.
     * @param unlimited idle if false, unlimited if true */
    inline void assign(bool unlimited = false);

    /** @brief Set the token rate and capacity.
     * @param rate refill rate in tokens per period
     * @param capacity maximum token accumulation
     *
     * Sets the token bucket's rate to @a rate and capacity to @a capacity.
     * If either @a rate or @a capacity is 0, the rate becomes idle. */
    inline void assign(token_type rate, token_type capacity);

    /** @brief Return true iff the token rate is unlimited. */
    bool unlimited() const {
	return _time_until_full == 0;
    }

    /** @brief Return true iff the token rate is idle. */
    bool idle() const {
	return _tokens_per_tick == 0;
    }

    /** @brief Return the rate in tokens per period.
     *
     * Returns max_tokens for unlimited rates.  Imprecise computer arithmetic
     * may cause the result to differ from the configured rate. */
    token_type rate() const;

    /** @brief Return the capacity in tokens.
     *
     * Returns max_tokens for unlimited rates.  Imprecise computer arithmetic
     * may cause the result to differ from the configured capacity. */
    token_type capacity() const {
	return max_tokens / _token_scale;
    }

    /** @brief Return the number of tokens per tick. */
    token_type tokens_per_tick() const {
	return _tokens_per_tick;
    }

    /** @brief Return the ratio of fractional tokens to real tokens. */
    token_type token_scale() const {
	return _token_scale;
    }

    /** @brief Return the number of ticks required to refill a counter to
     * capacity.
     *
     * Returns (ticks_type) -1 for idle rates. */
    ticks_type time_until_full() const {
	return _time_until_full;
    }

    /** @brief Return the current time point.
     *
     * Implemented as P::now(). */
    time_point_type now() const {
	return P::now();
    }

    /** @brief Return the time point corresponding to the @a time parameter.
     *
     * May not be available for all U types.  Implemented as P::time_point(@a
     * time). */
    template <typename U>
    time_point_type time_point(U time) const {
	return P::time_point(time);
    }

    /** @brief Return @a b - @a a, assuming that @a b was measured after @a a.
     *
     * Some time measurements can, in rare cases, appear to jump backwards,
     * as timestamps do when the user changes the current time.  If this
     * happens (@a b < @a a), time_monotonic_difference returns 0.
     * Implemented as P::time_monotonic_difference(@a a, @a b). */
    ticks_type time_monotonic_difference(time_point_type a, time_point_type b) const {
	return P::time_monotonic_difference(a, b);
    }


    /** @cond never */
    typedef time_point_type epoch_type CLICK_DEPRECATED;
    inline token_type tokens_per_epoch() const CLICK_DEPRECATED;
    inline ticks_type epochs_until_full() const CLICK_DEPRECATED;
    /** @endcond never */

 private:

    token_type _tokens_per_tick;	// 0 iff idle()
    token_type _token_scale;
    ticks_type _time_until_full;	// 0 iff unlimited()

};

template <typename P>
void TokenRateX<P>::assign(bool unlimited)
{
    _token_scale = 1;
    if (unlimited) {
	_tokens_per_tick = max_tokens;
	_time_until_full = 0;
    } else {
	_tokens_per_tick = 0;
	_time_until_full = (ticks_type) -1;
    }
}

template <typename P>
void TokenRateX<P>::assign(token_type rate, token_type capacity)
{
    if (capacity == 0) {
	rate = 0;
	capacity = max_tokens;
    }

    token_type frequency = P::frequency();
    if (rate != 0) {
	// constrain capacity so _tokens_per_tick fits in 1 limb
	unsigned min_capacity = (rate - 1) / frequency + 1;
	if (capacity < min_capacity)
	    capacity = min_capacity;
    }
    _token_scale = max_tokens / capacity;

    // XXX on non-32 bit types
    static_assert(sizeof(bigint::limb_type) == sizeof(token_type),
		  "bigint::limb_type should have the same size as token_type.");
    bigint::limb_type l[2] = { 0, 0 };
    bigint::limb_type a[2] = { rate, 0 };
    bigint::multiply_add(l, a, 2, _token_scale);
    (void) bigint::divide(l, l, 2, frequency);
    assert(l[1] == 0);

    if (rate != 0) {
	// constrain _tokens_per_tick to be at least 1
	_tokens_per_tick = (l[0] != 0 ? l[0] : 1);
	_time_until_full = (max_tokens - 1) / _tokens_per_tick + 1;
    } else {
	_tokens_per_tick = 0;
	_time_until_full = (ticks_type) -1;
    }
}

template <typename P>
typename P::token_type TokenRateX<P>::rate() const
{
    static_assert(sizeof(bigint::limb_type) == sizeof(token_type),
		  "bigint::limb_type should have the same size as token_type.");
    bigint::limb_type l[2] = { _tokens_per_tick / 2, 0 };
    bigint::limb_type a[2] = { _tokens_per_tick, 0 };
    bigint::multiply_add(l, a, 2, P::frequency());
    (void) bigint::divide(l, l, 2, token_scale());
    return l[1] ? (token_type) max_tokens : l[0];
}

/** @cond never */
template <typename P>
inline typename TokenRateX<P>::token_type TokenRateX<P>::tokens_per_epoch() const
{
    return tokens_per_tick();
}

template <typename P>
inline typename TokenRateX<P>::ticks_type TokenRateX<P>::epochs_until_full() const
{
    return time_until_full();
}
/** @endcond never */


/** @cond never */
/* TokenRateConverter safely scales token counts according to an input rate.
   Template specializations let us make use of a fast int_multiply() when one
   is available. */
template<typename rate_type, bool FM> struct TokenRateConverter {
};
template<typename rate_type> struct TokenRateConverter<rate_type, true> {
    static bool cvt(const rate_type &rate, typename rate_type::token_type &t) {
	typename rate_type::token_type high;
	int_multiply(t, rate.token_scale(), t, high);
	if (high)
	    t = rate_type::max_tokens;
	return !high;
    }
};
template<typename rate_type> struct TokenRateConverter<rate_type, false> {
    static bool cvt(const rate_type &rate, typename rate_type::token_type &t) {
	if (t <= rate.capacity()) {
	    t *= rate.token_scale();
	    return true;
	} else {
	    t = rate_type::max_tokens;
	    return false;
	}
    }
};
/** @endcond never */


/** @class TokenCounterX include/click/tokenbucket.hh <click/tokenbucket.hh>
 @brief  Token bucket counter template.

 The TokenCounterX class implements a token counter associated with a token
 bucket rate.  The rate type, normally a TokenRateX, is specified as a
 template parameter.  Most of its member functions take an explicit rate
 argument.  The contains() method reports whether the counter has at least a
 given number of tokens.  The counter is emptied by the remove() and
 remove_if() methods and refilled by the refill() methods.

 Often the token rate associated with a counter will not change during the
 counter's lifetime.  TokenCounterX will work correctly if the rate changes,
 however.  (See the adjust() method for details.)

 TokenCounterX internally maintains fractional tokens, so it should be
 relatively precise.

 Idle and unlimited rates affect how TokenCounters are refilled.  For idle
 rates, refill() is a no-op.  For unlimited rates, any refill() makes the
 counter full(), containing max_tokens tokens.  The set(), empty(), full(),
 remove(), and similar functions act as normal for idle and unlimited rates.

 Most users will be satisfied with the TokenCounter type, which is equal to
 TokenCounterX<TokenRateX<TokenBucketJiffyParameters<unsigned> > >.

 @sa TokenRateX, TokenBucketX */

template <typename R>
class TokenCounterX { public:

    /** @brief The token rate type. */
    typedef R rate_type;

    /** @brief Unsigned type of token counts. */
    typedef typename R::token_type token_type;

    /** @brief Type of time points. */
    typedef typename R::time_point_type time_point_type;

    /** @brief Unsigned type of tick counts (differences between time points). */
    typedef typename R::ticks_type ticks_type;

    enum {
	max_tokens = R::max_tokens
    };

    /** @brief Construct an empty TokenCounter.
     *
     * The initial time point is 0. */
    TokenCounterX()
	: _tokens(0), _time_point() {
    }

    /** @brief Construct a TokenCounter.
     * @param full whether the counter is created full
     *
     * The counter is initially full() if @a full is true, otherwise it is
     * empty.  The initial time point is 0. */
    explicit TokenCounterX(bool full)
	: _tokens(full ? (token_type) max_tokens : 0), _time_point() {
    }

    /** @brief Return the number of tokens in the counter.
     * @param rate associated token rate
     *
     * The return value is a lower bound on the number of tokens, since
     * TokenCounterX keeps track of fractional tokens. */
    token_type size(const rate_type &rate) const {
	return _tokens / rate.token_scale();
    }

    /** @brief Return the counter's fullness fraction.
     *
     * The return value is a number between 0 and max_tokens, where max_tokens
     * represents full capacity. */
    token_type fraction() const {
	return _tokens;
    }

    /** @brief Test if the token counter is completely empty. */
    bool empty() const {
	return _tokens == 0;
    }

    /** @brief Test if the token counter is at full capacity. */
    bool full() const {
	return _tokens == (token_type) max_tokens;
    }

    /** @brief Test if the token counter has at least @a t tokens.
     * @param rate associated token rate
     * @param t token count
     *
     * Returns false whenever @a t is greater than <em>rate</em>.@link
     * TokenRateX::capacity capacity()@endlink. */
    bool contains(const rate_type &rate, token_type t) const {
	return cvt_type::cvt(rate, t) && contains_fraction(t);
    }

    /** @brief Test if the token counter is above a fraction of its capacity.
     * @param f fullness fraction, where max_tokens is full capacity */
    bool contains_fraction(token_type f) const {
	return f <= _tokens;
    }

    /** @brief Clear the token counter.
     *
     * @sa set(), set_full() */
    void clear() {
	_tokens = 0;
    }

    /** @brief Fill the token counter to capacity.
     *
     * @sa clear(), set() */
    void set_full() {
	_tokens = max_tokens;
    }

    /** @brief Set the token counter to contain @a t tokens.
     * @param rate associated token rate
     * @param t number of tokens
     *
     * The result will never have more tokens than the associated capacity. */
    void set(const rate_type &rate, token_type t) {
	(void) cvt_type::cvt(rate, t);
	_tokens = t;
    }

    /** @brief Set the token counter to a fraction of its capacity.
     * @param f fullness fraction, where max_tokens is full capacity */
    void set_fraction(token_type f) {
	_tokens = f;
    }

    /** @brief Compensate the counter for a change of rate.
     * @param old_rate old associated token rate
     * @param new_rate new associated token rate
     *
     * TokenCounterX's internal representation stores the token count as a
     * fraction of the rate's capacity.  This means that if you change the
     * associated rate to have a different capacity, the token count will
     * appear to change.  To keep the token count roughly the same, call
     * adjust() with the old and new rates; TokenCounterX will as far as
     * possible compensate for the rate change. */
    void adjust(const rate_type &old_rate, const rate_type &new_rate) {
	if (old_rate.token_scale() != new_rate.token_scale()) {
	    static_assert(sizeof(bigint::limb_type) == sizeof(token_type),
			  "bigint::limb_type should have the same size as token_type.");
	    bigint::limb_type l[2] = { 0, 0 };
	    bigint::limb_type a[2] = { _tokens, 0 };
	    bigint::multiply_add(l, a, 2, new_rate.token_scale());
	    (void) bigint::divide(l, l, 2, old_rate.token_scale());
	    _tokens = l[1] ? (token_type) max_tokens : l[0];
	}
    }

    /** @brief Refill the token counter to time @a rate.now().
     * @param rate associated token rate
     *
     * There are three refill() methods, useful for different methods of
     * measuring time.  This method calls @a rate.now(), which returns the
     * current time.  Other methods use an explicit time point and a @a
     * rate.time_point(U) method.
     *
     * @sa set_time_point */
    void refill(const rate_type &rate);

    /** @brief Refill the token counter for @a time.
     * @param rate associated token rate
     * @param time new time point */
    void refill(const rate_type &rate, time_point_type time);

    /** @brief Refill the token counter for @a time.
     * @param rate associated token rate
     * @param time new time */
    template <typename U> void refill(const rate_type &rate, U time);

    /** @brief Set the token counter's internal time point to @a time.
     * @param time new time point
     *
     * Unlike refill(), this method does not refill the counter.
     *
     * @sa refill */
    void set_time_point(time_point_type time) {
	_time_point = time;
    }

    /** @brief Remove @a t tokens from the counter.
     * @param rate associated token rate
     * @param t number of tokens
     *
     * If the token counter contains less than @a t tokens, the new token
     * count is 0. */
    void remove(const rate_type &rate, token_type t) {
	(void) cvt_type::cvt(rate, t);
	remove_fraction(t);
    }

    /** @brief Remove @a t tokens from the counter if it contains @a t tokens.
     * @param rate associated token rate
     * @param t number of tokens
     * @return true if @a t tokens were removed, false otherwise
     *
     * If the counter contains @a t or more tokens, calls remove(@a t) and
     * returns true.  If it contains less than @a t tokens, returns false
     * without removing any tokens. */
    bool remove_if(const rate_type &rate, token_type t) {
	return cvt_type::cvt(rate, t) && remove_fraction_if(t);
    }

    /** @brief Remove a fullness fraction from the counter.
     * @param f fullness fraction, where max_tokens is full capacity
     *
     * If the token counter is less than @a f full, the new token count is 0. */
    void remove_fraction(token_type f) {
	_tokens = (f <= _tokens ? _tokens - f : 0);
    }

    /** @brief Remove a fullness fraction from the counter if it is full enough.
     * @param f fullness fraction, where max_tokens is full capacity
     * @return true if @a f was removed, false otherwise
     *
     * If fraction() is at least @a f, calls remove_fraction(@a f) and returns
     * true.  Otherwise, returns false without removing any tokens. */
    bool remove_fraction_if(token_type f) {
	if (f <= _tokens) {
	    _tokens -= f;
	    return true;
	} else
	    return false;
    }

    /** @brief Return the number of ticks until contains(<em>rate</em>, <em>t</em>).
     *
     * @param rate associated token rate
     * @param t token count
     *
     * Returns (ticks_type) -1 if passing time will never make
     * @link contains() contains(<em>rate</em>, <em>t</em>)@endlink
     * true. */
    ticks_type time_until_contains(const rate_type &rate,
				   token_type t) const {
	if (cvt_type::cvt(rate, t))
	    return time_until_contains_fraction(rate, t);
	else
	    return (ticks_type) -1;
    }

    /** @brief Return the number of ticks until contains_fraction(<em>f</em>).
     * @param rate associated token rate
     * @param f fullness fraction, where max_tokens is full capacity
     *
     * Returns (ticks_type) -1 if passing time will never make
     * @link contains_fraction() contains_fraction(<em>f</em>)@endlink
     * true. */
    ticks_type time_until_contains_fraction(const rate_type &rate,
					    token_type f) const {
	if (f <= _tokens || rate.time_until_full() == 0)
	    return 0;
	else if (rate.tokens_per_tick() == 0)
	    return (ticks_type) -1;
	else
	    return (f - _tokens - 1) / rate.tokens_per_tick() + 1;
    }


    /** @cond never */
    inline ticks_type epochs_until_contains(const rate_type &rate, token_type t) const CLICK_DEPRECATED;
    inline ticks_type epochs_until_contains_fraction(const rate_type &rate, token_type f) const CLICK_DEPRECATED;
    /** @endcond never */

  private:

    token_type _tokens;
    time_point_type _time_point;

    typedef TokenRateConverter<rate_type, has_fast_int_multiply<token_type>::value> cvt_type;

};

template <typename R>
void TokenCounterX<R>::refill(const rate_type &rate, time_point_type time)
{
    ticks_type diff = rate.time_monotonic_difference(_time_point, time);
    if (diff >= rate.time_until_full()) {
	// ignore special case of idle rates -- we assume that
	// rate.time_monotonic_difference() will never return (ticks_type) -1,
	// and ensure that ticks_type is unsigned
	_tokens = max_tokens;
    } else if (diff > 0) {
	token_type new_tokens = _tokens + diff * rate.tokens_per_tick();
	_tokens = (new_tokens < _tokens ? (token_type) max_tokens : new_tokens);
    }
    _time_point = time;
}

template <typename R>
void TokenCounterX<R>::refill(const rate_type &rate)
{
    refill(rate, rate.now());
}

template <typename R> template <typename U>
void TokenCounterX<R>::refill(const rate_type &rate, U time)
{
    refill(rate, rate.time_point(time));
}

/** @cond never */
template <typename R>
inline typename TokenCounterX<R>::ticks_type TokenCounterX<R>::epochs_until_contains(const rate_type &rate, token_type t) const
{
    return time_until_contains(rate, t);
}
template <typename R>
inline typename TokenCounterX<R>::ticks_type TokenCounterX<R>::epochs_until_contains_fraction(const rate_type &rate, token_type f) const
{
    return time_until_contains_fraction(rate, f);
}
/** @endcond never */


/** @class TokenBucketJiffyParameters include/click/tokenbucket.hh <click/tokenbucket.hh>
 @brief  Helper class for token bucket rate limiter.

 Pass this class as the parameter to TokenRateX.  TokenBucketJiffyParameters
 measures ticks in units of jiffies.  The template parameter is the type of
 tokens. */

template <typename T>
class TokenBucketJiffyParameters { public:

    /** @brief The type of tokens.  Always unsigned. */
    typedef T token_type;

    /** @brief The type of a time point.  Always unsigned. */
    typedef click_jiffies_t time_point_type;

    /** @brief The type of a difference between time points.  Always signed. */
    typedef click_jiffies_difference_t duration_type;

    /** @brief Return the current time point.
     * @note TokenBucketJiffyParameters measures time points in jiffies. */
    static time_point_type now() {
	return click_jiffies();
    }

    static time_point_type time_point(time_point_type t) {
	return t;
    }

    /** @brief Return @a b - @a a, assuming that @a b was measured after @a a.
     *
     * Some time measurements can, in rare cases, appear to jump backwards,
     * as timestamps do when the user changes the current time.  If this
     * happens, and @a b < @a a (even though @a b happened after @a a),
     * time_monotonic_difference must return 0. */
    static duration_type time_monotonic_difference(time_point_type a, time_point_type b) {
#if CLICK_JIFFIES_MONOTONIC
	return b - a;
#else
	return (likely(a <= b) ? b - a : 0);
#endif
    }

    /** @brief Return true if @a a happened before @a b. */
    static bool time_less(time_point_type a, time_point_type b) {
	return click_jiffies_less(a, b);
    }

    /** @brief Return the number of time points per period.
     *
     * Here, this is the number of jiffies per second. */
    static unsigned frequency() {
	return CLICK_HZ;
    }

    /** @brief Return the Timestamp representing a given time point. */
    static Timestamp timestamp(time_point_type x) {
	return Timestamp::make_jiffies(x);
    }

    /** @brief Return the Timestamp representing a given tick count. */
    static Timestamp timestamp(duration_type x) {
	return Timestamp::make_jiffies(x);
    }

};


/** @class TokenBucketX include/click/tokenbucket.hh <click/tokenbucket.hh>
 @brief  Token bucket rate limiter.

 The TokenBucketX class implements a token bucket rate limiter.  It is
 implemented as a pair of TokenRateX and TokenCounterX.

 Most users will be satisfied with the TokenBucket type, which is equal to
 TokenBucketX<TokenBucketJiffyParameters<unsigned> >.

 @sa GapRate */

template <typename P>
class TokenBucketX { public:

    /** @brief The template parameter type. */
    typedef P parameter_type;

    /** @brief The token rate type. */
    typedef TokenRateX<P> rate_type;

    /** @brief The token counter type. */
    typedef TokenCounterX<rate_type> counter_type;

    /** @brief Unsigned type of token counts. */
    typedef typename rate_type::token_type token_type;

    /** @brief Type of time ticks. */
    typedef typename rate_type::time_point_type time_point_type;

    /** @brief Unsigned type of differences between time ticks. */
    typedef typename rate_type::ticks_type ticks_type;

    enum {
	max_tokens = rate_type::max_tokens
    };

    /** @brief Construct an idle token bucket.
     *
     * The initial time point is 0. */
    TokenBucketX() {
    }

    /** @brief Construct an idle or unlimited token bucket.
     * @param unlimited idle if false, unlimited if true
     *
     * The initial time point is 0. */
    explicit TokenBucketX(bool unlimited)
	: _rate(unlimited), _bucket(unlimited) {
    }

    /** @brief Construct a token bucket representing @a rate.
     * @param rate refill rate in tokens per period
     * @param capacity maximum token accumulation
     *
     * The initial time point is 0 and the token bucket is initially full (the
     * initial token count equals @a capacity).  The rate is idle if either @a
     * rate or @a capacity is 0.
     *
     * @sa assign(@a rate, @a capacity) */
    TokenBucketX(token_type rate, token_type capacity)
	: _rate(rate, capacity), _bucket(rate != 0 && capacity != 0) {
    }

    /** @brief Set the token bucket rate to idle or unlimited.
     * @param unlimited idle if false, unlimited if true */
    void assign(bool unlimited = false) {
	_rate.assign(unlimited);
    }

    /** @brief Set the token bucket rate and capacity.
     * @param rate refill rate in tokens per period
     * @param capacity maximum token accumulation
     *
     * Sets the token bucket's rate to @a rate and capacity to @a capacity.
     * If either @a rate or @a capacity is 0, the token bucket becomes idle.
     * The time point is unchanged.
     *
     * The ratio of tokens/burst is unchanged by the assignment, so the actual
     * number of tokens could go up or down, depending on how the rate is
     * changed.
     *
     * @sa assign_adjust */
    void assign(token_type rate, token_type capacity) {
	_rate.assign(rate, capacity);
    }

    /** @brief Set the token bucket rate and capacity, preserving the
     *  token count.
     * @param rate refill rate in tokens per period
     * @param capacity maximum token accumulation
     *
     * This performs the same function as assign(), but additionally
     * keeps the number of tokens roughly stable.
     *
     * @sa assign */
    void assign_adjust(token_type rate, token_type capacity) {
	rate_type old_rate(_rate);
	_rate.assign(rate, capacity);
	_bucket.adjust(old_rate, _rate);
    }

    /** @brief Return true iff the token rate is unlimited. */
    bool unlimited() const {
	return _rate.unlimited();
    }

    /** @brief Return true iff the token rate is idle. */
    bool idle() const {
	return _rate.idle();
    }

    /** @brief Return the rate in tokens per period.
     *
     * Returns max_tokens for unlimited rates.  Imprecise computer arithmetic
     * may cause the result to differ from the configured rate. */
    token_type rate() const {
	return _rate.rate();
    }

    /** @brief Return the capacity in tokens.
     *
     * Returns max_tokens for unlimited rates.  Imprecise computer arithmetic
     * may cause the result to differ from the configured capacity. */
    token_type capacity() const {
	return _rate.capacity();
    }

    /** @brief Return the number of tokens in the bucket.
     *
     * The return value is a lower bound on the number of tokens, since
     * TokenBucketX keeps track of fractional tokens. */
    token_type size() const {
	return _bucket.size(_rate);
    }

    /** @brief Return the bucket's fullness fraction.
     *
     * The return value is a number between 0 and max_tokens, where max_tokens
     * represents full capacity. */
    token_type fraction() const {
	return _bucket.fraction();
    }

    /** @brief Test if the token bucket is completely empty. */
    bool empty() const {
	return _bucket.empty();
    }

    /** @brief Test if the token bucket is at full capacity. */
    bool full() const {
	return _bucket.full();
    }

    /** @brief Test if the token bucket has at least @a t tokens.
     *
     * Returns true whenever @a t is zero or @a rate is unlimited.  Returns
     * false whenever @a t is greater than @a rate.capacity(). */
    bool contains(token_type t) const {
	return _bucket.contains(_rate, t);
    }

    /** @brief Test if the token bucket is above a fraction of its capacity.
     * @param f fullness fraction, where max_tokens is full capacity */
    bool contains_fraction(token_type f) const {
	return _bucket.contains_fraction(f);
    }

    /** @brief Clear the token bucket.
     *
     * @sa set(), set_full() */
    void clear() {
	_bucket.clear();
    }

    /** @brief Fill the token bucket to capacity.
     *
     * @sa clear(), set() */
    void set_full() {
	_bucket.set_full();
    }

    /** @brief Set the token bucket to contain @a t tokens.
     * @param t number of tokens
     *
     * The result will never have more tokens than the associated capacity. */
    void set(token_type t) {
	_bucket.set(_rate, t);
    }

    /** @brief Set the token bucket to a fraction of its capacity.
     * @param f fullness fraction, where max_tokens is full capacity */
    void set_fraction(token_type f) {
	_bucket.set_fraction(f);
    }

    /** @brief Refill the token bucket to time P::now().
     *
     * There are three refill() methods, useful for different methods of
     * measuring ticks.  This method call parameter_type::now(), which returns
     * the current time.  Other methods use an explicit time point and a
     * parameter_type::time(U) method.
     *
     * @sa set_time_point */
    void refill() {
	_bucket.refill(_rate);
    }

    /** @brief Refill the token bucket for @a time. */
    void refill(time_point_type time) {
	_bucket.refill(_rate, time);
    }

    /** @brief Refill the token bucket for time P::time_point(@a time). */
    template <typename U> void refill(U time) {
	_bucket.refill(_rate, time);
    }

    /** @brief Set the token bucket's internal time point to @a time.
     *
     * Unlike refill(), this method does not refill the counter.
     *
     * @sa refill */
    void set_time_point(time_point_type time) {
	_bucket.set_time_point(time);
    }

    /** @brief Remove @a t tokens from the bucket.
     * @param t number of tokens
     *
     * If the token bucket contains less than @a t tokens, the new token
     * count is 0. */
    void remove(token_type t) {
	_bucket.remove(_rate, t);
    }

    /** @brief Remove @a t tokens from the bucket if it contains @a t tokens.
     * @param t number of tokens
     * @return true if @a t tokens were removed, false otherwise
     *
     * If the token bucket contains @a t or more tokens, calls remove(@a t)
     * and returns true.  If it contains less than @a t tokens, returns false
     * without removing any tokens. */
    bool remove_if(token_type t) {
	return _bucket.remove_if(_rate, t);
    }

    /** @brief Remove a fullness fraction from the bucket.
     * @param f fullness fraction, where max_tokens is full capacity
     *
     * If the token counter is less than @a f full, the new token count is 0. */
    void remove_fraction(token_type f) {
	_bucket.remove_fraction(f);
    }

    /** @brief Remove a fullness fraction from the bucket if it is full enough.
     * @param f fullness fraction, where max_tokens is full capacity
     * @return true if @a f was removed, false otherwise
     *
     * If fraction() is at least @a f, calls remove_fraction(@a f) and returns
     * true.  Otherwise, returns false without removing any tokens. */
    bool remove_fraction_if(token_type f) {
	return _bucket.remove_fraction_if(f);
    }

    /** @brief Return the number of ticks until contains(@a t).
     *
     * Returns (ticks_type) -1 if passing time will never make contains(@a t)
     * true. */
    ticks_type time_until_contains(token_type t) const {
	return _bucket.time_until_contains(_rate, t);
    }

    /** @brief Return the number of ticks until contains_fraction(@a f).
     *
     * Returns (ticks_type) -1 if passing time will never make
     * contains_fraction(@a f) true. */
    ticks_type time_until_contains_fraction(ticks_type f) const {
	return _bucket.time_until_contains_fraction(_rate, f);
    }


    /** @cond never */
    inline ticks_type epochs_until_contains(const rate_type &rate, token_type t) const CLICK_DEPRECATED;
    inline ticks_type epochs_until_contains_fraction(const rate_type &rate, token_type f) const CLICK_DEPRECATED;
    /** @endcond never */

  private:

    rate_type _rate;
    counter_type _bucket;

};

/** @cond never */
template <typename P>
inline typename TokenBucketX<P>::ticks_type TokenBucketX<P>::epochs_until_contains(const rate_type &rate, token_type t) const
{
    return time_until_contains(rate, t);
}
template <typename P>
inline typename TokenBucketX<P>::ticks_type TokenBucketX<P>::epochs_until_contains_fraction(const rate_type &rate, token_type f) const
{
    return time_until_contains_fraction(rate, f);
}
/** @endcond never */


/** @class TokenRate include/click/tokenbucket.hh <click/tokenbucket.hh>
 * @brief Jiffy-based token bucket rate
 *
 * Equivalent to
 * @link TokenRateX TokenRateX<TokenBucketJiffyParameters<unsigned> >@endlink.
 * @sa TokenRateX, TokenBucketJiffyParameters */
typedef TokenRateX<TokenBucketJiffyParameters<unsigned> > TokenRate;

/** @class TokenCounter include/click/tokenbucket.hh <click/tokenbucket.hh>
 * @brief Jiffy-based token counter
 *
 * Equivalent to
 * @link TokenCounterX TokenCounterX<TokenRate>@endlink.
 * @sa TokenCounterX, TokenRate */
typedef TokenCounterX<TokenRate> TokenCounter;

/** @class TokenBucket include/click/tokenbucket.hh <click/tokenbucket.hh>
 * @brief Jiffy-based token bucket rate limiter
 *
 * Equivalent to
 * @link TokenBucketX TokenBucketX<TokenBucketJiffyParameters<unsigned> >@endlink.
 * @sa TokenBucketX, TokenBucketJiffyParameters */
typedef TokenBucketX<TokenBucketJiffyParameters<unsigned> > TokenBucket;

CLICK_ENDDECLS
#endif
