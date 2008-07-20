#ifndef CLICK_HASHCODE_HH
#define CLICK_HASHCODE_HH
CLICK_DECLS

// Notes about the hashcode template: On GCC 4.3.0, "template <>" is required
// on the specializations or they aren't used.  Just plain overloaded
// functions aren't used.  The specializations must be e.g. "const char &",
// not "char", or GCC complains about a specialization not matching the
// general template.  The main template takes a const reference for two
// reasons.  First, providing both "hashcode_t hashcode(T)" and "hashcode_t
// hashcode(const T&)" leads to ambiguity errors.  Second, providing only
// "hashcode_t hashcode(T)" is slower by looks like 8% when T is a String,
// because of copy constructors; for types with more expensive non-default
// copy constructors this would probably be worse.

typedef size_t hashcode_t;	///< Typical type for a hashcode() value.

template <typename T>
inline hashcode_t hashcode(T const &x) {
    return x.hashcode();
}

template <>
inline hashcode_t hashcode(char const &x) {
    return x;
}

template <>
inline hashcode_t hashcode(signed char const &x) {
    return x;
}

template <>
inline hashcode_t hashcode(unsigned char const &x) {
    return x;
}

template <>
inline hashcode_t hashcode(short const &x) {
    return x;
}

template <>
inline hashcode_t hashcode(unsigned short const &x) {
    return x;
}

template <>
inline hashcode_t hashcode(int const &x) {
    return x;
}

template <>
inline hashcode_t hashcode(unsigned const &x) {
    return x;
}

template <>
inline hashcode_t hashcode(long const &x) {
    return x;
}

template <>
inline hashcode_t hashcode(unsigned long const &x) {
    return x;
}

#if HAVE_LONG_LONG
template <>
inline hashcode_t hashcode(long long const &x) {
    return (x >> 32) ^ x;
}

template <>
inline hashcode_t hashcode(unsigned long long const &x) {
    return (x >> 32) ^ x;
}
#endif

#if HAVE_INT64_TYPES && !HAVE_INT64_IS_LONG && !HAVE_INT64_IS_LONG_LONG
template <>
inline hashcode_t hashcode(int64_t const &x) {
    return (x >> 32) ^ x;
}

template <>
inline hashcode_t hashcode(uint64_t const &x) {
    return (x >> 32) ^ x;
}
#endif

template <typename T>
inline hashcode_t hashcode(T * const &x) {
    return reinterpret_cast<uintptr_t>(x) >> 3;
}

template <typename T>
inline typename T::key_const_reference hashkey(const T &x) {
    return x.hashkey();
}

CLICK_ENDDECLS
#endif
