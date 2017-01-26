// libdivide.h
// Copyright 2010 - 2016 ridiculous_fish

/*
  libdivide
  Copyright (C) 2010 ridiculous_fish

  This software is provided 'as-is', without any express or implied
  warranty.  In no event will the authors be held liable for any damages
  arising from the use of this software.

  Permission is granted to anyone to use this software for any purpose,
  including commercial applications, and to alter it and redistribute it
  freely, subject to the following restrictions:

  1. The origin of this software must not be misrepresented; you must not
     claim that you wrote the original software. If you use this software
     in a product, an acknowledgment in the product documentation would be
     appreciated but is not required.
  2. Altered source versions must be plainly marked as such, and must not be
     misrepresented as being the original software.
  3. This notice may not be removed or altered from any source distribution.

libdivide@ridiculousfish.com
*/
/* modified from https://github.com/ridiculousfish/libdivide for click */

#ifndef CLICK_LIBDIVIDE_H
#define CLICK_LIBDIVIDE_H
#include <click/glue.hh>
#include <click/integers.hh>
CLICK_DECLS

#ifndef __has_builtin
#define __has_builtin(x) 0 // Compatibility with non-clang compilers.
#endif

#if defined(__SIZEOF_INT128__)
#define HAS_INT128_T 1
#endif

#if defined(__x86_64__) || defined(_WIN64) || defined(_M_X64)
#define LIBDIVIDE_IS_X86_64 1
#endif

#if defined(__i386__)
#define LIBDIVIDE_IS_i386 1
#endif

#if __GNUC__ || __clang__
#define LIBDIVIDE_GCC_STYLE_ASM 1
#endif

#define LIBDIVIDE_ASSERT(x) assert(x)

// Explanation of "more" field: bit 6 is whether to use shift path. If we are
// using the shift path, bit 7 is whether the divisor is negative in the signed
// case; in the unsigned case it is 0. Bits 0-4 is shift value (for shift
// path or mult path).  In 32 bit case, bit 5 is always 0. We use bit 7 as the
// "negative divisor indicator" so that we can use sign extension to
// efficiently go to a full-width -1.
//
// u32: [0-4] shift value
//      [5] ignored
//      [6] add indicator
//      [7] shift path
//
// s32: [0-4] shift value
//      [5] shift path
//      [6] add indicator
//      [7] indicates negative divisor
//
// u64: [0-5] shift value
//      [6] add indicator
//      [7] shift path
//
// s64: [0-5] shift value
//      [6] add indicator
//      [7] indicates negative divisor
//      magic number of 0 indicates shift path (we ran out of bits!)
//
// In s32 and s64 branchfree modes, the magic number is negated according to
// whether the divisor is negated. In branchfree strategy, it is not negated.

enum {
    LIBDIVIDE_32_SHIFT_MASK = 0x1F,
    LIBDIVIDE_64_SHIFT_MASK = 0x3F,
    LIBDIVIDE_ADD_MARKER = 0x40,
    LIBDIVIDE_U32_SHIFT_PATH = 0x80,
    LIBDIVIDE_U64_SHIFT_PATH = 0x80,
    LIBDIVIDE_S32_SHIFT_PATH = 0x20,
    LIBDIVIDE_NEGATIVE_DIVISOR = 0x80    
};

struct libdivide_u32_t {
    uint32_t magic;
    uint8_t more;
};

struct libdivide_u32_branchfree_t {
    uint32_t magic;
    uint8_t more;
};

#ifndef LIBDIVIDE_API
#define LIBDIVIDE_API static inline
#endif

LIBDIVIDE_API struct libdivide_u32_t libdivide_u32_gen(uint32_t y);

LIBDIVIDE_API struct libdivide_u32_branchfree_t libdivide_u32_branchfree_gen(uint32_t y);
    
LIBDIVIDE_API uint32_t libdivide_u32_do(uint32_t numer, const struct libdivide_u32_t *denom);

LIBDIVIDE_API uint32_t libdivide_u32_branchfree_do(uint32_t numer, const struct libdivide_u32_branchfree_t *denom);
    
LIBDIVIDE_API uint32_t libdivide_u32_recover(const struct libdivide_u32_t *denom);

LIBDIVIDE_API uint32_t libdivide_u32_branchfree_recover(const struct libdivide_u32_branchfree_t *denom);

LIBDIVIDE_API int libdivide_u32_get_algorithm(const struct libdivide_u32_t *denom);
LIBDIVIDE_API uint32_t libdivide_u32_do_alg0(uint32_t numer, const struct libdivide_u32_t *denom);
LIBDIVIDE_API uint32_t libdivide_u32_do_alg1(uint32_t numer, const struct libdivide_u32_t *denom);
LIBDIVIDE_API uint32_t libdivide_u32_do_alg2(uint32_t numer, const struct libdivide_u32_t *denom);
 

//////// Internal Utility Functions

static inline uint32_t libdivide__mullhi_u32(uint32_t x, uint32_t y) {
    uint64_t xl = x, yl = y;
    uint64_t rl = xl * yl;
    return (uint32_t)(rl >> 32);
}
     
static inline int32_t libdivide__count_leading_zeros32(uint32_t val) {
#if __GNUC__ || __has_builtin(__builtin_clz)
    // Fast way to count leading zeros
    return __builtin_clz(val);    
#else
  int32_t result = 0;
  uint32_t hi = 1U << 31;

  while (~val & hi) {
      hi >>= 1;
      result++;
  }
  return result;
#endif
}

static inline int32_t libdivide__count_leading_zeros64(uint64_t val) {
#if __GNUC__ || __has_builtin(__builtin_clzll)
    // Fast way to count leading zeros
    return __builtin_clzll(val);
#else
    uint32_t hi = val >> 32;
    uint32_t lo = val & 0xFFFFFFFF;
    if (hi != 0) return libdivide__count_leading_zeros32(hi);
    return 32 + libdivide__count_leading_zeros32(lo);
#endif
}

// libdivide_64_div_32_to_32: divides a 64 bit uint {u1, u0} by a 32 bit
// uint {v}. The result must fit in 32 bits.
// Returns the quotient directly and the remainder in *r
#if (LIBDIVIDE_IS_i386 || LIBDIVIDE_IS_X86_64) && LIBDIVIDE_GCC_STYLE_ASM
static uint32_t libdivide_64_div_32_to_32(uint32_t u1, uint32_t u0, uint32_t v, uint32_t *r) {
    uint32_t result;
    __asm__("divl %[v]"
            : "=a"(result), "=d"(*r)
            : [v] "r"(v), "a"(u0), "d"(u1)
            );
    return result;
}
#else
static uint32_t libdivide_64_div_32_to_32(uint32_t u1, uint32_t u0, uint32_t v, uint32_t *r) {
    uint64_t n = (((uint64_t)u1) << 32) | u0;
    uint32_t result = (uint32_t)int_divide(n, v);
    *r = (uint32_t)(n - result * (uint64_t)v);
    return result;
}
#endif
    
////////// UINT32

static inline struct libdivide_u32_t libdivide_internal_u32_gen(uint32_t d, int branchfree) {
    // 1 is not supported with branchfree algorithm
    LIBDIVIDE_ASSERT(!branchfree || d != 1);

    struct libdivide_u32_t result;
    const uint32_t floor_log_2_d = 31 - libdivide__count_leading_zeros32(d);
    if ((d & (d - 1)) == 0) {
        // Power of 2
        if (! branchfree) {
            result.magic = 0;
            result.more = floor_log_2_d | LIBDIVIDE_U32_SHIFT_PATH;
        } else {
            // We want a magic number of 2**32 and a shift of floor_log_2_d
            // but one of the shifts is taken up by LIBDIVIDE_ADD_MARKER, so we
            // subtract 1 from the shift
            result.magic = 0;
            result.more = (floor_log_2_d-1) | LIBDIVIDE_ADD_MARKER;
        }
    } else {
        uint8_t more;
        uint32_t rem, proposed_m;
        proposed_m = libdivide_64_div_32_to_32(1U << floor_log_2_d, 0, d, &rem);
        
        LIBDIVIDE_ASSERT(rem > 0 && rem < d);
        const uint32_t e = d - rem;
        
        // This power works if e < 2**floor_log_2_d.
        if (!branchfree && (e < (1U << floor_log_2_d))) {
            // This power works
            more = floor_log_2_d;
        } else {
            // We have to use the general 33-bit algorithm.  We need to compute
            // (2**power) / d. However, we already have (2**(power-1))/d and
            // its remainder.  By doubling both, and then correcting the
            // remainder, we can compute the larger division.
            // don't care about overflow here - in fact, we expect it
            proposed_m += proposed_m;
            const uint32_t twice_rem = rem + rem;
            if (twice_rem >= d || twice_rem < rem) proposed_m += 1;
            more = floor_log_2_d | LIBDIVIDE_ADD_MARKER;
        }
        result.magic = 1 + proposed_m;
        result.more = more;
        // result.more's shift should in general be ceil_log_2_d. But if we
        // used the smaller power, we subtract one from the shift because we're
        // using the smaller power. If we're using the larger power, we
        // subtract one from the shift because it's taken care of by the add
        // indicator. So floor_log_2_d happens to be correct in both cases.
    }
    return result;
}
    
struct libdivide_u32_t libdivide_u32_gen(uint32_t d) {
    return libdivide_internal_u32_gen(d, 0);
}
    
struct libdivide_u32_branchfree_t libdivide_u32_branchfree_gen(uint32_t d) {
    struct libdivide_u32_t tmp = libdivide_internal_u32_gen(d, 1);
    struct libdivide_u32_branchfree_t ret = {tmp.magic, (uint8_t)(tmp.more & LIBDIVIDE_32_SHIFT_MASK)};
    return ret;
}

uint32_t libdivide_u32_do(uint32_t numer, const struct libdivide_u32_t *denom) {
    uint8_t more = denom->more;
    if (more & LIBDIVIDE_U32_SHIFT_PATH) {
        return numer >> (more & LIBDIVIDE_32_SHIFT_MASK);
    }
    else {
        uint32_t q = libdivide__mullhi_u32(denom->magic, numer);
        if (more & LIBDIVIDE_ADD_MARKER) {
            uint32_t t = ((numer - q) >> 1) + q;
            return t >> (more & LIBDIVIDE_32_SHIFT_MASK);
        }
        else {
            return q >> more; // all upper bits are 0 - don't need to mask them off
        }
    }
}

uint32_t libdivide_u32_recover(const struct libdivide_u32_t *denom) {
    uint8_t more = denom->more;
    uint8_t shift = more & LIBDIVIDE_32_SHIFT_MASK;
    if (more & LIBDIVIDE_U32_SHIFT_PATH) {
        return 1U << shift;
    } else if (! (more & LIBDIVIDE_ADD_MARKER)) {
        // We compute q = n/d = n*m / 2^(32 + shift)
        // Therefore we have d = 2^(32 + shift) / m
        // We need to ceil it.
        // We know d is not a power of 2, so m is not a power of 2,
        // so we can just add 1 to the floor
        uint32_t hi_dividend = 1U << shift;
        uint32_t rem_ignored;
        return 1 + libdivide_64_div_32_to_32(hi_dividend, 0, denom->magic, &rem_ignored);
    } else {
        // Here we wish to compute d = 2^(32+shift+1)/(m+2^32).
        // Notice (m + 2^32) is a 33 bit number. Use 64 bit division for now
        // Also note that shift may be as high as 31, so shift + 1 will
        // overflow. So we have to compute it as 2^(32+shift)/(m+2^32), and
        // then double the quotient and remainder.
        // TODO: do something better than 64 bit math
        uint64_t half_n = 1ULL << (32 + shift);
        uint64_t d = (1ULL << 32) | denom->magic;
        // Note that the quotient is guaranteed <= 32 bits, but the remainder
        // may need 33!
        uint32_t half_q = (uint32_t)int_divide(half_n, d);
        uint64_t rem = half_n - half_q * d; // broken
        // We computed 2^(32+shift)/(m+2^32)
        // Need to double it, and then add 1 to the quotient if doubling th
        // remainder would increase the quotient.
        // Note that rem<<1 cannot overflow, since rem < d and d is 33 bits
        uint32_t full_q = half_q + half_q + ((rem<<1) >= d);
        
        // We rounded down in gen unless we're a power of 2 (i.e. in branchfree case)
        // We can detect that by looking at m. If m zero, we're a power of 2
        return full_q + (denom->magic != 0);
    }
}

uint32_t libdivide_u32_branchfree_recover(const struct libdivide_u32_branchfree_t *denom) {
    struct libdivide_u32_t denom_u32 = {denom->magic, (uint8_t)(denom->more | LIBDIVIDE_ADD_MARKER)};
    return libdivide_u32_recover(&denom_u32);
}

int libdivide_u32_get_algorithm(const struct libdivide_u32_t *denom) {
    uint8_t more = denom->more;
    if (more & LIBDIVIDE_U32_SHIFT_PATH) return 0;
    else if (! (more & LIBDIVIDE_ADD_MARKER)) return 1;
    else return 2;
}
 
uint32_t libdivide_u32_do_alg0(uint32_t numer, const struct libdivide_u32_t *denom) {
    return numer >> (denom->more & LIBDIVIDE_32_SHIFT_MASK);
}
 
uint32_t libdivide_u32_do_alg1(uint32_t numer, const struct libdivide_u32_t *denom) {
    uint32_t q = libdivide__mullhi_u32(denom->magic, numer);
    return q >> denom->more;
}    
 
uint32_t libdivide_u32_do_alg2(uint32_t numer, const struct libdivide_u32_t *denom) {
    // denom->add != 0
    uint32_t q = libdivide__mullhi_u32(denom->magic, numer);
    uint32_t t = ((numer - q) >> 1) + q;
    // Note that this mask is typically free. Only the low bits are meaningful
    // to a shift, so compilers can optimize out this AND.
    return t >> (denom->more & LIBDIVIDE_32_SHIFT_MASK);
}

uint32_t libdivide_u32_branchfree_do(uint32_t numer, const struct libdivide_u32_branchfree_t *denom) {
    // same as alg 2
    uint32_t q = libdivide__mullhi_u32(denom->magic, numer);
    uint32_t t = ((numer - q) >> 1) + q;
    return t >> denom->more;
}

CLICK_ENDDECLS
#endif
