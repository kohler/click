#ifndef CLICK_TYPE_TRAITS_HH
#define CLICK_TYPE_TRAITS_HH

template <typename T, T val>
struct integral_constant {
    typedef integral_constant<T, val> type;
    typedef T value_type;
    static const T value = val;
};
typedef integral_constant<bool, true> true_type;
typedef integral_constant<bool, false> false_type;

template <typename T> struct has_trivial_copy : public false_type {};
template <> struct has_trivial_copy<unsigned char> : public true_type {};
template <> struct has_trivial_copy<signed char> : public true_type {};
template <> struct has_trivial_copy<char> : public true_type {};
template <> struct has_trivial_copy<unsigned short> : public true_type {};
template <> struct has_trivial_copy<short> : public true_type {};
template <> struct has_trivial_copy<unsigned> : public true_type {};
template <> struct has_trivial_copy<int> : public true_type {};
template <> struct has_trivial_copy<unsigned long> : public true_type {};
template <> struct has_trivial_copy<long> : public true_type {};
#if HAVE_LONG_LONG
template <> struct has_trivial_copy<unsigned long long> : public true_type {};
template <> struct has_trivial_copy<long long> : public true_type {};
#endif
template <typename T> struct has_trivial_copy<T *> : public true_type {};


#if HAVE_INT64_TYPES && (!HAVE_LONG_LONG || SIZEOF_LONG_LONG < 8)
typedef int64_t click_int_large_t;
typedef uint64_t click_uint_large_t;
#elif HAVE_LONG_LONG
typedef long long click_int_large_t;
typedef unsigned long long click_uint_large_t;
#else
typedef long click_int_large_t;
typedef unsigned long click_uint_large_t;
#endif


/** @class numeric_traits
  @brief Numeric traits template.

  The numeric_traits template defines constants and type definitions related
  to integers.  Where T is an integer, numeric_traits<T> defines:

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

  If T is <em>not</em> an integer, numeric_traits<T> defines:

  <dl>
  <dt>const bool is_numeric</dt>
  <dd>False.</dd>
  <dt>const bool is_integer</dt>
  <dd>False.</dd>
  </dl> */
template <typename T>
struct numeric_traits {
    static const bool is_numeric = false;
    static const bool is_integer = false;
};

template <>
struct numeric_traits<unsigned char> {
    static const bool is_numeric = true;
    static const bool is_integer = true;
    static const unsigned char const_min = 0;
    static const unsigned char const_max = ~const_min;
    static const bool is_signed = false;
    typedef signed char signed_type;
    typedef unsigned char unsigned_type;
};

template <>
struct numeric_traits<signed char> {
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
struct numeric_traits<char> : public numeric_traits<unsigned char> {
    static const char const_min = 0;
    static const char const_max = ~const_min;
};
#else
template <>
struct numeric_traits<char> : public numeric_traits<signed char> {
    static const char const_min = -128;
    static const char const_max = 127;
};
#endif

template <>
struct numeric_traits<unsigned short> {
    static const bool is_numeric = true;
    static const bool is_integer = true;
    static const unsigned short const_min = 0;
    static const unsigned short const_max = ~const_min;
    static const bool is_signed = false;
    typedef short signed_type;
    typedef unsigned short unsigned_type;
};

template <>
struct numeric_traits<short> {
    static const bool is_numeric = true;
    static const bool is_integer = true;
    static const short const_min = -32768;
    static const short const_max = 32767;
    static const bool is_signed = true;
    typedef short signed_type;
    typedef unsigned short unsigned_type;
};

template <>
struct numeric_traits<unsigned int> {
    static const bool is_numeric = true;
    static const bool is_integer = true;
    static const unsigned int const_min = 0;
    static const unsigned int const_max = ~const_min;
    static const bool is_signed = false;
    typedef int signed_type;
    typedef unsigned int unsigned_type;
};

template <>
struct numeric_traits<int> {
    static const bool is_numeric = true;
    static const bool is_integer = true;
    static const int const_min = 1 << (8*SIZEOF_INT - 1);
    static const int const_max = (unsigned) const_min - 1;
    static const bool is_signed = true;
    typedef int signed_type;
    typedef unsigned int unsigned_type;
};

template <>
struct numeric_traits<unsigned long> {
    static const bool is_numeric = true;
    static const bool is_integer = true;
    static const unsigned long const_min = 0;
    static const unsigned long const_max = ~const_min;
    static const bool is_signed = false;
    typedef long signed_type;
    typedef unsigned long unsigned_type;
};

template <>
struct numeric_traits<long> {
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
struct numeric_traits<unsigned long long> {
    static const bool is_numeric = true;
    static const bool is_integer = true;
    static const unsigned long long const_min = 0;
    static const unsigned long long const_max = ~const_min;
    static const bool is_signed = false;
    typedef long long signed_type;
    typedef unsigned long long unsigned_type;
};

template <>
struct numeric_traits<long long> {
    static const bool is_numeric = true;
    static const bool is_integer = true;
    static const long long const_min = (long long) 1 << (8*SIZEOF_LONG_LONG - 1);
    static const long long const_max = (unsigned long long) const_min - 1;
    static const bool is_signed = true;
    typedef long long signed_type;
    typedef unsigned long long unsigned_type;
};
#endif

#if HAVE_INT64_TYPES && !HAVE_INT64_IS_LONG && !HAVE_INT64_IS_LONG_LONG
template <>
struct numeric_traits<uint64_t> {
    static const bool is_numeric = true;
    static const bool is_integer = true;
    static const uint64_t const_min = 0;
    static const uint64_t const_max = ~const_min;
    static const bool is_signed = false;
    typedef int64_t signed_type;
    typedef uint64_t unsigned_type;
};

template <>
struct numeric_traits<int64_t> {
    static const bool is_numeric = true;
    static const bool is_integer = true;
    static const int64_t const_min = (int64_t) 1 << 63;
    static const int64_t const_max = (uint64_t) const_min - 1;
    static const bool is_signed = true;
    typedef int64_t signed_type;
    typedef uint64_t unsigned_type;
};
#endif

#endif
