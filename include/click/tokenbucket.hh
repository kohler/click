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

 The token bucket templates divide time into discrete units called I<epochs>.
 Token counters are refilled up to once per epoch.  An epoch may be less than
 a full period.  For example, if periods and epochs are 1 second a 1
 millisecond, respectively, then a TokenCounterX with associated rate 1000
 tokens per second would be refilled at 1 token per millisecond.  The
 TokenRateX template parameter P defines the epoch unit.  The provided
 TokenBucketJiffyParameters class is designed to be used as TokenRateX's
 parameter; it measures epochs in units of jiffies.

 @sa GapRate */

/** @class TokenRateX include/click/tokenbucket.hh <click/tokenbucket.hh>
 @brief  Token bucket rate template.

 The TokenRateX class implements a token bucket rate.  TokenBucketX is
 initialized with a rate, in tokens per second, and a capacity in tokens.
 Associated token buckets fill up with tokens at the given rate, to a maximum
 of the capacity.

 Two special types of rate are supported.  An <em>unlimited</em> TokenRateX
 contains an effectively infinite number of tokens, and is indicated by a rate
 of 0.  An <em>idle</em> TokenRateX never fills, and is indicated by a
 capacity of 0.

 Most users will be satisfied with the TokenRate type, which is equal to
 TokenRateX<TokenBucketJiffyParameters<unsigned> >.

 @sa TokenCounterX, TokenBucketX */

template <typename P> class TokenRateX;

template <typename P>
class TokenRateX { public:

    /** @brief The template parameter type. */
    typedef P parameter_type;

    /** @brief Unsigned type of token counts. */
    typedef typename P::token_type token_type;

    /** @brief Type of time epochs. */
    typedef typename P::epoch_type epoch_type;

    enum {
	max_tokens = (token_type) -1
    };

    /** @brief Construct an unlimited token rate. */
    inline TokenRateX();

    /** @brief Construct a token rate representing @a rate.
     * @param rate refill rate in tokens per period
     * @param capacity maximum token accumulation
     *
     * The initial epoch is 0.
     *
     * @sa assign(@a rate, @a capacity) */
    inline TokenRateX(token_type rate, token_type capacity);

    /** @brief Set the token rate and capacity.
     * @param rate refill rate in tokens per period
     * @param capacity maximum token accumulation
     *
     * Sets the token bucket's rate to @a rate and capacity to @a capacity.
     *
     * If @a rate is zero, the token rate becomes unlimited; associated
     * counters will always be full.  If @a rate is nonzero but @a capacity is
     * negative or zero, the token rate becomes idle; associated counters will
     * never be full. */
    inline void assign(token_type rate, token_type capacity);

    /** @brief Return true iff the token rate is normal (not unlimited or
	idle). */
    bool normal() const {
	return _token_scale != 0 && _tokens_per_epoch != 0;
    }

    /** @brief Return true iff the token rate is unlimited. */
    bool unlimited() const {
	return _token_scale == 0;
    }

    /** @brief Return true iff the token rate is idle. */
    bool idle() const {
	return _tokens_per_epoch == 0;
    }

    /** @brief Return the rate in tokens per period.
     *
     * Returns 0 for idle and unlimited rates.  Imprecise computer
     * arithmetic may cause the result to differ from the configured
     * rate. */
    token_type rate() const;

    /** @brief Return the capacity in tokens.
     *
     * Returns 0 for idle rates and max_tokens for unlimited rates.
     * Imprecise computer arithmetic may cause the result to differ from the
     * configured capacity. */
    token_type capacity() const;

    /** @brief Return the number of tokens per epoch. */
    token_type tokens_per_epoch() const {
	return _tokens_per_epoch;
    }

    /** @brief Return the ratio of fractional tokens to real tokens. */
    token_type token_scale() const {
	return _token_scale;
    }

    /** @brief Return the number of epochs required to fill a counter to
	capacity. */
    epoch_type epochs_to_fill() const {
	return _epochs_to_fill;
    }

    /** @brief Return the current epoch.
     *
     * Implemented as P::epoch(). */
    epoch_type epoch() const {
	return P::epoch();
    }

    /** @brief Return the epoch corresponding to the @a time parameter.
     *
     * May not be available for all U types.  Implemented as P::epoch(@a
     * time). */
    template <typename U>
    epoch_type epoch(U time) const {
	return P::epoch(time);
    }

    /** @brief Return @a b - @a a, assuming that @a b was measured after @a a.
     *
     * Some epoch measurements can, in rare cases, appear to jump backwards,
     * as timestamps do when the user changes the current time.  If this
     * happens (@a b < @a a), epoch_monotonic_difference returns 0.
     * Implemented as P::epoch_monotonic_difference(@a a, @a b). */
    epoch_type epoch_monotonic_difference(epoch_type a, epoch_type b) const {
	return P::epoch_monotonic_difference(a, b);
    }

 private:

    token_type _tokens_per_epoch;	// 0 iff idle()
    token_type _token_scale;		// 0 iff unlimited()
    epoch_type _epochs_to_fill;

};


template <typename P>
TokenRateX<P>::TokenRateX()
    : _tokens_per_epoch(0), _token_scale(0), _epochs_to_fill(1)
{
}

template <typename P>
TokenRateX<P>::TokenRateX(token_type rate, token_type capacity)
{
    assign(rate, capacity);
}

template <typename P>
void TokenRateX<P>::assign(token_type rate, token_type capacity)
{
    if (rate == 0) {
	_token_scale = 0;
	_tokens_per_epoch = 1;
	_epochs_to_fill = 1;
    } else if (capacity <= 0) {
	_token_scale = 2;
	_tokens_per_epoch = 0;
	_epochs_to_fill = 0;
    } else {
	token_type frequency = P::epoch_frequency();

	// constrain capacity so _tokens_per_epoch fits in 1 limb
	unsigned min_capacity = (rate - 1) / frequency + 1;
	if (capacity < min_capacity)
	    capacity = min_capacity;

	_token_scale = max_tokens / capacity;

	// XXX on non-32 bit types
	static_assert(sizeof(bigint::limb_type) == sizeof(token_type));
	bigint::limb_type l[2] = { 0, 0 };
	bigint::limb_type a[2] = { rate, 0 };
	bigint::multiply_add(l, a, 2, _token_scale);
	(void) bigint::divide(l, l, 2, frequency);
	// constrain _tokens_per_epoch to be at least 1
	_tokens_per_epoch = l[0] ? l[0] : 1;
	assert(l[1] == 0);

	_epochs_to_fill = (max_tokens - 1) / _tokens_per_epoch + 1;
    }
}

template <typename P>
typename P::token_type TokenRateX<P>::rate() const
{
    if (!normal())
	return 0;
    static_assert(sizeof(bigint::limb_type) == sizeof(token_type));
    bigint::limb_type l[2] = { 0, 0 };
    bigint::limb_type a[2] = { _tokens_per_epoch, 0 };
    bigint::multiply_add(l, a, 2, P::epoch_frequency());
    (void) bigint::divide(l, l, 2, _token_scale);
    return l[0];
}

template <typename P>
typename P::token_type TokenRateX<P>::capacity() const
{
    if (normal())
	return max_tokens / _token_scale;
    else
	return idle() ? 0 : (token_type) max_tokens;
}

/** @class TokenCounterX include/click/tokenbucket.hh <click/tokenbucket.hh>
 @brief  Token bucket counter template.

 The TokenCounterX class implements a token counter associated with a token
 bucket rate.  The rate type, normally a TokenRateX, is specified as a
 template parameter.  Most of its member functions take an explicit rate
 argument.  The contains() method reports whether the counter has at least a
 given number of tokens.  The counter is emptied by the remove() and
 remove_if() methods, and filled by the fill() methods.

 Often the token rate associated with a counter will not change during the
 counter's lifetime.  TokenCounterX will work correctly if the rate changes,
 however.  (See the adjust() method for details.)

 TokenCounterX internally maintains fractional tokens, so it should be
 relatively precise.

 Most users will be satisfied with the TokenCounter type, which is equal to
 TokenCounterX<TokenRateX<TokenBucketJiffyParameters<unsigned> > >.

 @sa TokenRateX, TokenBucketX */

template <typename R>
class TokenCounterX { public:

    /** @brief The token rate type. */
    typedef R rate_type;

    /** @brief Unsigned type of token counts. */
    typedef typename R::token_type token_type;

    /** @brief Type of time epochs. */
    typedef typename R::epoch_type epoch_type;

    enum {
	max_tokens = R::max_tokens
    };

    /** @brief Construct an empty TokenCounter.
     *
     * The initial epoch is 0. */
    TokenCounterX()
	: _tokens(0), _epoch() {
    }

    /** @brief Construct a TokenCounter.
     * @param full whether the counter is created full
     *
     * The counter is initially full() if @a full is true, otherwise it is
     * empty.  The initial epoch is 0. */
    explicit TokenCounterX(bool full)
	: _tokens(full ? (token_type) max_tokens : 0), _epoch() {
    }

    /** @brief Return the number of tokens in the counter.
     * @param rate associated token rate
     *
     * The return value is a lower bound on the number of tokens, since
     * TokenCounterX keeps track of fractional tokens.  Returns zero for idle
     * rates and max_tokens for unlimited rates. */
    token_type size(const rate_type &rate) const {
	if (likely(rate.normal()))
	    return _tokens / rate.token_scale();
	else
	    return rate.unlimited() ? (token_type) max_tokens : 0;
    }

    /** @brief Return true iff the token counter is completely empty.
     * @param rate associated token rate
     *
     * Always returns true for idle rates and false for unlimited rates. */
    inline bool empty(const rate_type &rate) const {
	if (likely(rate.normal()))
	    return _tokens == 0;
	else
	    return rate.idle();
    }

    /** @brief Return true iff the token counter is at capacity.
     * @param rate associated token rate
     *
     * Always returns false for idle rates and true for unlimited rates. */
    inline bool full(const rate_type &rate) const {
	if (likely(rate.normal()))
	    return _tokens == (token_type) max_tokens;
	else
	    return rate.unlimited();
    }

    /** @brief Return true iff the token counter has at least @a t tokens.
     * @param rate associated token rate
     *
     * Returns true whenever @a t is zero or @a rate is unlimited. */
    inline bool contains(const rate_type &rate, token_type t) const {
	return t * rate.token_scale() <= (rate.normal() ? _tokens : 1);
    }

    /** @brief Clear the token counter.
     *
     * The resulting counter will appear empty() for all normal token rates.
     * @sa set(), set_full() */
    void clear() {
	_tokens = 0;
    }

    /** @brief Fill the token counter to capacity.
     *
     * The resulting counter will appear full() for all normal token rates.
     * @sa clear(), set() */
    void set_full() {
	_tokens = max_tokens;
    }

    /** @brief Set the token counter to contain @a t tokens.
     * @param rate associated token rate
     * @param t number of tokens
     *
     * The result will never have more tokens than the associated capacity.
     * For idle @a rate, the counter remains empty. */
    void set(const rate_type &rate, token_type t) {
	if (unlikely(rate.unlimited() || t > max_tokens / rate.token_scale()))
	    _tokens = max_tokens;
	else
	    _tokens = t * rate.token_scale();
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
	if (old_rate.token_scale() != new_rate.token_scale()
	    && _tokens != 0
	    && !old_rate.unlimited()
	    && !new_rate.unlimited()
	    && (_tokens != (token_type) max_tokens
		|| new_rate.token_scale() < old_rate.token_scale())) {
	    static_assert(sizeof(bigint::limb_type) == sizeof(token_type));
	    bigint::limb_type l[2] = { 0, 0 };
	    bigint::limb_type a[2] = { _tokens, 0 };
	    bigint::multiply_add(l, a, 2, new_rate.token_scale());
	    (void) bigint::divide(l, l, 2, old_rate.token_scale());
	    _tokens = l[1] ? (token_type) max_tokens : l[0];
	}
    }

    /** @brief Fill the token counter to time @a rate.epoch().
     * @param rate associated token rate
     *
     * There are three fill() methods, useful for different methods of
     * measuring epochs.  This method calls @a rate.epoch(), which returns the
     * current epoch.  Other methods use an explicit epoch and a @a
     * rate.epoch(U) method. */
    void fill(const rate_type &rate);

    /** @brief Fill the token counter for time @a epoch.
     * @param rate associated token rate */
    void fill(const rate_type &rate, epoch_type epoch);

    /** @brief Fill the token counter for @a time. */
    template <typename U> void fill(const rate_type &rate, U time);

    /** @brief Remove @a t tokens from the counter.
     * @param rate associated token rate
     * @param t number of tokens
     * @pre @a t <= capacity
     *
     * If the token bucket contains less than @a t tokens, the new token count
     * is 0. */
    inline void remove(const rate_type &rate, token_type t);

    /** @brief Remove @a t tokens from the counter if it contains @a t tokens.
     * @param rate associated token rate
     * @param t number of tokens
     * @pre @a t <= capacity
     * @return true if @a t tokens were removed, false otherwise
     *
     * If the counter contains @a t or more tokens, calls remove(@a t)
     * and returns true.  If it contains less than @a t tokens, returns false
     * without removing any tokens. */
    inline bool remove_if(const rate_type &rate, token_type t);

    /** @brief Return the number of epochs until contains(@a t) is true.
     * @param rate associated token rate
     *
     * Returns (epoch_type) -1 if @a rate is idle. */
    inline epoch_type epochs_until_contains(const rate_type &rate,
					    token_type t) const {
	if (unlikely(!rate.normal()))
	    return rate.idle() ? (epoch_type) -1 : 0;
	t *= rate.token_scale();
	if (_tokens >= t)
	    return 0;
	return (t - _tokens + rate.tokens_per_epoch() - 1) / rate.tokens_per_epoch();
    }

  private:

    token_type _tokens;
    epoch_type _epoch;

};

template <typename R>
void TokenCounterX<R>::fill(const rate_type &rate, epoch_type epoch)
{
    epoch_type diff = rate.epoch_monotonic_difference(_epoch, epoch);
    if (diff >= rate.epochs_to_fill())
	_tokens = max_tokens;
    else if (diff > 0) {
	token_type delta = diff * rate.tokens_per_epoch();
	_tokens = (_tokens + delta < _tokens ? (token_type) max_tokens : _tokens + delta);
    }
    _epoch = epoch;
}

template <typename R>
void TokenCounterX<R>::fill(const rate_type &rate)
{
    fill(rate, rate.epoch());
}

template <typename R> template <typename U>
void TokenCounterX<R>::fill(const rate_type &rate, U time)
{
    fill(rate, rate.epoch(time));
}

template <typename R>
inline void TokenCounterX<R>::remove(const rate_type &rate, token_type t)
{
    t *= rate.token_scale();
    _tokens = (_tokens < t ? 0 : _tokens - t);
}

template <typename R>
inline bool TokenCounterX<R>::remove_if(const rate_type &rate, token_type t)
{
    t *= rate.token_scale();
    if (_tokens < t || unlikely(rate.idle() && t != 0))
	return false;
    else {
	_tokens -= t;
	return true;
    }
}


/** @class TokenBucketJiffyParameters include/click/tokenbucket.hh <click/tokenbucket.hh>
 @brief  Helper class for token bucket rate limiter.

 Pass this class as the parameter to TokenRateX.  TokenBucketJiffyParameters
 measures epochs in units of jiffies.  The template parameter is the type of
 tokens. */

template <typename T>
class TokenBucketJiffyParameters { public:

    /** @brief The type of tokens.  Always unsigned. */
    typedef T token_type;

    /** @brief The type of an epoch.  Possibly signed. */
    typedef click_jiffies_t epoch_type;

    /** @brief The type of a difference between epochs.  Always signed. */
    typedef click_jiffies_difference_t epoch_difference_type;

    /** @brief Return the current epoch number.
     * @note TokenBucketJiffyParameters measures epochs in jiffies. */
    static epoch_type epoch() {
	return click_jiffies();
    }

    static epoch_type epoch(epoch_type t) {
	return t;
    }

    /** @brief Return @a b - @a a, assuming that @a b was measured after @a a.
     *
     * Some epoch measurements can, in rare cases, appear to jump backwards,
     * as timestamps do when the user changes the current time.  If this
     * happens, and @a b < @a a (even though @a b happened after @a a),
     * epoch_monotonic_difference must return 0. */
    static epoch_type epoch_monotonic_difference(epoch_type a, epoch_type b) {
#if CLICK_JIFFIES_MONOTONIC
	return b - a;
#else
	return (likely(a <= b) ? b - a : 0);
#endif
    }

    /** @brief Return true if @a a happened before @a b. */
    static bool epoch_less(epoch_type a, epoch_type b) {
	return click_jiffies_less(a, b);
    }

    /** @brief Return the number of epochs per period.
     *
     * Here, this is the number of jiffies per second. */
    static unsigned epoch_frequency() {
	return CLICK_HZ;
    }

    /** @brief Return the Timestamp representing a given number of epochs. */
    static Timestamp epoch_timestamp(epoch_type x) {
	return Timestamp::make_jiffies(x);
    }

    /** @brief Return the Timestamp representing a given number of epochs. */
    static Timestamp epoch_timestamp(epoch_difference_type x) {
	return Timestamp::make_jiffies(x);
    }

};


/** @class TokenBucket include/click/tokenbucket.hh <click/tokenbucket.hh>
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

    /** @brief Type of time epochs. */
    typedef typename rate_type::epoch_type epoch_type;

    enum {
	max_tokens = rate_type::max_tokens
    };

    /** @brief Construct an unlimited token bucket.
     *
     * This token bucket is unlimited, meaning contains(t) is true
     * for any t.  The initial epoch is 0. */
    TokenBucketX()
	: _rate(), _bucket(true) {
    }

    /** @brief Construct a token bucket representing @a rate.
     * @param rate refill rate in tokens per period
     * @param capacity maximum token accumulation
     *
     * The initial epoch is 0 and the token bucket is initially full (the
     * initial token count equals @a capacity).
     *
     * @sa assign(@a rate, @a capacity) */
    TokenBucketX(token_type rate, token_type capacity)
	: _rate(rate, capacity), _bucket(true) {
    }

    /** @brief Set the token bucket rate and capacity.
     * @param rate refill rate in tokens per period
     * @param capacity maximum token accumulation
     *
     * Sets the token bucket's rate to @a rate and capacity to @a capacity.
     * The epoch is unchanged.  The ratio of tokens/burst is unchanged by
     * the assignment, so the actual number of tokens could go up or down,
     * depending on how the rate is changed.
     *
     * If @a rate is zero, the token bucket becomes unlimited: it
     * contains an infinite number of tokens.  If @a rate is nonzero
     * but @a capacity is negative or zero, the token bucket becomes
     * empty: it never contains any tokens. */
    void assign(token_type rate, token_type capacity) {
	_rate.assign(rate, capacity);
    }

    /** @brief Return true iff the token rate is normal (not unlimited or
	idle). */
    bool normal() const {
	return _rate.normal();
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
     * Returns 0 for idle and unlimited rates.  Imprecise computer
     * arithmetic may cause the result to differ from the configured
     * rate. */
    token_type rate() const {
	return _rate.rate();
    }

    /** @brief Return the capacity in tokens.
     *
     * Returns 0 for idle rates and max_tokens for unlimited rates.
     * Imprecise computer arithmetic may cause the result to differ from the
     * configured capacity. */
    token_type capacity() const {
	return _rate.capacity();
    }

    /** @brief Return the number of tokens in the bucket.
     *
     * Returns zero for empty token buckets.  The return value is a lower
     * bound on the number of tokens, since TokenBucketX keeps track of
     * fractional tokens. */
    token_type size() const {
	return _bucket.size(_rate);
    }

    /** @brief Return true iff the token bucket is completely empty. */
    inline bool empty() const {
	return _bucket.empty(_rate);
    }

    /** @brief Return true iff the token bucket is at capacity. */
    inline bool full() const {
	return _bucket.full(_rate);
    }

    /** @brief Return true iff the token bucket has at least @a t tokens. */
    inline bool contains(token_type t) const {
	return _bucket.contains(_rate, t);
    }

    /** @brief Clear the token bucket.
     *
     * The resulting bucket will appear empty() for all normal rates.
     * @sa set(), set_full() */
    void clear() {
	_bucket.clear();
    }

    /** @brief Fill the token bucket to capacity.
     *
     * The resulting bucket will appear full() for all normal rates.
     * @sa clear(), set() */
    void set_full() {
	_bucket.set_full();
    }

    /** @brief Set the token bucket to contain @a t tokens.
     * @a t number of tokens
     *
     * The result will never have more tokens than the associated capacity.
     * For idle @a rate, the counter remains empty. */
    void set(token_type t) {
	_bucket.set(_rate, t);
    }

    /** @brief Fill the token bucket to time P::epoch().
     *
     * There are three fill() methods, useful for different methods of
     * measuring epochs.  This method call parameter_type::epoch(), which
     * returns the current epoch.  Other methods use an explicit epoch and a
     * parameter_type::epoch(U) method. */
    void fill() {
	_bucket.fill(_rate);
    }

    /** @brief Fill the token bucket for time @a epoch. */
    void fill(epoch_type epoch) {
	_bucket.fill(_rate, epoch);
    }

    /** @brief Fill the token bucket for time P::epoch(@a time). */
    template <typename U> void fill(U time) {
	_bucket.fill(_rate, time);
    }

    /** @brief Remove @a t tokens from the bucket.
     * @param t number of tokens
     * @pre @a t <= capacity
     *
     * If the token bucket contains less than @a t tokens, the new token
     * count is 0. */
    void remove(token_type t) {
	_bucket.remove(_rate, t);
    }

    /** @brief Remove @a t tokens from the bucket if it contains @a t tokens.
     * @param t number of tokens
     * @pre @a t <= capacity
     * @return true if @a t tokens were removed, false otherwise
     *
     * If the token bucket contains @a t or more tokens, calls remove(@a t)
     * and returns true.  If it contains less than @a t tokens, returns false
     * without removing any tokens. */
    bool remove_if(token_type t) {
	return _bucket.remove_if(_rate, t);
    }

    /** @brief Return the number of epochs until contains(@a t) is true.
     *
     * Returns (epoch_type) -1 if the bucket is idle. */
    epoch_type epochs_until_contains(token_type t) const {
	return _bucket.epochs_until_contains(_rate, t);
    }

  private:

    rate_type _rate;
    counter_type _bucket;

};

/** @brief Default token bucket rate.
 * @relates TokenRateX */
typedef TokenRateX<TokenBucketJiffyParameters<unsigned> > TokenRate;

/** @brief Default token counter.
 * @relates TokenCounterX */
typedef TokenCounterX<TokenRate> TokenCounter;

/** @brief Default token bucket rate limiter.
 * @relates TokenBucketX */
typedef TokenBucketX<TokenBucketJiffyParameters<unsigned> > TokenBucket;

CLICK_ENDDECLS
#endif
