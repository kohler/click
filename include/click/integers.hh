// -*- c-basic-offset: 4; related-file-name: "../../lib/integers.cc" -*-
#ifndef CLICK_INTEGERS_HH
#define CLICK_INTEGERS_HH
CLICK_DECLS

#ifdef HAVE_INT64_TYPES

inline uint64_t htonq(uint64_t x) {
    uint32_t hi = x >> 32;
    uint32_t lo = x & 0xffffffff;
    return (((uint64_t)htonl(lo)) << 32) | htonl(hi);
}

inline uint64_t ntohq(uint64_t x) {
    uint32_t hi = x >> 32;
    uint32_t lo = x & 0xffffffff;
    return (((uint64_t)ntohl(lo)) << 32) | ntohl(hi);
}

#endif

// MSB is bit #1
#if HAVE___BUILTIN_CLZ && SIZEOF_INT == 4
inline int ffs_msb(uint32_t x) {
    return (x ? __builtin_clz(x) + 1 : 0);
}
#else
int ffs_msb(uint32_t);
#endif
#ifdef HAVE_INT64_TYPES
# if HAVE___BUILTIN_CLZLL && SIZEOF_LONG_LONG == 8
inline int ffs_msb(uint64_t x) {
    return (x ? __builtin_clzll(x) + 1 : 0);
}
# elif HAVE___BUILTIN_CLZL && SIZEOF_LONG == 8
inline int ffs_msb(uint64_t x) {
    return (x ? __builtin_clzl(x) + 1 : 0);
}
# else
int ffs_msb(uint64_t);
# endif
#endif

uint32_t int_sqrt(uint32_t);
#if HAVE_INT64_TYPES && !CLICK_LINUXMODULE
uint64_t int_sqrt(uint64_t);
#endif

CLICK_ENDDECLS
#endif
