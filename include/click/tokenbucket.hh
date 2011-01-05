// -*- c-basic-offset: 4 -*-
#ifndef CLICK_TOKENBUCKET_HH
#define CLICK_TOKENBUCKET_HH
#include <click/timestamp.hh>
#include <click/bigint.hh>
#include <click/splittokenbucket.hh>
CLICK_DECLS
class ErrorHandler;

/** @file <click/tokenbucket.hh>
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

template <typename P>
class TokenBucketX { public:

    /** @brief The template parameter type. */
    typedef P parameter_type;

    /** @brief Unsigned type of token counts. */
    typedef typename P::token_type token_type;

    /** @brief Type of time epochs. */
    typedef typename P::epoch_type epoch_type;

    /** @brief Construct an unlimited token bucket.
     *
     * This token bucket is unlimited, meaning contains(t) is true
     * for any t.  The initial epoch is 0. */
    TokenBucketX() : _rate(), _bucket() {}

    /** @brief Construct a token bucket with @a rate.
     * @param rate refill rate in tokens per second
     * @param burst maximum token accumulation
     *
     * The initial epoch is 0 and the token bucket is initially full (the
     * initial token count equals @a burst).
     *
     * @sa assign(@a rate, @a burst). */
    TokenBucketX(unsigned rate, token_type burst) : _rate(rate, burst), _bucket() {}

    /** @brief Set the token bucket rate and burst size.
     * @param rate refill rate in tokens per second
     * @param burst maximum token accumulation
     *
     * Sets the token bucket's rate to @a rate and burst size to @a burst.  The
     * epoch is unchanged.  The ratio of tokens/burst is unchanged by the
     * assignment, so the actual number of tokens could go up or down, depending
     * on how the rate is changed.
     *
     * If @a rate is zero, the token bucket becomes unlimited: it contains an
     * infinite number of tokens.  If @a rate is nonzero but @a burst is
     * negative or zero, the token bucket becomes empty: it never contains any
     * tokens. */
    void assign(unsigned rate, token_type burst) {
	return _rate.assign(rate, burst);
    }

    /** @brief Return true iff the token bucket is unlimited. */
    bool unlimited() const {
	return _rate.unlimited();
    }

    /** @brief Return true iff the token bucket is idle. */
    bool idle() const {
	return _rate.idle();
    }

    /** @brief Clear the token bucket.
     * @post tokens() == 0
     * @sa set_tokens() */
    void clear() {
	_bucket.clear();
    }

    /** @brief Return the number of tokens in the bucket.
     * @pre !unlimited()
     *
     * Returns zero for empty token buckets.  The return value is a lower
     * bound on the number of tokens, since TokenBucketX keeps track of
     * fractional tokens. */
    token_type size() const {
	return _bucket.size(_rate);
    }

    /** @brief Return true iff the token bucket is full. */
    inline bool full() const {
	return _bucket.full();
    }

    /** @brief Return true iff the token bucket has at least @a t tokens. */
    inline bool contains(token_type t) const {
	return _bucket.contains(_rate, t);
    }

    /** @brief Set the token bucket to contain @a t tokens.
     * @a t token count
     *
     * @a t is constrained to be less than or equal to the burst size. */
    void set(token_type t) {
	_bucket.set(_rate, t);
    }

    /** @brief Fill the token bucket.
     *
     * Sets the token bucket to contain a full burst's worth of tokens. */
    void set_full() {
	_bucket.set_full(_rate);
    }

    /** @brief Fill the token bucket for time P::epoch().
     *
     * There are three fill() methods, useful for different methods of
     * measuring epochs.  This method call parameter_type::epoch() and uses
     * that as the new epoch.  Other methods use an explicit epoch and a
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
     * @param t token count
     * @pre @a t <= burst size
     *
     * If the token bucket contains less than @a t tokens, the new token count
     * is 0. */
    void remove(token_type t) {
	_bucket.remove(_rate, t);
    }

    /** @brief Remove @a t tokens from the bucket if it contains @a t tokens.
     * @param t token count
     * @pre @a t <= burst size
     * @return true if @a t tokens were removed, false otherwise
     *
     * If the token bucket contains @a t or more tokens, calls remove(@a t)
     * and returns true.  If it contains less than @a t tokens, returns false
     * without removing any tokens. */
    bool remove_if(token_type t) {
	return _bucket.remove_if(_rate, t);
    }

    /** @brief Return the number of epochs until contains(@a t) is true.
     * @pre !idle() */
    epoch_type epochs_until_contains(token_type t) const {
	return _bucket.epochs_until_contains(_rate, t);
    }

  private:

    SplitTokenRateX<P> _rate;
    SplitTokenBucketX<P> _bucket;

};

/** @brief Default token bucket rate limiter.
 * @relates TokenBucketX */
typedef TokenBucketX<TokenBucketJiffyParameters<unsigned> > TokenBucket;

CLICK_ENDDECLS
#endif
