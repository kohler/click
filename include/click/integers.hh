// -*- c-basic-offset: 2; related-file-name: "../../lib/integers.cc" -*-
#ifndef CLICK_INTEGERS_HH
#define CLICK_INTEGERS_HH

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
