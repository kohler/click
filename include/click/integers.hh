#ifndef INTEGERS_HH
#define INTEGERS_HH

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

class net_uint16_t {
  uint16_t _d;
 public:
  net_uint16_t(uint16_t x)		{ _d = htons(x); }
  operator uint16_t() const		{ return ntohs(_d); }
  uint16_t v() const			{ return ntohs(_d); }
  net_uint16_t &operator=(uint16_t x) {
    _d = htons(x); return *this;
  }
};

class net_uint32_t {
  uint32_t _d;
 public:
  net_uint32_t(uint32_t x)		{ _d = htonl(x); }
  operator uint32_t() const		{ return ntohl(_d); }
  uint32_t v() const			{ return ntohl(_d); }
  net_uint32_t &operator=(uint32_t x) {
    _d = htonl(x); return *this;
  }
};

class net_uint64_t {
  uint64_t _d;
 public:
  net_uint64_t(uint64_t x)		{ _d = htonq(x); }
  operator uint64_t() const		{ return ntohq(_d); }
  uint64_t v() const			{ return ntohq(_d); }
  net_uint64_t &operator=(uint64_t x) {
    _d = htonq(x); return *this;
  }
};

class una_net_uint16_t {
  unsigned char _d[2];
 public:
  una_net_uint16_t(uint16_t x)		{ *((u_int16_t *)_d) = htons(x); }
  operator uint16_t() const		{ return ntohs(*((uint16_t *)_d)); }
  uint16_t v() const			{ return ntohs(*((uint16_t *)_d)); }
  una_net_uint16_t &operator=(uint16_t x) {
    *((uint16_t *)_d) = htons(x); return *this;
  }
};

class una_net_uint32_t {
  unsigned char _d[4];
 public:
  una_net_uint32_t(uint32_t x)		{ *((u_int32_t *)_d) = htonl(x); }
  operator uint32_t() const		{ return ntohl(*((uint32_t *)_d)); }
  uint32_t v() const			{ return ntohl(*((uint32_t *)_d)); }
  una_net_uint32_t &operator=(uint32_t x) {
    *((uint32_t *)_d) = htonl(x); return *this;
  }
};

class una_net_uint64_t {
  unsigned char _d[8];
 public:
  una_net_uint64_t(uint64_t x)		{ *((u_int64_t *)_d) = htonq(x); }
  operator uint64_t() const		{ return ntohq(*((uint64_t *)_d)); }
  uint64_t v() const			{ return ntohq(*((uint64_t *)_d)); }
  una_net_uint64_t &operator=(uint64_t x) {
    *((uint64_t *)_d) = htonq(x); return *this;
  }
};

#endif
