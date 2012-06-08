#ifndef CLICK_BIGINT_HH
#define CLICK_BIGINT_HH 1
/*
 * bigint.hh -- multiple-precision arithmetic
 * Eddie Kohler and the authors of GMP
 *
 * Copyright (c) 2008 Meraki, Inc.
 * Derived from the GNU Multiple Precision Arithmetic Library, which is
 * Copyright (c) 2001-2008 Free Software Foundation, Inc.
 *
 * This source code is free software; you can redistribute it and/or modify it
 * under the terms of the GNU Lesser General Public License as published by
 * the Free Software Foundation, version 3.
 *
 * This program is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE.  See the GNU Lesser General Public
 * License for more details.
 *
 * Using the interfaces provided by this file does not create a derived work.
 */
#include <click/integers.hh>
#include <click/type_traits.hh>
#include <click/string.hh>
CLICK_DECLS

/** @file <click/bigint.hh>
 * @brief Simple multiple-precision integer arithmetic.
 */

/** @class Bigint
 * @brief Template for multiple-precision integer arithmetic.
 *
 * The Bigint template provides a couple simple, but useful, functions for
 * multiple-precision integer arithmetic.  It is a port of a very limited
 * subset of the GNU Multiple Precision Arithmetic library's mpn functionality
 * (architecture-generic functions that work on unsigned integers of known
 * size).
 *
 * A multiple-precision integer is an array of "limbs," or machine integers.
 * The multiply_add() and divide() functions take arrays of limbs as
 * arguments.  In such arrays, the least significant limb is stored at array
 * index 0.
 *
 * For example, here's Bigint speak for "x = 0x123_4567_89AB_CDEF * 0x45":
 * @code
 * // Initialize result
 * uint32_t x[2] = { 0, 0 };
 * // Define multiplicand; note that least significant comes first
 * uint32_t a[2] = { 0x89ABCDEF, 0x1234567 };
 * Bigint<uint32_t>::multiply_add(x, a, 2, 0x45);
 * @endcode
 *
 * Bigint is not optimized for speed.
 *
 * Bigint's template parameter is the type of a limb.  Most users will use
 * bigint, which is equivalent to Bigint<uint32_t>.
 */
template <typename L, typename Lhalf = L>
class Bigint { public:

    /** @brief Type of a limb, the unit of single arithmetic. */
    typedef L limb_type;

    /** @brief Type of an arithmetic unit no smaller than one-half limb_type. */
    typedef Lhalf half_limb_type;

    enum {
	limb_bits = sizeof(limb_type) * 8,
	half_limb_bits = sizeof(limb_type) * 4
    };
    enum {
	limb_high_bit = limb_type(1) << (limb_bits - 1),
	limb_half = limb_type(1) << half_limb_bits,
	limb_low_mask = limb_half - 1,
	limb_zero = limb_type(0)
    };

    /** @brief Return the less significant half of @a x.
     * @invariant low(@a x) + ((limb_type) high(@a x) << half_limb_bits) ==
     *    @a x */
    static inline half_limb_type low(limb_type x) {
	return x & limb_low_mask;
    }

    /** @brief Return the more significant half of @a x. */
    static inline half_limb_type high(limb_type x) {
	return x >> half_limb_bits;
    }

    /** @brief Add two-limb integers [@a a1,@a a0] and [@a b1,@a b0], storing
     * the result in [@a x1,@a x0].
     * @param[out] x1 most significant limb of result
     * @param[out] x0 least significant limb of result
     * @param a1 most significant limb of addend
     * @param a0 least significant limb of addend
     * @param b1 most significant limb of addend
     * @param b0 least significant limb of addend
     *
     * Handles carry in the low order limbs. */
    static void add(limb_type &x1, limb_type &x0,
		    limb_type a1, limb_type a0, limb_type b1, limb_type b0) {
	x0 = a0 + b0;
	x1 = a1 + b1 + (x0 < a0);
    }

    /** @brief Multiply one-limb integers @a a and @a b, storing the result in
     * [@a x1,@a x0].
     * @param[out] x1 most significant limb of result
     * @param[out] x0 least significant limb of result
     * @param a multiplicand
     * @param b multiplicand */
    static void multiply(limb_type &x1, limb_type &x0,
			 limb_type a, limb_type b) {
	int_multiply(a, b, x0, x1);
    }

    /** @brief Multiply one-limb integer @a a by half-limb integer @a b,
     * storing the result in [@a x1,@a x0].
     * @param[out] x1 most significant limb of result
     * @param[out] x0 least significant limb of result
     * @param a multiplicand
     * @param b multiplicand */
    static void multiply_half(limb_type &x1, limb_type &x0,
			      limb_type a, half_limb_type b) {
	if (has_fast_int_multiply<limb_type>::value)
	    multiply(x1, x0, a, b);
	else {
	    half_limb_type al = low(a), ah = high(a);

	    limb_type r0 = (limb_type) al * b;

	    limb_type r1 = (limb_type) ah * b;
	    limb_type r2 = high(r0) + r1;

	    x1 = (r2 < r1 ? limb_half : limb_zero) + high(r2);
	    x0 = (r2 << half_limb_bits) + low(r0);
	}
    }

    /** @brief Return the inverse of limb @a x.
     *
     * Returns the limb @a y that is the largest limb not larger than
     * (2**(2*limb_bits))/@a x - (2**limb_bits).  If this would yield
     * overflow, @a y is the largest possible number (i.e., only ones). */
    static limb_type inverse(limb_type x) {
	limb_type y1 = ~x, y0 = ~limb_zero;

	// now calculate [y1y0] / x
	limb_type x1 = high(x), x0 = low(x);
	limb_type q1 = y1 / x1;
	limb_type r1 = y1 - q1 * x1;
	limb_type m = q1 * x0;
	r1 = r1 * limb_half | high(y0);
	if (r1 < m) {
	    --q1, r1 += x;
	    if (r1 >= x && r1 < m)
		--q1, r1 += x;
	}
	r1 -= m;

	limb_type q0 = r1 / x1;
	limb_type r0 = r1 - q0 * x1;
	m = q0 * x0;
	r0 = r0 * limb_half | low(y0);
	if (r0 < m) {
	    --q0, r0 += x;
	    if (r0 >= x && r0 < m)
		--q0, r0 += x;
	}

	return q1 * limb_half | q0;
    }

    /** @brief Set @a n-limb integer @a a to the value @a b.
     * @param[out] a points to @a n-limb result
     * @param n number of limbs in @a a
     * @param b input value
     * @return carry */
    static click_uint_large_t set(limb_type *a, int n, click_uint_large_t b) {
	while (n > 0) {
	    *a++ = b;
	    n--;
	    b >>= limb_bits;
	}
	return b;
    }

    /** @brief Multiply @a n-limb integer @a a by 1-limb integer @a b and add
     * the result to @a n-limb integer @a x.
     * @param[in,out] x points to @a n-limb addend and result
     * @param a points to @a n-limb multiplicand
     * @param n number of limbs in @a x and @a a
     * @param b 1-limb multiplicand
     * @return overflow
     *
     * Like @a x += @a a * @a b.  Both @a x and @a a must have @a n limbs.  It
     * is safe for @a x and @a a to point to exactly the same memory, but they
     * must not otherwise overlap. */
    static limb_type multiply_add(limb_type *x, const limb_type *a, int n,
				  limb_type b) {
	limb_type carry = 0;
	do {
	    limb_type x0, x1;
	    multiply(x1, x0, *a++, b);
	    x0 += carry;
	    carry = (x0 < carry) + x1;
	    x0 += *x;
	    carry += (x0 < *x);
	    *x++ = x0;
	} while (--n != 0);
	return carry;
    }

    /** @brief Multiply @a n-limb integer @a a by 1-limb integer @a b, add
     * 1-limb integer @a carry, and store the result in @a n-limb integer @a x.
     * @param[in,out] x points to @a n-limb result
     * @param a points to @a n-limb multiplicand
     * @param n number of limbs in @a x and @a a
     * @param b 1-limb multiplicand
     * @param carry 1-limb initial carry
     * @return overflow
     *
     * Like @a x = (@a a * @a b) + @a carry.  Both @a x and @a a must have @a
     * n limbs.  It is safe for @a x and @a a to point to exactly the same
     * memory, but they must not otherwise overlap. */
    static limb_type multiply(limb_type *x, const limb_type *a, int n,
			      limb_type b, limb_type carry = 0) {
	do {
	    limb_type x0, x1;
	    multiply(x1, x0, *a++, b);
	    x0 += carry;
	    carry = (x0 < carry) + x1;
	    *x++ = x0;
	} while (--n != 0);
	return carry;
    }

    /** @brief Multiply @a n-limb integer @a a by 1/2-limb integer @a b, add
     * 1-limb integer @a carry, and store the result in @a n-limb integer @a x.
     * @param[in,out] x points to @a n-limb result
     * @param a points to @a n-limb multiplicand
     * @param n number of limbs in @a x and @a a
     * @param b 1/2-limb multiplicand
     * @param carry 1-limb initial carry
     * @return overflow
     *
     * Like @a x = (@a a * @a b) + @a carry.  Both @a x and @a a must have @a
     * n limbs.  It is safe for @a x and @a a to point to exactly the same
     * memory, but they must not otherwise overlap. */
    static limb_type multiply_half(limb_type *x, const limb_type *a, int n,
				   half_limb_type b, limb_type carry = 0) {
	do {
	    limb_type x0, x1;
	    multiply_half(x1, x0, *a++, b);
	    x0 += carry;
	    carry = (x0 < carry) + x1;
	    *x++ = x0;
	} while (--n != 0);
	return carry;
    }

    /** @brief Divide @a n-limb integer @a a by 1-limb integer @a b and store
     * the result in @a n-limb integer @a x.
     * @param[out] x points to @a n-limb result
     * @param a points to @a n-limb dividend
     * @param n number of limbs in @a x and @a a
     * @param b 1-limb divisor
     * @return the remainder
     *
     * Like @a x = @a a / @a b.  Both @a x and @a a must have @a n limbs.  It
     * is safe for @a x and @a a to point to exactly the same memory, but they
     * must not otherwise overlap. */
    static limb_type divide(limb_type *x, const limb_type *a, int n,
			    limb_type b) {
	x += n - 1;   /* Make x point at most significant quotient limb */
	a += n - 1;   /* Make a point at most significant dividend limb */
	limb_type r = 0;

	if (b & limb_high_bit) {
	    /* High quotient limb is 0 or 1, skip a divide step. */
	    r = *a;
	    *x = (r >= b);
	    r -= (b & -*x);
	    --x, --a, --n;

	    /* Multiply-by-inverse, divisor already normalized. */
	    limb_type b_inverse = inverse(b);
	    while (n > 0) {
		preinverted_divide(*x, r, r, *a, b, b_inverse);
		--x, --a, --n;
	    }

	} else {
	    /* Most significant bit of divisor == 0.  */
	    /* Skip a division if high < divisor (high quotient 0).  Testing
	       here before normalizing will still skip as often as
	       possible.  */
	    if (*a < b) {
		r = *a;
		*x = 0;
		--x, --a, --n;
	    }

	    if (n != 0) {
		int norm = ffs_msb(b) - 1; // number of most significant 0 bits
		b <<= norm;
		r <<= norm;
		limb_type b_inverse = inverse(b);

		limb_type a1 = *a;
		r |= (a1 >> (limb_bits - norm));
		--a, --n;
		while (n > 0) {
		    limb_type a0 = *a;
		    preinverted_divide(*x, r, r, ((a1 << norm) | (a0 >> (limb_bits - norm))), b, b_inverse);
		    --x, --a, --n;
		    a1 = a0;
		}
		preinverted_divide(*x, r, r, a1 << norm, b, b_inverse);
		r >>= norm;
	    }
	}

	return r;
    }

    /** @brief Return a string representation of @a n-limb integer @a x and
     * set @a x to 0.
     * @param[in,out] x @a n-limb input, set to zero on output
     * @param n number of limbs in @a x
     * @param base base (between 2 and 36)
     * @param uppercase if true, use uppercase letters for digits >= 10 */
    static String unparse_clear(limb_type *x, int n, int base = 10, bool uppercase = false) {
	// need d chars, min d s.t. 10^d >= 2^(sizeof(limb_type) * 8 * n)
	// == min d s.t. d >= sizeof(limb_type) * 8 * n / lg 10
	int div = (base >= 16 ? 4 : (base >= 8 ? 3 : 1));
	String s = String::make_uninitialized((n * limb_bits) / div + 1);
	char *q = const_cast<char *>(s.end());
	assert(base >= 2 && base <= 36);
	while (1) {
	    while (n > 0 && x[n - 1] == 0)
		--n;
	    if (n == 0)
		break;
	    int r = divide(x, x, n, base);
	    if (r <= 9)
		*--q = '0' + r;
	    else if (uppercase)
		*--q = 'A' + r - 10;
	    else
		*--q = 'a' + r - 10;
	}
	assert(q >= s.begin());
	if (q == s.end())
	    *--q = '0';
	return s.substring(q, s.end());
    }

  private:

    /* Divide the two-limb number in (a1,a0) by b, with b_inverse being the
       largest limb not larger than (2**(2*limb_bits))/b - (2**limb_bits).  If
       this would yield overflow, b_inverse should be the largest possible
       number (i.e., only ones).  For correct operation, the most significant
       bit of b has to be set.  Put the quotient in q and the remainder in
       r.  */
    /* Like udiv_qrnnd_preinv, but branch-free. */
    static void preinverted_divide(limb_type &q, limb_type &r,
				   limb_type a1, limb_type a0, limb_type b,
				   limb_type b_inverse) {
	limb_type a0_mask = (a0 & limb_high_bit ? ~limb_zero : limb_zero);
	limb_type a0_adjusted = a0 + (b & a0_mask);
	limb_type x1, x0;
	multiply(x1, x0, b_inverse, a1 - a0_mask);
	add(x1, x0, x1, x0, a1, a0_adjusted);
	limb_type q1 = ~x1;
	multiply(x1, x0, q1, b);
	add(x1, x0, x1, x0, a1, a0);
	x1 -= b;
	r = x0 + (b & x1);
	q = x1 - q1;
    }

};

/** @brief Typical Bigint usage with uint32_t limb_type. */
typedef Bigint<uint32_t> bigint;

CLICK_ENDDECLS
#endif
