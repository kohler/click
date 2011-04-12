// -*- c-basic-offset: 4; related-file-name: "../../lib/integers.cc" -*-
#ifndef CLICK_INTEGERS_HH
#define CLICK_INTEGERS_HH
#include <click/glue.hh>
#if !HAVE___BUILTIN_FFS && HAVE_FFS && HAVE_STRINGS_H
# include <strings.h>
#endif
CLICK_DECLS

/** @file <click/integers.hh>
 * @brief Functions and types for manipulating integers.
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

/** @class NumericTraits
  @brief Numeric traits template.

  The NumericTraits template defines constants and type definitions related
  to integers.  Where T is an integer, NumericTraits<T> defines:

  <dl>
  <dt>const T const_min</dt>
  <dd>The minimum value defined for the type.</dd>
  <dt>const T const_max</dt>
  <dd>The maximum value available for the type.</dd>
  <dt>const bool is_numeric</dt>
  <dd>True.</dd>
  <dt>const bool is_integer</dt>
  <dd>True.</dd>
  <dt>const bool is_signed</dt>
  <dd>True iff the type is signed.</dd>
  <dt>signed_type (typedef)</dt>
  <dd>Signed version of the type.</dd>
  <dt>unsigned_type (typedef)</dt>
  <dd>Unsigned version of the type.</dd>
  </dl>

  If T is <em>not</em> an integer, NumericTraits<T> defines:

  <dl>
  <dt>const bool is_numeric</dt>
  <dd>False.</dd>
  <dt>const bool is_integer</dt>
  <dd>False.</dd>
  </dl> */
template <typename T>
struct NumericTraits {
    static const bool is_numeric = false;
    static const bool is_integer = false;
};

template <>
struct NumericTraits<unsigned char> {
    static const bool is_numeric = true;
    static const bool is_integer = true;
    static const unsigned char const_min = 0;
    static const unsigned char const_max = ~const_min;
    static const bool is_signed = false;
    typedef signed char signed_type;
    typedef unsigned char unsigned_type;
};

template <>
struct NumericTraits<signed char> {
    static const bool is_numeric = true;
    static const bool is_integer = true;
    static const signed char const_min = -128;
    static const signed char const_max = 127;
    static const bool is_signed = true;
    typedef signed char signed_type;
    typedef unsigned char unsigned_type;
};

#if __CHAR_UNSIGNED__
template <>
struct NumericTraits<char> : public NumericTraits<unsigned char> {
    static const char const_min = 0;
    static const char const_max = ~const_min;
};
#else
template <>
struct NumericTraits<char> : public NumericTraits<signed char> {
    static const char const_min = -128;
    static const char const_max = 127;
};
#endif

template <>
struct NumericTraits<unsigned short> {
    static const bool is_numeric = true;
    static const bool is_integer = true;
    static const unsigned short const_min = 0;
    static const unsigned short const_max = ~const_min;
    static const bool is_signed = false;
    typedef short signed_type;
    typedef unsigned short unsigned_type;
};

template <>
struct NumericTraits<short> {
    static const bool is_numeric = true;
    static const bool is_integer = true;
    static const short const_min = -32768;
    static const short const_max = 32767;
    static const bool is_signed = true;
    typedef short signed_type;
    typedef unsigned short unsigned_type;
};

template <>
struct NumericTraits<unsigned int> {
    static const bool is_numeric = true;
    static const bool is_integer = true;
    static const unsigned int const_min = 0;
    static const unsigned int const_max = ~const_min;
    static const bool is_signed = false;
    typedef int signed_type;
    typedef unsigned int unsigned_type;
};

template <>
struct NumericTraits<int> {
    static const bool is_numeric = true;
    static const bool is_integer = true;
    static const int const_min = 1 << (8*SIZEOF_INT - 1);
    static const int const_max = (unsigned) const_min - 1;
    static const bool is_signed = true;
    typedef int signed_type;
    typedef unsigned int unsigned_type;
};

template <>
struct NumericTraits<unsigned long> {
    static const bool is_numeric = true;
    static const bool is_integer = true;
    static const unsigned long const_min = 0;
    static const unsigned long const_max = ~const_min;
    static const bool is_signed = false;
    typedef long signed_type;
    typedef unsigned long unsigned_type;
};

template <>
struct NumericTraits<long> {
    static const bool is_numeric = true;
    static const bool is_integer = true;
    static const long const_min = (long) 1 << (8*SIZEOF_LONG - 1);
    static const long const_max = (unsigned long) const_min - 1;
    static const bool is_signed = true;
    typedef long signed_type;
    typedef unsigned long unsigned_type;
};

#if HAVE_LONG_LONG
template <>
struct NumericTraits<unsigned long long> {
    static const bool is_numeric = true;
    static const bool is_integer = true;
    static const unsigned long long const_min = 0;
    static const unsigned long long const_max = ~const_min;
    static const bool is_signed = false;
    typedef long long signed_type;
    typedef unsigned long long unsigned_type;
};

template <>
struct NumericTraits<long long> {
    static const bool is_numeric = true;
    static const bool is_integer = true;
    static const long long const_min = (long long) 1 << (8*SIZEOF_LONG_LONG - 1);
    static const long long const_max = (unsigned long long) const_min - 1;
    static const bool is_signed = true;
    typedef long long signed_type;
    typedef unsigned long long unsigned_type;
};
#endif


#if HAVE_INT64_TYPES

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

CLICK_ENDDECLS
#endif
