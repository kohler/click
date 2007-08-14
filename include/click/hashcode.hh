#ifndef CLICK_HASHCODE_HH
#define CLICK_HASHCODE_HH
CLICK_DECLS

template <typename T>
inline size_t
hashcode(const T &x)
{
    return x.hashcode();
}

inline size_t hashcode(char c) {
    return c;
}

inline size_t hashcode(signed char c) {
    return c;
}

inline size_t hashcode(unsigned char c) {
    return c;
}

inline size_t hashcode(short s) {
    return s;
}

inline size_t hashcode(unsigned short us) {
    return us;
}

inline size_t hashcode(int i) {
    return i;
}

inline size_t hashcode(unsigned u) {
    return u;
}

inline size_t hashcode(long l) {
    return l;
}

inline size_t hashcode(unsigned long ul) {
    return ul;
}

#if HAVE_LONG_LONG
inline size_t hashcode(long long ll) {
    return ll;
}

inline size_t hashcode(unsigned long long ull) {
    return ull;
}
#endif

#if HAVE_INT64_TYPES && !HAVE_INT64_IS_LONG && !HAVE_INT64_IS_LONG_LONG
inline size_t hashcode(int64_t q) {
    return q;
}

inline size_t hashcode(uint64_t uq) {
    return uq;
}
#endif

inline size_t hashcode(void *v) {
    return reinterpret_cast<uintptr_t>(v) >> 3;
}

CLICK_ENDDECLS
#endif
