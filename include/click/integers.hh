// -*- c-basic-offset: 4; related-file-name: "../../lib/integers.cc" -*-
#ifndef CLICK_INTEGERS_HH
#define CLICK_INTEGERS_HH
#include <click/glue.hh>
#include <click/type_traits.hh>
#if !HAVE___BUILTIN_FFS && HAVE_FFS && HAVE_STRINGS_H
# include <strings.h>
#endif
CLICK_DECLS

/** @file <click/integers.hh>
 * @brief Functions for manipulating integers.
 */

#if HAVE_INT64_TYPES && !defined(htonq)
# if CLICK_BYTE_ORDER != CLICK_LITTLE_ENDIAN && CLICK_BYTE_ORDER != CLICK_BIG_ENDIAN
inline uint64_t htonq(uint64_t x) __attribute__((error("unknown byte order")));
# endif

/** @brief Return @a x translated from host to network byte order. */
inline uint64_t htonq(uint64_t x) {
# if CLICK_BYTE_ORDER == CLICK_LITTLE_ENDIAN
    uint32_t hi = x >> 32;
    uint32_t lo = x & 0xffffffff;
    return (((uint64_t)htonl(lo)) << 32) | htonl(hi);
# elif CLICK_BYTE_ORDER == CLICK_BIG_ENDIAN
    return x;
# else
    return 0;
# endif
}
#endif

#if HAVE_INT64_TYPES && !defined(ntohq)
# if CLICK_BYTE_ORDER != CLICK_LITTLE_ENDIAN && CLICK_BYTE_ORDER != CLICK_BIG_ENDIAN
inline uint64_t htonq(uint64_t x) __attribute__((error("unknown byte order")));
# endif

/** @brief Return @a x translated from network to host byte order. */
inline uint64_t ntohq(uint64_t x) {
# if CLICK_BYTE_ORDER == CLICK_LITTLE_ENDIAN
    uint32_t hi = x >> 32;
    uint32_t lo = x & 0xffffffff;
    return (((uint64_t)ntohl(lo)) << 32) | ntohl(hi);
# elif CLICK_BYTE_ORDER == CLICK_BIG_ENDIAN
    return x;
# else
    return 0;
# endif
}
#endif

/** @brief Translate @a x to network byte order.
 *
 * Compare htons/htonl/htonq.  host_to_net_order is particularly useful in
 * template functions, where the type to be translated to network byte order
 * is unknown. */
inline unsigned char host_to_net_order(unsigned char x) {
    return x;
}
/** @overload */
inline signed char host_to_net_order(signed char x) {
    return x;
}
/** @overload */
inline char host_to_net_order(char x) {
    return x;
}
/** @overload */
inline short host_to_net_order(short x) {
    return htons(x);
}
/** @overload */
inline unsigned short host_to_net_order(unsigned short x) {
    return htons(x);
}
/** @overload */
inline int host_to_net_order(int x) {
    return htonl(x);
}
/** @overload */
inline unsigned host_to_net_order(unsigned x) {
    return htonl(x);
}
#if SIZEOF_LONG == 4
/** @overload */
inline long host_to_net_order(long x) {
    return htonl(x);
}
/** @overload */
inline unsigned long host_to_net_order(unsigned long x) {
    return htonl(x);
}
#endif
#if HAVE_INT64_TYPES
# if SIZEOF_LONG == 8
/** @overload */
inline long host_to_net_order(long x) {
    return htonq(x);
}
/** @overload */
inline unsigned long host_to_net_order(unsigned long x) {
    return htonq(x);
}
# endif
# if HAVE_LONG_LONG && SIZEOF_LONG_LONG == 8
/** @overload */
inline long long host_to_net_order(long long x) {
    return htonq(x);
}
/** @overload */
inline unsigned long long host_to_net_order(unsigned long long x) {
    return htonq(x);
}
# endif
# if !HAVE_INT64_IS_LONG && !HAVE_INT64_IS_LONG_LONG
/** @overload */
inline int64_t host_to_net_order(int64_t x) {
    return htonq(x);
}
/** @overload */
inline uint64_t host_to_net_order(uint64_t x) {
    return htonq(x);
}
# endif
#endif

/** @brief Translate @a x to host byte order.
 *
 * Compare ntohs/ntohl/ntohq.  net_to_host_order is particularly useful in
 * template functions, where the type to be translated to network byte order
 * is unknown. */
inline unsigned char net_to_host_order(unsigned char x) {
    return x;
}
/** @overload */
inline signed char net_to_host_order(signed char x) {
    return x;
}
/** @overload */
inline char net_to_host_order(char x) {
    return x;
}
/** @overload */
inline short net_to_host_order(short x) {
    return ntohs(x);
}
/** @overload */
inline unsigned short net_to_host_order(unsigned short x) {
    return ntohs(x);
}
/** @overload */
inline int net_to_host_order(int x) {
    return ntohl(x);
}
/** @overload */
inline unsigned net_to_host_order(unsigned x) {
    return ntohl(x);
}
#if SIZEOF_LONG == 4
/** @overload */
inline long net_to_host_order(long x) {
    return ntohl(x);
}
/** @overload */
inline unsigned long net_to_host_order(unsigned long x) {
    return ntohl(x);
}
#endif
#if HAVE_INT64_TYPES
# if SIZEOF_LONG == 8
/** @overload */
inline long net_to_host_order(long x) {
    return ntohq(x);
}
/** @overload */
inline unsigned long net_to_host_order(unsigned long x) {
    return ntohq(x);
}
# endif
# if HAVE_LONG_LONG && SIZEOF_LONG_LONG == 8
/** @overload */
inline long long net_to_host_order(long long x) {
    return ntohq(x);
}
/** @overload */
inline unsigned long long net_to_host_order(unsigned long long x) {
    return ntohq(x);
}
# endif
# if !HAVE_INT64_IS_LONG && !HAVE_INT64_IS_LONG_LONG
/** @overload */
inline int64_t net_to_host_order(int64_t x) {
    return ntohq(x);
}
/** @overload */
inline uint64_t net_to_host_order(uint64_t x) {
    return ntohq(x);
}
# endif
#endif


// MSB is bit #1
#if HAVE___BUILTIN_CLZ && !HAVE_NO_INTEGER_BUILTINS
/** @brief Return the index of the most significant bit set in @a x.
 * @return 0 if @a x = 0; otherwise the index of first bit set, where the
 * most significant bit is numbered 1.
 */
inline int ffs_msb(unsigned x) {
    return (x ? __builtin_clz(x) + 1 : 0);
}
#else
# define NEED_FFS_MSB_UNSIGNED 1
/** @overload */
int ffs_msb(unsigned x);
#endif

#if HAVE___BUILTIN_CLZL && !HAVE_NO_INTEGER_BUILTINS
/** @overload */
inline int ffs_msb(unsigned long x) {
    return (x ? __builtin_clzl(x) + 1 : 0);
}
#elif SIZEOF_INT == SIZEOF_LONG
/** @overload */
inline int ffs_msb(unsigned long x) {
    return ffs_msb(static_cast<unsigned>(x));
}
#else
# define NEED_FFS_MSB_UNSIGNED_LONG 1
/** @overload */
int ffs_msb(unsigned long x);
#endif

#if HAVE_LONG_LONG && HAVE___BUILTIN_CLZLL && !HAVE_NO_INTEGER_BUILTINS
/** @overload */
inline int ffs_msb(unsigned long long x) {
    return (x ? __builtin_clzll(x) + 1 : 0);
}
#elif HAVE_LONG_LONG && SIZEOF_LONG == SIZEOF_LONG_LONG
/** @overload */
inline int ffs_msb(unsigned long long x) {
    return ffs_msb(static_cast<unsigned long>(x));
}
#elif HAVE_LONG_LONG
# define NEED_FFS_MSB_UNSIGNED_LONG_LONG 1
/** @overload */
int ffs_msb(unsigned long long x);
#endif

#if HAVE_INT64_TYPES && !HAVE_INT64_IS_LONG && !HAVE_INT64_IS_LONG_LONG
# if SIZEOF_LONG >= 8
/** @overload */
inline int ffs_msb(uint64_t x) {
    return ffs_msb(static_cast<unsigned long>(x));
}
# elif HAVE_LONG_LONG && SIZEOF_LONG_LONG >= 8
/** @overload */
inline int ffs_msb(uint64_t x) {
    return ffs_msb(static_cast<unsigned long long>(x));
}
# else
#  define NEED_FFS_MSB_UINT64_T 1
/** @overload */
int ffs_msb(uint64_t x);
# endif
#endif


// LSB is bit #1
#if HAVE___BUILTIN_FFS && !HAVE_NO_INTEGER_BUILTINS
/** @brief Return the index of the least significant bit set in @a x.
 * @return 0 if @a x = 0; otherwise the index of first bit set, where the
 * least significant bit is numbered 1.
 */
inline int ffs_lsb(unsigned x) {
    return __builtin_ffs(x);
}
#elif HAVE_FFS && !HAVE_NO_INTEGER_BUILTINS
/** overload */
inline int ffs_lsb(unsigned x) {
    return ffs(x);
}
#else
# define NEED_FFS_LSB_UNSIGNED 1
/** @overload */
int ffs_lsb(unsigned x);
#endif

#if HAVE___BUILTIN_FFSL && !HAVE_NO_INTEGER_BUILTINS
/** @overload */
inline int ffs_lsb(unsigned long x) {
    return __builtin_ffsl(x);
}
#elif SIZEOF_INT == SIZEOF_LONG
/** @overload */
inline int ffs_lsb(unsigned long x) {
    return ffs_lsb(static_cast<unsigned>(x));
}
#else
# define NEED_FFS_LSB_UNSIGNED_LONG 1
/** @overload */
int ffs_lsb(unsigned long x);
#endif

#if HAVE_LONG_LONG && HAVE___BUILTIN_FFSLL && !HAVE_NO_INTEGER_BUILTINS
/** @overload */
inline int ffs_lsb(unsigned long long x) {
    return __builtin_ffsll(x);
}
#elif HAVE_LONG_LONG && SIZEOF_LONG == SIZEOF_LONG_LONG
/** @overload */
inline int ffs_lsb(unsigned long long x) {
    return ffs_lsb(static_cast<unsigned long>(x));
}
#elif HAVE_LONG_LONG
# define NEED_FFS_LSB_UNSIGNED_LONG_LONG 1
/** @overload */
int ffs_lsb(unsigned long long x);
#endif

#if HAVE_INT64_TYPES && !HAVE_INT64_IS_LONG && !HAVE_INT64_IS_LONG_LONG
# if SIZEOF_LONG >= 8
/** @overload */
inline int ffs_lsb(uint64_t x) {
    return ffs_lsb(static_cast<unsigned long>(x));
}
# elif HAVE_LONG_LONG && SIZEOF_LONG_LONG >= 8
/** @overload */
inline int ffs_lsb(uint64_t x) {
    return ffs_lsb(static_cast<unsigned long long>(x));
}
# else
#  define NEED_FFS_LSB_UINT64_T 1
/** @overload */
int ffs_lsb(uint64_t x);
# endif
#endif


/** @brief Return the integer approximation of @a x's square root.
 * @return The integer @a y where @a y*@a y <= @a x, but
 * (@a y+1)*(@a y+1) > @a x.
 */
uint32_t int_sqrt(uint32_t x);

#if HAVE_INT64_TYPES && HAVE_INT64_DIVIDE
/** @overload */
uint64_t int_sqrt(uint64_t x);
#endif


/** @brief Return @a a / @a b. */
inline uint32_t int_divide(uint32_t a, uint32_t b) {
    return a / b;
}

/** @overload */
inline int32_t int_divide(int32_t a, uint32_t b) {
    return a / b;
}

#if HAVE_INT64_TYPES
/** @overload */
inline uint64_t int_divide(uint64_t a, uint32_t b) {
# if CLICK_LINUXMODULE && BITS_PER_LONG < 64
    do_div(a, b);
    return a;
# else
    return a / b;
# endif
}

/** @overload */
inline int64_t int_divide(int64_t a, uint32_t b) {
# if CLICK_LINUXMODULE && BITS_PER_LONG < 64
    if (unlikely(a < 0)) {
        uint64_t a_abs = -(a + 1);
        do_div(a_abs, b);
        return (int64_t) -a_abs - 1;
    } else {
        uint64_t &a_unsigned = reinterpret_cast<uint64_t &>(a);
        do_div(a_unsigned, b);
        return a_unsigned;
    }
# else
    return a / b;
# endif
}


/** @brief Multiply @a a * @a b, placing the low-order bits of the result in @a xlow
    and the high-order bits in @a xhigh. */
template<typename T>
void int_multiply(T a, T b, T &xlow, T &xhigh)
{
    typedef fast_half_integer<T> fasthalf;
    typedef typename fasthalf::half_type half_type;

    half_type al = fasthalf::low(a), ah = fasthalf::high(a),
        bl = fasthalf::low(b), bh = fasthalf::high(b);

    T r0 = T(al) * bl;
    T r3 = T(ah) * bh;
    T r1 = T(ah) * bl;
    T r2 = T(al) * bh + fasthalf::high(r0) + r1;
    if (r2 < r1)
        r3 += fasthalf::half_value;

    xhigh = r3 + fasthalf::high(r2);
    xlow = (r2 << fasthalf::half_bits) + fasthalf::low(r0);
}

template<typename T>
struct has_fast_int_multiply : public false_type {
    enum { check_t_integral = integer_traits<T>::is_signed };
};

#if defined(__i386__) || defined(__x86_64__)
inline void int_multiply(unsigned a, unsigned b, unsigned &xlow, unsigned &xhigh)
{
    __asm__("mul %2" : "=a" (xlow), "=d" (xhigh) : "r" (a), "a" (b) : "cc");
}
template<> struct has_fast_int_multiply<unsigned> : public true_type {};

# if SIZEOF_LONG == 4 || (defined(__x86_64__) && SIZEOF_LONG == 8)
inline void int_multiply(unsigned long a, unsigned long b, unsigned long &xlow, unsigned long &xhigh)
{
    __asm__("mul %2" : "=a" (xlow), "=d" (xhigh) : "r" (a), "a" (b) : "cc");
}
template<> struct has_fast_int_multiply<unsigned long> : public true_type {};
# endif

# if defined(__x86_64__) && SIZEOF_LONG_LONG == 8
inline void int_multiply(unsigned long long a, unsigned long long b, unsigned long long &xlow, unsigned long long &xhigh)
{
    __asm__("mul %2" : "=a" (xlow), "=d" (xhigh) : "r" (a), "a" (b) : "cc");
}
template<> struct has_fast_int_multiply<unsigned long long> : public true_type {};
# endif
#endif


/** @brief Divide @a a / @a b, placing quotient in @a quot and returning remainder. */
inline uint32_t int_divide(uint32_t a, uint32_t b, uint32_t &quot) {
    quot = a / b;
    return a - quot * b;
}

/** @overload */
inline int32_t int_divide(int32_t a, uint32_t b, int32_t &quot) {
    if (unlikely(a < 0))
        quot = -(-(a + 1) / b) - 1;
    else
        quot = a / b;
    return a - quot * b;
}

/** @overload */
inline uint32_t int_divide(uint64_t a, uint32_t b, uint64_t &quot) {
# if CLICK_LINUXMODULE && BITS_PER_LONG < 64
    uint32_t rem = do_div(a, b);
    quot = a;
    return rem;
# else
    quot = a / b;
    return a - quot * b;
# endif
}

/** @overload */
inline uint32_t int_divide(int64_t a, uint32_t b, int64_t &quot) {
# if CLICK_LINUXMODULE && BITS_PER_LONG < 64
    if (unlikely(a < 0)) {
        uint64_t a_abs = -(a + 1);
        uint32_t rem = do_div(a_abs, b);
        quot = (int64_t) -a_abs - 1;
        return rem ? b - rem : 0;
    } else {
        uint64_t &a_unsigned = reinterpret_cast<uint64_t &>(a);
        uint32_t rem = do_div(a_unsigned, b);
        quot = a_unsigned;
        return rem;
    }
# else
    // This arithmetic is about twice as fast on my laptop as the
    // alternative "div = a / b;
    //          rem = a - (value_type) div * b;
    //          if (rem < 0) div--, rem += b;",
    // and 3-4x faster than "div = a / b;
    //                   rem = a % b;
    //                   if (rem < 0) div--, rem += b;".
    if (unlikely(a < 0))
        quot = -(-(a + 1) / b) - 1;
    else
        quot = a / b;
    return a - quot * b;
# endif
}
#endif

CLICK_ENDDECLS
#endif
