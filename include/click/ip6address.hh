// -*- related-file-name: "../../lib/ip6address.cc" -*-
#ifndef CLICK_IP6ADDRESS_HH
#define CLICK_IP6ADDRESS_HH
#include <click/string.hh>
#include <clicknet/ip6.h>
#include <click/ipaddress.hh>
#include <click/etheraddress.hh>
#if !CLICK_TOOL
# include <click/packet.hh>
# include <click/packet_anno.hh>
#endif
CLICK_DECLS

class IP6Address { public:

  IP6Address();
  explicit IP6Address(const unsigned char *);
  explicit IP6Address(IPAddress ip);
  explicit IP6Address(const String &);		// "fec0:0:0:1::1"
  explicit IP6Address(const click_in6_addr &a)	: _addr(a) { }
  static IP6Address make_prefix(int);

  typedef uint32_t (IP6Address::*unspecified_bool_type)() const;
  operator unspecified_bool_type() const;

  operator const click_in6_addr &() const	{ return _addr; }
  operator click_in6_addr &()			{ return _addr; }
  const click_in6_addr &in6_addr() const	{ return _addr;	}
  click_in6_addr &in6_addr()			{ return _addr;	}

  unsigned char *data()			{ return &_addr.s6_addr[0]; }
  const unsigned char *data() const	{ return &_addr.s6_addr[0]; }
  uint32_t *data32()			{ return &_addr.s6_addr32[0]; }
  const uint32_t *data32() const	{ return &_addr.s6_addr32[0]; }

    inline uint32_t hashcode() const;

  int mask_to_prefix_len() const;
  bool matches_prefix(const IP6Address &addr, const IP6Address &mask) const;
  bool mask_as_specific(const IP6Address &) const;

  bool ether_address(EtherAddress &) const;
  bool ip4_address(IPAddress &) const;

  // bool operator==(const IP6Address &, const IP6Address &);
  // bool operator!=(const IP6Address &, const IP6Address &);

  // IP6Address operator&(const IP6Address &, const IP6Address &);
  // IP6Address operator|(const IP6Address &, const IP6Address &);
  // IP6Address operator~(const IP6Address &);

  IP6Address &operator&=(const IP6Address &);
  IP6Address &operator&=(const click_in6_addr &);
  IP6Address &operator|=(const IP6Address &);
  IP6Address &operator|=(const click_in6_addr &);

  IP6Address &operator=(const click_in6_addr &);

  String unparse() const;
  String unparse_expanded() const;

  operator String() const		{ return unparse(); }
  String s() const			{ return unparse(); }

 private:

  click_in6_addr _addr;

};

inline
IP6Address::operator unspecified_bool_type() const
{
  const uint32_t *ai = data32();
  return ai[0] || ai[1] || ai[2] || ai[3] ? &IP6Address::hashcode : 0;
}

inline bool
operator==(const IP6Address &a, const IP6Address &b)
{
  const uint32_t *ai = a.data32(), *bi = b.data32();
  return ai[0] == bi[0] && ai[1] == bi[1] && ai[2] == bi[2] && ai[3] == bi[3];
}

inline bool
operator!=(const IP6Address &a, const IP6Address &b)
{
  const uint32_t *ai = a.data32(), *bi = b.data32();
  return ai[0] != bi[0] || ai[1] != bi[1] || ai[2] != bi[2] || ai[3] != bi[3];
}

class StringAccum;
StringAccum &operator<<(StringAccum &, const IP6Address &);

inline bool
IP6Address::matches_prefix(const IP6Address &addr, const IP6Address &mask) const
{
  const uint32_t *xi = data32(), *ai = addr.data32(), *mi = mask.data32();
  return ((xi[0] ^ ai[0]) & mi[0]) == 0
    && ((xi[1] ^ ai[1]) & mi[1]) == 0
    && ((xi[2] ^ ai[2]) & mi[2]) == 0
    && ((xi[3] ^ ai[3]) & mi[3]) == 0;
}

inline bool
IP6Address::mask_as_specific(const IP6Address &mask) const
{
  const uint32_t *xi = data32(), *mi = mask.data32();
  return ((xi[0] & mi[0]) == mi[0] && (xi[1] & mi[1]) == mi[1]
	  && (xi[2] & mi[2]) == mi[2] && (xi[3] & mi[3]) == mi[3]);
}

inline IP6Address &
IP6Address::operator&=(const IP6Address &b)
{
  uint32_t *ai = data32();
  const uint32_t *bi = b.data32();
  ai[0] &= bi[0]; ai[1] &= bi[1]; ai[2] &= bi[2]; ai[3] &= bi[3];
  return *this;
}

inline IP6Address &
IP6Address::operator&=(const click_in6_addr &b)
{
  uint32_t *ai = data32();
  const uint32_t *bi = b.s6_addr32;
  ai[0] &= bi[0]; ai[1] &= bi[1]; ai[2] &= bi[2]; ai[3] &= bi[3];
  return *this;
}

inline IP6Address &
IP6Address::operator|=(const IP6Address &b)
{
  uint32_t *ai = data32();
  const uint32_t *bi = b.data32();
  ai[0] |= bi[0]; ai[1] |= bi[1]; ai[2] |= bi[2]; ai[3] |= bi[3];
  return *this;
}

inline IP6Address &
IP6Address::operator|=(const click_in6_addr &b)
{
  uint32_t *ai = data32();
  const uint32_t *bi = b.s6_addr32;
  ai[0] |= bi[0]; ai[1] |= bi[1]; ai[2] |= bi[2]; ai[3] |= bi[3];
  return *this;
}

inline IP6Address
operator&(const IP6Address &a, const IP6Address &b)
{
  const uint32_t *ai = a.data32(), *bi = b.data32();
  IP6Address result;
  uint32_t *ri = result.data32();
  ri[0] = ai[0] & bi[0];
  ri[1] = ai[1] & bi[1];
  ri[2] = ai[2] & bi[2];
  ri[3] = ai[3] & bi[3];
  return result;
}

inline IP6Address
operator|(const IP6Address &a, const IP6Address &b)
{
  const uint32_t *ai = a.data32(), *bi = b.data32();
  IP6Address result;
  uint32_t *ri = result.data32();
  ri[0] = ai[0] | bi[0];
  ri[1] = ai[1] | bi[1];
  ri[2] = ai[2] | bi[2];
  ri[3] = ai[3] | bi[3];
  return result;
}

inline IP6Address
operator~(const IP6Address &a)
{
  const uint32_t *ai = a.data32();
  IP6Address result;
  uint32_t *ri = result.data32();
  ri[0] = ~ai[0]; ri[1] = ~ai[1]; ri[2] = ~ai[2]; ri[3] = ~ai[3];
  return result;
}

inline IP6Address &
IP6Address::operator=(const click_in6_addr &a)
{
  _addr = a;
  return *this;
}

inline uint32_t
IP6Address::hashcode() const
{
    return (data32()[3] << 1) + data32()[4];
}

#if !CLICK_TOOL
inline const IP6Address &
DST_IP6_ANNO(Packet *p)
{
    return *reinterpret_cast<IP6Address *>(p->anno_u8() + DST_IP6_ANNO_OFFSET);
}

inline void
SET_DST_IP6_ANNO(Packet *p, const IP6Address &a)
{
    memcpy(p->anno_u8() + DST_IP6_ANNO_OFFSET, a.data(), DST_IP6_ANNO_SIZE);
}

inline void
SET_DST_IP6_ANNO(Packet *p, const click_in6_addr &a)
{
    memcpy(p->anno_u8() + DST_IP6_ANNO_OFFSET, &a, DST_IP6_ANNO_SIZE);
}
#endif

CLICK_ENDDECLS
#endif
