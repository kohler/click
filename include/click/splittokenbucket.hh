// -*- c-basic-offset: 4 -*-
#ifndef CLICK_SPLITTOKENBUCKET_HH
#define CLICK_SPLITTOKENBUCKET_HH
#include <click/timestamp.hh>
#include <click/bigint.hh>
CLICK_DECLS
class ErrorHandler;

/** @file <click/splittokenbucket.hh>
 *  @brief  Token bucket rate limiters.
 */

/** @class TokenBucket include/click/tokenbucket.hh <click/tokenbucket.hh>
 @brief  Token bucket rate limiter.

 The TokenBucketX class implements a token bucket rate limiter.  TokenBucketX
 is initialized with a rate, in tokens per second, and a burst size in tokens.
 The token bucket fills up with tokens at the given rate, to a maximum of the
 burst size.  The contains() method reports whether a given number of tokens
 are in the token bucket.  The token bucket is emptied by the remove() and
 remove_if() methods.

 Token buckets are built on a notion of time.  TokenBucketX divides time into
 units called epochs.  The template parameter P defines the epoch unit.  The
 provided TokenBucketJiffyParameters class is designed to be used as
 TokenBucketX's parameter.  It measures epochs in units of jiffies.  The
 fill() methods fill the token bucket based on the current time, or a
 specified time.

 Two special types of rate are supported.  An <em>unlimited</em> TokenBucketX
 contains an effectively infinite number of tokens, and is indicated by a rate
 of 0.  An <em>idle</em> TokenBucketX never fills, and is indicated by a burst
 size of 0.

 TokenBucketX internally maintains fractional tokens, so it should be
 relatively precise.

 Most users will be satisfied with the TokenBucket type, which is equal to
 TokenBucketX<TokenBucketJiffyParameters<unsigned> >.

 @sa GapRate */

template <typename P> class SplitTokenBucketX;

template <typename P>
class SplitTokenRateX { public:

    /** @brief The template parameter type. */
    typedef P parameter_type;

    /** @brief Unsigned type of token counts. */
    typedef typename P::token_type token_type;

    /** @brief Type of time epochs. */
    typedef typename P::epoch_type epoch_type;

    /** @brief Construct an unlimited token bucket.
     *
     * This token bucket is unlimited. */
    inline SplitTokenRateX();

    /** @brief Construct a token bucket with @a rate.
     * @param rate refill rate in tokens per second
     * @param burst maximum token accumulation
     *
     * The initial epoch is 0 and the token bucket is initially full (the
     * initial token count equals @a burst).
     *
     * @sa assign(@a rate, @a burst). */
    inline SplitTokenRateX(unsigned rate, token_type burst);

    /** @brief Set the token bucket rate and burst size.
     * @param rate refill rate in tokens per second
     * @param burst maximum token accumulation
     *
     * Sets the token bucket's rate to @a rate and burst size to @a burst.
     *
     * If @a rate is zero, the token bucket becomes unlimited: it contains an
     * infinite number of tokens.  If @a rate is nonzero but @a burst is
     * negative or zero, the token bucket becomes empty: it never contains any
     * tokens. */
    inline void assign(unsigned rate, token_type burst);

    /** @brief Return true iff the token bucket is unlimited. */
    bool unlimited() const {
	return _token_scale == 0;
    }

    /** @brief Return true iff the token bucket is idle. */
    bool idle() const {
	return _tokens_per_epoch == 0;
    }

private:
    enum {
	max_tokens = (token_type) -1
    };

    token_type _tokens_per_epoch;	// 0 if idle()
    token_type _token_scale;		// 0 if unlimited()
    epoch_type _epochs_per_burst;

    friend class SplitTokenBucketX<P>;
};

template <typename P>
class SplitTokenBucketX { public:

    /** @brief The template parameter type. */
    typedef P parameter_type;

    /** @brief Unsigned type of token counts. */
    typedef typename P::token_type token_type;

    /** @brief Type of time epochs. */
    typedef typename P::epoch_type epoch_type;

    typedef SplitTokenRateX<P> rate_type;

    SplitTokenBucketX();

    /** @brief Clear the token bucket.
     * @post tokens() == 0
     * @sa set_tokens() */
    void clear() {
	_tokens = 0;
    }

    /** @brief Return the number of tokens in the bucket.
     * @pre !unlimited()
     *
     * Returns zero for empty token buckets.  The return value is a lower
     * bound on the number of tokens, since TokenBucketX keeps track of
     * fractional tokens. */
    token_type size(const rate_type &rate) const {
	return _tokens / rate._token_scale;
    }

    /** @brief Return true iff the token bucket is full. */
    inline bool full() const {
	return _tokens == rate_type::max_tokens;
    }

    /** @brief Return true iff the token bucket has at least @a t tokens. */
    inline bool contains(const rate_type &rate, token_type t) const {
	return t * rate._token_scale <= _tokens;
    }

    /** @brief Set the token bucket to contain @a t tokens.
     * @a t token count
     *
     * @a t is constrained to be less than or equal to the burst size. */
    void set(const rate_type &rate, token_type t) {
	if (likely(rate._tokens_per_epoch != 0 && rate._token_scale != 0)) {
	    if (unlikely(t > rate_type::max_tokens / rate._token_scale))
		_tokens = rate_type::max_tokens;
	    else
		_tokens = t * rate._token_scale;
	}
    }

    /** @brief Fill the token bucket.
     *
     * Sets the token bucket to contain a full burst's worth of tokens. */
    void set_full(const rate_type &rate) {
	if (likely(rate._tokens_per_epoch != 0))
	    _tokens = rate_type::max_tokens;
    }

    /** @brief Fill the token bucket for time P::epoch().
     *
     * There are three fill() methods, useful for different methods of
     * measuring epochs.  This method call parameter_type::epoch() and uses
     * that as the new epoch.  Other methods use an explicit epoch and a
     * parameter_type::epoch(U) method. */
    void fill(const rate_type &rate);

    /** @brief Fill the token bucket for time @a epoch. */
    void fill(const rate_type &rate, epoch_type epoch);

    /** @brief Fill the token bucket for time P::epoch(@a time). */
    template <typename U> void fill(const rate_type &rate, U time);

    /** @brief Remove @a t tokens from the bucket.
     * @param t token count
     * @pre @a t <= burst size
     *
     * If the token bucket contains less than @a t tokens, the new token count
     * is 0. */
    inline void remove(const rate_type &rate, token_type t);

    /** @brief Remove @a t tokens from the bucket if it contains @a t tokens.
     * @param t token count
     * @pre @a t <= burst size
     * @return true if @a t tokens were removed, false otherwise
     *
     * If the token bucket contains @a t or more tokens, calls remove(@a t)
     * and returns true.  If it contains less than @a t tokens, returns false
     * without removing any tokens. */
    inline bool remove_if(const rate_type &rate, token_type t);

    /** @brief Return the number of epochs until contains(@a t) is true.
     * @pre !idle() */
    inline epoch_type epochs_until_contains(const rate_type &rate, token_type t) const {
	t *= rate._token_scale;
	if (_tokens >= t)
	    return 0;
	return (t - _tokens + rate._tokens_per_epoch - 1) / rate._tokens_per_epoch;
    }

  private:

    token_type _tokens;
    epoch_type _epoch;

};

template <typename P>
SplitTokenBucketX<P>::SplitTokenBucketX()
    : _tokens(0), _epoch(0)
{
}

template <typename P>
SplitTokenRateX<P>::SplitTokenRateX()
    : _tokens_per_epoch(0), _token_scale(0), _epochs_per_burst(1)
{
}

template <typename P>
SplitTokenRateX<P>::SplitTokenRateX(unsigned rate, token_type burst)
{
    assign(rate, burst);
}

template <typename P>
void SplitTokenRateX<P>::assign(unsigned rate, token_type burst)
{
    if (rate == 0) {
	_token_scale = 0;
	_tokens_per_epoch = 1;
	_epochs_per_burst = 1;
	return;
    } else if (burst <= 0) {
	_token_scale = 1;
	_tokens_per_epoch = 0;
	_epochs_per_burst = 0;
	return;
    }

    _token_scale = max_tokens / burst;

    // XXX on non-32 bit types
    static_assert(sizeof(bigint::limb_type) == sizeof(token_type));
    static_assert(sizeof(bigint::limb_type) == sizeof(unsigned));
    bigint::limb_type l[2] = { 0, 0 };
    bigint::limb_type a[2] = { rate, 0 };
    bigint::multiply_add(l, a, 2, _token_scale);
    (void) bigint::divide(l, l, 2, P::epoch_frequency());
    _tokens_per_epoch = l[0];
    assert(l[1] == 0);

    _epochs_per_burst = (max_tokens / _tokens_per_epoch);
}

template <typename P>
void SplitTokenBucketX<P>::fill(const rate_type &rate, epoch_type epoch)
{
    typename P::epoch_type diff = P::epoch_monotonic_difference(_epoch, epoch);
    if (diff > rate._epochs_per_burst && likely(rate._tokens_per_epoch != 0))
	_tokens = rate_type::max_tokens;
    else if (diff > 0) {
	token_type delta = diff * rate._tokens_per_epoch;
	_tokens = (_tokens + delta < _tokens ? rate_type::max_tokens : _tokens + delta);
    }
    _epoch = epoch;
}

template <typename P>
void SplitTokenBucketX<P>::fill(const rate_type &rate)
{
    fill(rate, P::epoch());
}

template <typename P> template <typename U>
void SplitTokenBucketX<P>::fill(const rate_type &rate, U time)
{
    fill(rate, P::epoch(time));
}

template <typename P>
inline void SplitTokenBucketX<P>::remove(const rate_type &rate, token_type t)
{
    t *= rate._token_scale;
    _tokens = (_tokens < t ? 0 : _tokens - t);
}

template <typename P>
inline bool SplitTokenBucketX<P>::remove_if(const rate_type &rate, token_type t)
{
    t *= rate._token_scale;
    if (_tokens < t)
	return false;
    else {
	_tokens -= t;
	return true;
    }
}


/** @class TokenBucketJiffyParameters include/click/tokenbucket.hh <click/tokenbucket.hh>
 @brief  Helper class for token bucket rate limiter.

 Pass this class as the parameter to SplitTokenRateX.  TokenBucketJiffyParameters
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
     * @note TokenBucketParameters measures epochs in jiffies. */
    static epoch_type epoch() {
	return click_jiffies();
    }

    /** @brief Return @a b - @a a, assuming that @a b was measured after @a a.
     *
     * Some epoch measurements can, in rare cases, appear to jump backwards,
     * as timestamps do when the user changes the current time.  If this
     * happens, and @a b < @a a (even though @a b happened after @a a),
     * epoch_monotonic_difference must returns 0. */
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

    /** @brief Return the number of epochs (jiffies) per second. */
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


/** @brief Default token bucket rate limiter.
 * @relates SplitTokenRateX */
typedef SplitTokenRateX<TokenBucketJiffyParameters<unsigned> > SplitTokenRate;
typedef SplitTokenBucketX<TokenBucketJiffyParameters<unsigned> > SplitTokenBucket;

CLICK_ENDDECLS
#endif
