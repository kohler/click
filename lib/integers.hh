#ifndef INTEGERS_HH
#define INTEGERS_HH

#ifndef __KERNEL__
typedef unsigned char u_int8_t;
typedef unsigned short u_int16_t;
typedef unsigned int u_int32_t;
typedef unsigned long long u_int64_t;
#endif

inline u_int64_t htonq(u_int64_t x) {
  u_int32_t hi = x >> 32;
  u_int32_t lo = x & 0xffffffff;
  return (((u_int64_t)htonl(lo)) << 32) | htonl(hi);
}

inline u_int64_t ntohq(u_int64_t x) {
  u_int32_t hi = x >> 32;
  u_int32_t lo = x & 0xffffffff;
  return (((u_int64_t)ntohl(lo)) << 32) | ntohl(hi);
}

class net_u_int16_t {
  u_int16_t _d;
 public:
  net_u_int16_t(u_int16_t x)		{ _d = htons(x); }
  operator u_int16_t() const		{ return ntohs(_d); }
  u_int16_t v() const			{ return ntohs(_d); }
  net_u_int16_t &operator=(u_int16_t x) {
    _d = htons(x); return *this;
  }
};

class net_u_int32_t {
  u_int32_t _d;
 public:
  net_u_int32_t(u_int32_t x)		{ _d = htonl(x); }
  operator u_int32_t() const		{ return ntohl(_d); }
  u_int32_t v() const			{ return ntohl(_d); }
  net_u_int32_t &operator=(u_int32_t x) {
    _d = htonl(x); return *this;
  }
};

class net_u_int64_t {
  u_int64_t _d;
 public:
  net_u_int64_t(u_int64_t x)		{ _d = htonq(x); }
  operator u_int64_t() const		{ return ntohq(_d); }
  u_int64_t v() const			{ return ntohq(_d); }
  net_u_int64_t &operator=(u_int64_t x) {
    _d = htonq(x); return *this;
  }
};

class una_net_u_int16_t {
  unsigned char _d[2];
 public:
  una_net_u_int16_t(u_int16_t x)	{ *((u_int16_t *)_d) = htons(x); }
  operator u_int16_t() const		{ return ntohs(*((u_int16_t *)_d)); }
  u_int16_t v() const			{ return ntohs(*((u_int16_t *)_d)); }
  una_net_u_int16_t &operator=(u_int16_t x) {
    *((u_int16_t *)_d) = htons(x); return *this;
  }
};

class una_net_u_int32_t {
  unsigned char _d[4];
 public:
  una_net_u_int32_t(u_int32_t x)	{ *((u_int32_t *)_d) = htonl(x); }
  operator u_int32_t() const		{ return ntohl(*((u_int32_t *)_d)); }
  u_int32_t v() const			{ return ntohl(*((u_int32_t *)_d)); }
  una_net_u_int32_t &operator=(u_int32_t x) {
    *((u_int32_t *)_d) = htonl(x); return *this;
  }
};

class una_net_u_int64_t {
  unsigned char _d[8];
 public:
  una_net_u_int64_t(u_int64_t x)	{ *((u_int64_t *)_d) = htonq(x); }
  operator u_int64_t() const		{ return ntohq(*((u_int64_t *)_d)); }
  u_int64_t v() const			{ return ntohq(*((u_int64_t *)_d)); }
  una_net_u_int64_t &operator=(u_int64_t x) {
    *((u_int64_t *)_d) = htonq(x); return *this;
  }
};

#endif
