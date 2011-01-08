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
 contains an infinite number of tokens.  An <em>idle</em> TokenRateX never
 refills (although an idle TokenCounter can be explicitly set to contain a
 nonzero number of tokens).

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
	return _epochs_until_full == 0;
    }

    /** @brief Return true iff the token rate is idle. */
    bool idle() const {
	return _tokens_per_epoch == 0;
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
	return max_tokens / (_token_scale ? _token_scale : 1);
    }

    /** @brief Return the number of tokens per epoch. */
    token_type tokens_per_epoch() const {
	return _tokens_per_epoch;
    }

    /** @brief Return the ratio of fractional tokens to real tokens.
     *
     * The result is 0 for unlimited rates. */
    token_type token_scale() const {
	return _token_scale;
    }

    /** @brief Return max(token_scale(), 1). */
    token_type token_scale1() const {
	return _token_scale ? _token_scale : 1;
    }

    /** @brief Return the number of epochs required to refill a counter to
     * capacity.
     *
     * Returns (epoch_type) -1 for idle rates. */
    epoch_type epochs_until_full() const {
	return _epochs_until_full;
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
    epoch_type _epochs_until_full;

};


template <typename P>
void TokenRateX<P>::assign(bool unlimited)
{
    if (unlimited) {
	_token_scale = 0;
	_tokens_per_epoch = max_tokens;
	_epochs_until_full = 0;
    } else {
	// Give a nominal capacity of 65535.
	_token_scale = max_tokens / 65535;
	_tokens_per_epoch = 0;
	_epochs_until_full = (epoch_type) -1;
    }
}

template <typename P>
void TokenRateX<P>::assign(token_type rate, token_type capacity)
{
    if (rate != 0 && capacity != 0) {
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

	_epochs_until_full = (max_tokens - 1) / _tokens_per_epoch + 1;
    } else
	assign(false);
}

template <typename P>
typename P::token_type TokenRateX<P>::rate() const
{
    static_assert(sizeof(bigint::limb_type) == sizeof(token_type));
    bigint::limb_type l[2] = { _tokens_per_epoch / 2, 0 };
    bigint::limb_type a[2] = { _tokens_per_epoch, 0 };
    bigint::multiply_add(l, a, 2, P::epoch_frequency());
    (void) bigint::divide(l, l, 2, token_scale1());
    return l[1] ? (token_type) max_tokens : l[0];
}

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
     * TokenCounterX keeps track of fractional tokens. */
    token_type size(const rate_type &rate) const {
	if (rate.token_scale() == 0)
	    return max_tokens;
	else
	    return _tokens / rate.token_scale();
    }

    /** @brief Test if the token counter is completely empty. */
    bool empty(const rate_type &rate) const {
	return _tokens == 0 && rate.token_scale() != 0;
    }

    /** @brief Test if the token counter is at full capacity. */
    bool full(const rate_type &rate) const {
	return _tokens == (token_type) max_tokens || rate.token_scale() == 0;
    }

    /** @brief Test if the token counter has at least @a t tokens.
     * @param rate associated token rate
     *
     * Returns true whenever @a t is zero or @a rate is unlimited.  Returns
     * false whenever @a t is greater than @a rate.capacity().
     *
     * @sa fast_contains */
    bool contains(const rate_type &rate, token_type t) const {
	return t <= rate.capacity() && fast_contains(rate, t);
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
	if (t > rate.capacity() || rate.token_scale() == 0)
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
	if (old_rate.token_scale() != new_rate.token_scale()) {
	    static_assert(sizeof(bigint::limb_type) == sizeof(token_type));
	    bigint::limb_type l[2] = { 0, 0 };
	    bigint::limb_type a[2] = { _tokens, 0 };
	    bigint::multiply_add(l, a, 2, new_rate.token_scale1());
	    (void) bigint::divide(l, l, 2, old_rate.token_scale1());
	    _tokens = l[1] ? (token_type) max_tokens : l[0];
	}
    }

    /** @brief Refill the token counter to time @a rate.epoch().
     * @param rate associated token rate
     *
     * There are three refill() methods, useful for different methods of
     * measuring epochs.  This method calls @a rate.epoch(), which returns the
     * current epoch.  Other methods use an explicit epoch and a @a
     * rate.epoch(U) method. */
    void refill(const rate_type &rate);

    /** @brief Refill the token counter for time @a epoch.
     * @param rate associated token rate */
    void refill(const rate_type &rate, epoch_type epoch);

    /** @brief Refill the token counter for @a time.
     * @param rate associated token rate */
    template <typename U> void refill(const rate_type &rate, U time);

    /** @brief Remove @a t tokens from the counter.
     * @param rate associated token rate
     * @param t number of tokens
     *
     * If the token counter contains less than @a t tokens, the new token
     * count is 0.
     *
     * @sa fast_remove */
    void remove(const rate_type &rate, token_type t) {
	if (t > rate.capacity())
	    _tokens = 0;
	else
	    fast_remove(rate, t);
    }

    /** @brief Remove @a t tokens from the counter if it contains @a t tokens.
     * @param rate associated token rate
     * @param t number of tokens
     * @return true if @a t tokens were removed, false otherwise
     *
     * If the counter contains @a t or more tokens, calls remove(@a t) and
     * returns true.  If it contains less than @a t tokens, returns false
     * without removing any tokens.
     *
     * @sa fast_remove_if */
    bool remove_if(const rate_type &rate, token_type t) {
	return t <= rate.capacity() && fast_remove_if(rate, t);
    }

    /** @brief Return the number of epochs until contains(@a t) is true.
     * @param rate associated token rate
     *
     * Returns (epoch_type) -1 if passing time will never make contains(@a t)
     * true. */
    epoch_type epochs_until_contains(const rate_type &rate,
				     token_type t) const {
	if (t <= rate.capacity()) {
	    t *= rate.token_scale();
	    if (t <= _tokens)
		return 0;
	    else if (rate.tokens_per_epoch() == 0)
		return (epoch_type) -1;
	    else
		return (t - _tokens - 1) / rate.tokens_per_epoch() + 1;
	} else
	    return (epoch_type) -1;
    }


    /** @brief Return true iff the token counter has at least @a t tokens.
     * @param rate associated token rate
     * @pre @a t <= @a rate.capacity()
     *
     * Returns true whenever @a t is zero or @a rate is unlimited.
     *
     * Consider using fast_contains() when you know that @a t <= @a
     * rate.capacity(); it is slightly faster than contains(). */
    bool fast_contains(const rate_type &rate, token_type t) const {
	return t * rate.token_scale() <= _tokens;
    }

    /** @brief Remove @a t tokens from the counter.
     * @param rate associated token rate
     * @param t number of tokens
     * @pre @a t <= @a rate.capacity()
     *
     * If the token counter contains less than @a t tokens, the new token
     * count is 0.
     *
     * Consider using fast_remove() when you know that @a t <= @a
     * rate.capacity(); it is slightly faster than remove(). */
    void fast_remove(const rate_type &rate, token_type t) {
	t *= rate.token_scale();
	_tokens = (t <= _tokens ? _tokens - t : 0);
    }

    /** @brief Remove @a t tokens from the counter if it contains @a t tokens.
     * @param rate associated token rate
     * @param t number of tokens
     * @pre @a t <= @a rate.capacity()
     * @return true if @a t tokens were removed, false otherwise
     *
     * If the counter contains @a t or more tokens, calls remove(@a t)
     * and returns true.  If it contains less than @a t tokens, returns false
     * without removing any tokens.
     *
     * Consider using fast_remove() when you know that @a t <= @a
     * rate.capacity(); it is slightly faster than remove_if(). */
    bool fast_remove_if(const rate_type &rate, token_type t) {
	t *= rate.token_scale();
	if (t <= _tokens) {
	    _tokens -= t;
	    return true;
	} else
	    return false;
    }

  private:

    token_type _tokens;
    epoch_type _epoch;

};

template <typename R>
void TokenCounterX<R>::refill(const rate_type &rate, epoch_type epoch)
{
    epoch_type diff = rate.epoch_monotonic_difference(_epoch, epoch);
    if (diff >= rate.epochs_until_full()) {
	// special case: idle rates never fill, but rate.epochs_until_full()
	// might be -1 if epoch_type is signed
	if (rate.tokens_per_epoch() != 0)
	    _tokens = max_tokens;
    } else if (diff > 0) {
	token_type new_tokens = _tokens + diff * rate.tokens_per_epoch();
	_tokens = (new_tokens < _tokens ? (token_type) max_tokens : new_tokens);
    }
    _epoch = epoch;
}

template <typename R>
void TokenCounterX<R>::refill(const rate_type &rate)
{
    refill(rate, rate.epoch());
}

template <typename R> template <typename U>
void TokenCounterX<R>::refill(const rate_type &rate, U time)
{
    refill(rate, rate.epoch(time));
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

    /** @brief Construct an idle token bucket.
     *
     * The initial epoch is 0. */
    TokenBucketX() {
    }

    /** @brief Construct an idle or unlimited token bucket.
     * @param unlimited idle if false, unlimited if true
     *
     * The initial epoch is 0. */
    explicit TokenBucketX(bool unlimited)
	: _rate(unlimited), _bucket(unlimited) {
    }

    /** @brief Construct a token bucket representing @a rate.
     * @param rate refill rate in tokens per period
     * @param capacity maximum token accumulation
     *
     * The initial epoch is 0 and the token bucket is initially full (the
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
     * The epoch is unchanged.
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

    /** @brief Test if the token bucket is completely empty. */
    bool empty() const {
	return _bucket.empty(_rate);
    }

    /** @brief Test if the token bucket is at full capacity. */
    bool full() const {
	return _bucket.full(_rate);
    }

    /** @brief Test if the token bucket has at least @a t tokens.
     *
     * Returns true whenever @a t is zero or @a rate is unlimited.  Returns
     * false whenever @a t is greater than @a rate.capacity().
     *
     * @sa fast_contains */
    bool contains(token_type t) const {
	return _bucket.contains(_rate, t);
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

    /** @brief Refill the token bucket to time P::epoch().
     *
     * There are three refill() methods, useful for different methods of
     * measuring epochs.  This method call parameter_type::epoch(), which
     * returns the current epoch.  Other methods use an explicit epoch and a
     * parameter_type::epoch(U) method. */
    void refill() {
	_bucket.refill(_rate);
    }

    /** @brief Refill the token bucket for time @a epoch. */
    void refill(epoch_type epoch) {
	_bucket.refill(_rate, epoch);
    }

    /** @brief Refill the token bucket for time P::epoch(@a time). */
    template <typename U> void refill(U time) {
	_bucket.refill(_rate, time);
    }

    /** @brief Remove @a t tokens from the bucket.
     * @param t number of tokens
     *
     * If the token bucket contains less than @a t tokens, the new token
     * count is 0.
     *
     * @sa fast_remove */
    void remove(token_type t) {
	_bucket.remove(_rate, t);
    }

    /** @brief Remove @a t tokens from the bucket if it contains @a t tokens.
     * @param t number of tokens
     * @return true if @a t tokens were removed, false otherwise
     *
     * If the token bucket contains @a t or more tokens, calls remove(@a t)
     * and returns true.  If it contains less than @a t tokens, returns false
     * without removing any tokens.
     *
     * @sa fast_remove_if */
    bool remove_if(token_type t) {
	return _bucket.remove_if(_rate, t);
    }

    /** @brief Return the number of epochs until contains(@a t) is true.
     *
     * Returns (epoch_type) -1 if passing time will never make contains(@a t)
     * true. */
    epoch_type epochs_until_contains(token_type t) const {
	return _bucket.epochs_until_contains(_rate, t);
    }


    /** @brief Return true iff the token bucket has at least @a t tokens.
     * @pre @a t <= capacity()
     *
     * Returns true whenever @a t is zero or the token bucket is unlimited.
     *
     * Consider using fast_contains() when you know that @a t <= capacity();
     * it is slightly faster than contains(). */
    bool fast_contains(token_type t) const {
	return _bucket.fast_contains(_rate, t);
    }

    /** @brief Remove @a t tokens from the counter.
     * @param t number of tokens
     * @pre @a t <= capacity()
     *
     * If the token bucket contains less than @a t tokens, the new token count
     * is 0.
     *
     * Consider using fast_remove() when you know that @a t <= capacity(); it
     * is slightly faster than remove(). */
    void fast_remove(token_type t) {
	_bucket.fast_remove(_rate, t);
    }

    /** @brief Remove @a t tokens from the counter if it contains @a t tokens.
     * @param t number of tokens
     * @pre @a t <= capacity()
     * @return true if @a t tokens were removed, false otherwise
     *
     * If the bucket contains @a t or more tokens, calls remove(@a t)
     * and returns true.  If it contains less than @a t tokens, returns false
     * without removing any tokens.
     *
     * Consider using fast_remove() when you know that @a t <= capacity(); it
     * is slightly faster than remove_if(). */
    bool fast_remove_if(token_type t) {
	return _bucket.fast_remove_if(_rate, t);
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
