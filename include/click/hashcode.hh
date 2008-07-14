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

template <typename T>
inline size_t hashcode(const T &x) {
    return x.hashcode();
}

template <>
inline size_t hashcode(const char &c) {
    return c;
}

template <>
inline size_t hashcode(const signed char &c) {
    return c;
}

template <>
inline size_t hashcode(const unsigned char &c) {
    return c;
}

template <>
inline size_t hashcode(const short &s) {
    return s;
}

template <>
inline size_t hashcode(const unsigned short &us) {
    return us;
}

template <>
inline size_t hashcode(const int &i) {
    return i;
}

template <>
inline size_t hashcode(const unsigned &u) {
    return u;
}

template <>
inline size_t hashcode(const long &l) {
    return l;
}

template <>
inline size_t hashcode(const unsigned long &ul) {
    return ul;
}

#if HAVE_LONG_LONG
template <>
inline size_t hashcode(const long long &ll) {
    return (ll >> 32) ^ ll;
}

template <>
inline size_t hashcode(const unsigned long long &ull) {
    return (ull >> 32) ^ ull;
}
#endif

#if HAVE_INT64_TYPES && !HAVE_INT64_IS_LONG && !HAVE_INT64_IS_LONG_LONG
template <>
inline size_t hashcode(const int64_t &q) {
    return (q >> 32) ^ q;
}

template <>
inline size_t hashcode(const uint64_t &uq) {
    return (uq >> 32) ^ uq;
}
#endif

template <typename T>
inline size_t hashcode(T * const &v) {
    return reinterpret_cast<uintptr_t>(v) >> 3;
}

template <typename T>
inline typename T::key_const_reference hashkey(const T &x) {
    return x.hashkey();
}

CLICK_ENDDECLS
#endif
