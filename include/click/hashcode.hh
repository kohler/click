#ifndef CLICK_HASHCODE_HH
#define CLICK_HASHCODE_HH
CLICK_DECLS

// Notes about the hashcode template: On GCC 4.3.0, "template <>" is required
// on the specializations or they aren't used.  Just plain overloaded
// functions aren't used.  The specializations must be e.g. "const char &",
// not "char", or GCC complains about a specialization not matching the
// general template.  The main template takes a const reference for two
// reasons.  First, providing both "size_t hashcode(T)" and "size_t
// hashcode(const T&)" leads to ambiguity errors.  Second, providing only
// "size_t hashcode(T)" is slower by looks like 8% when T is a String, because
// of copy constructors; for types with more expensive non-default copy
// constructors this would probably be worse.

typedef size_t hashcode_t;	///< Typical type for a hashcode() value.

template <typename T>
inline hashcode_t hashcode(const T &x) {
    return x.hashcode();
}

template <>
inline hashcode_t hashcode(const char &x) {
    return x;
}

template <>
inline hashcode_t hashcode(const signed char &x) {
    return x;
}

template <>
inline hashcode_t hashcode(const unsigned char &x) {
    return x;
}

template <>
inline hashcode_t hashcode(const short &x) {
    return x;
}

template <>
inline hashcode_t hashcode(const unsigned short &x) {
    return x;
}

template <>
inline hashcode_t hashcode(const int &x) {
    return x;
}

template <>
inline hashcode_t hashcode(const unsigned &x) {
    return x;
}

template <>
inline hashcode_t hashcode(const long &x) {
    return x;
}

template <>
inline hashcode_t hashcode(const unsigned long &x) {
    return x;
}

#if HAVE_LONG_LONG
template <>
inline hashcode_t hashcode(const long long &x) {
    return (x >> 32) ^ x;
}

template <>
inline hashcode_t hashcode(const unsigned long long &x) {
    return (x >> 32) ^ x;
}
#endif

#if HAVE_INT64_TYPES && !HAVE_INT64_IS_LONG && !HAVE_INT64_IS_LONG_LONG
template <>
inline hashcode_t hashcode(const int64_t &x) {
    return (x >> 32) ^ x;
}

template <>
inline hashcode_t hashcode(const uint64_t &x) {
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
