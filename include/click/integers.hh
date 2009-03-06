// -*- c-basic-offset: 4; related-file-name: "../../lib/integers.cc" -*-
#ifndef CLICK_INTEGERS_HH
#define CLICK_INTEGERS_HH
#if !HAVE___BUILTIN_FFS && HAVE_FFS && HAVE_STRINGS_H
# include <strings.h>
#endif
CLICK_DECLS

/** @file <click/integers.hh>
 * @brief Functions for manipulating integers.
 */

#if HAVE_INT64_TYPES && (!HAVE_LONG_LONG || SIZEOF_LONG_LONG <= 8)
typedef int64_t click_int_large_t;
typedef uint64_t click_uint_large_t;
#elif HAVE_LONG_LONG
typedef long long click_int_large_t;
typedef unsigned long long click_uint_large_t;
#else
typedef long click_int_large_t;
typedef unsigned long click_uint_large_t;
#endif

#ifdef HAVE_INT64_TYPES

/** @brief Return @a x translated from host to network byte order. */
inline uint64_t htonq(uint64_t x) {
    uint32_t hi = x >> 32;
    uint32_t lo = x & 0xffffffff;
    return (((uint64_t)htonl(lo)) << 32) | htonl(hi);
}

/** @brief Return @a x translated from network to host byte order. */
inline uint64_t ntohq(uint64_t x) {
    uint32_t hi = x >> 32;
    uint32_t lo = x & 0xffffffff;
    return (((uint64_t)ntohl(lo)) << 32) | ntohl(hi);
}

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

CLICK_ENDDECLS
#endif
