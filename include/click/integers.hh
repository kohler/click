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
#if HAVE___BUILTIN_FFS && SIZEOF_INT == 4
inline int ffs_msb(uint32_t u) {
    int ffs = __builtin_ffs(u);
    return (ffs ? 33 - ffs : 0);
}
#else
int ffs_msb(uint32_t);
#endif
#ifdef HAVE_INT64_TYPES
# if HAVE___BUILTIN_FFSLL && SIZEOF_LONG_LONG == 8
inline int ffs_msb(uint64_t x) {
    int ffs = __builtin_ffsll(x);
    return (ffs ? 65 - ffs : 0);
}
# elif HAVE___BUILTIN_FFSL && SIZEOF_LONG == 8
inline int ffs_msb(uint64_t x) {
    int ffs = __builtin_ffsl(x);
    return (ffs ? 65 - ffs : 0);
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
