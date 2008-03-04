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
inline int ffs_msb(uint32_t x) {
    return (x ? __builtin_clz(x) + 1 : 0);
}
#else
# define NEED_FFS_MSB_UINT32_T 1
int ffs_msb(uint32_t);
#endif

#ifdef HAVE_INT64_TYPES
# if HAVE___BUILTIN_CLZLL && SIZEOF_LONG_LONG == 8 && !HAVE_NO_INTEGER_BUILTINS
/** @overload */
inline int ffs_msb(uint64_t x) {
    return (x ? __builtin_clzll(x) + 1 : 0);
}
# elif HAVE___BUILTIN_CLZL && SIZEOF_LONG == 8 && !HAVE_NO_INTEGER_BUILTINS
inline int ffs_msb(uint64_t x) {
    return (x ? __builtin_clzl(x) + 1 : 0);
}
# elif HAVE___BUILTIN_CLZ && SIZEOF_INT == 8 && !HAVE_NO_INTEGER_BUILTINS
inline int ffs_msb(uint64_t x) {
    return (x ? __builtin_clz(x) + 1 : 0);
}
# else
#  define NEED_FFS_MSB_UINT64_T 1
int ffs_msb(uint64_t);
# endif
#endif

// LSB is bit #1
#if HAVE___BUILTIN_FFS && !HAVE_NO_INTEGER_BUILTINS
/** @brief Return the index of the least significant bit set in @a x.
 * @return 0 if @a x = 0; otherwise the index of first bit set, where the
 * least significant bit is numbered 1.
 */
inline int ffs_lsb(uint32_t x) {
    return __builtin_ffs(x);
}
#elif HAVE_FFS && !HAVE_NO_INTEGER_BUILTINS
inline int ffs_lsb(uint32_t x) {
    return ffs(x);
}
#else
# define NEED_FFS_LSB_UINT32_T 1
int ffs_lsb(uint32_t);
#endif

#ifdef HAVE_INT64_TYPES
# if HAVE___BUILTIN_FFSLL && SIZEOF_LONG_LONG == 8 && !HAVE_NO_INTEGER_BUILTINS
/** @overload */
inline int ffs_lsb(uint64_t x) {
    return __builtin_ffsll(x);
}
# elif HAVE___BUILTIN_FFSL && SIZEOF_LONG == 8 && !HAVE_NO_INTEGER_BUILTINS
inline int ffs_lsb(uint64_t x) {
    return __builtin_ffsl(x);
}
# elif HAVE___BUILTIN_FFS && SIZEOF_INT == 8 && !HAVE_NO_INTEGER_BUILTINS
inline int ffs_lsb(uint64_t x) {
    return __builtin_ffs(x);
}
# else
#  define NEED_FFS_LSB_UINT64_T 1
int ffs_lsb(uint64_t);
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
