#ifndef IP6ADDRESS_HH
#define IP6ADDRESS_HH
#include <click/string.hh>
#include <click/click_ip6.h>
#include <click/ipaddress.hh>

class IP6Address { public:
 
  IP6Address();
  explicit IP6Address(const unsigned char *);
  explicit IP6Address(IPAddress ip);
  explicit IP6Address(const String &);		// "fec0:0:0:1::1"
  explicit IP6Address(const click_in6_addr &a)	: _addr(a) { }
  static IP6Address make_prefix(int);

  operator bool() const;
  
  operator const click_in6_addr &() const	{ return _addr; }
  operator click_in6_addr &()			{ return _addr; }
  const click_in6_addr &in6_addr() const	{ return _addr;	}
  click_in6_addr &in6_addr()		 	{ return _addr;	}

  unsigned char *data()			{ return &_addr.s6_addr[0]; }
  const unsigned char *data() const	{ return &_addr.s6_addr[0]; }
  unsigned *data32()			{ return &_addr.s6_addr32[0]; }
  const unsigned *data32() const	{ return &_addr.s6_addr32[0]; }
  
  // int *hashcode() const	{ return (unsigned *)&(_addr); }
  unsigned hashcode() const	        { return _addr.s6_addr32[3]; }
  int mask_to_prefix_len() const;
  bool matches_prefix(const IP6Address &addr, const IP6Address &mask) const;
  bool mask_more_specific(const IP6Address &) const;
  
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
IP6Address::operator bool() const
{
  const unsigned *ai = data32();
  return ai[0] || ai[1] || ai[2] || ai[3];
}

inline bool
operator==(const IP6Address &a, const IP6Address &b)
{
  const unsigned *ai = a.data32(), *bi = b.data32();
  return ai[0] == bi[0] && ai[1] == bi[1] && ai[2] == bi[2] && ai[3] == bi[3];
}

inline bool
operator!=(const IP6Address &a, const IP6Address &b)
{
  const unsigned *ai = a.data32(), *bi = b.data32();
  return ai[0] != bi[0] || ai[1] != bi[1] || ai[2] != bi[2] || ai[3] != bi[3];
}

inline bool
IP6Address::matches_prefix(const IP6Address &addr, const IP6Address &mask) const
{
  const unsigned *xi = data32(), *ai = addr.data32(), *mi = mask.data32();
  return ((xi[0] & mi[0]) == ai[0] && (xi[1] & mi[1]) == ai[1]
	  && (xi[2] & mi[2]) == ai[2] && (xi[3] & mi[3]) == ai[3]);
}

inline bool
IP6Address::mask_more_specific(const IP6Address &mask) const
{
  const unsigned *xi = data32(), *mi = mask.data32();
  return ((xi[0] & mi[0]) == mi[0] && (xi[1] & mi[1]) == mi[1]
	  && (xi[2] & mi[2]) == mi[2] && (xi[3] & mi[3]) == mi[3]);
}

inline IP6Address &
IP6Address::operator&=(const IP6Address &b)
{
  unsigned *ai = data32();
  const unsigned *bi = b.data32();
  ai[0] &= bi[0]; ai[1] &= bi[1]; ai[2] &= bi[2]; ai[3] &= bi[3];
  return *this;
}

inline IP6Address &
IP6Address::operator&=(const click_in6_addr &b)
{
  unsigned *ai = data32();
  const unsigned *bi = b.s6_addr32;
  ai[0] &= bi[0]; ai[1] &= bi[1]; ai[2] &= bi[2]; ai[3] &= bi[3];
  return *this;
}

inline IP6Address &
IP6Address::operator|=(const IP6Address &b)
{
  unsigned *ai = data32();
  const unsigned *bi = b.data32();
  ai[0] |= bi[0]; ai[1] |= bi[1]; ai[2] |= bi[2]; ai[3] |= bi[3];
  return *this;
}

inline IP6Address &
IP6Address::operator|=(const click_in6_addr &b)
{
  unsigned *ai = data32();
  const unsigned *bi = b.s6_addr32;
  ai[0] |= bi[0]; ai[1] |= bi[1]; ai[2] |= bi[2]; ai[3] |= bi[3];
  return *this;
}

inline IP6Address
operator&(const IP6Address &a, const IP6Address &b)
{
  const unsigned *ai = a.data32(), *bi = b.data32();
  IP6Address result;
  unsigned *ri = result.data32();
  ri[0] = ai[0] & bi[0]; ri[1] = ai[1] & bi[1];
  ri[2] = ai[2] & bi[2]; ri[3] = ai[3] & bi[3];
  return result;
}

inline IP6Address
operator|(const IP6Address &a, const IP6Address &b)
{
  const unsigned *ai = a.data32(), *bi = b.data32();
  IP6Address result;
  unsigned *ri = result.data32();
  ri[0] = ai[0] | bi[0]; ri[1] = ai[1] | bi[1];
  ri[2] = ai[2] | bi[2]; ri[3] = ai[3] | bi[3];
  return result;
}

inline IP6Address
operator~(const IP6Address &a)
{
  const unsigned *ai = a.data32();
  IP6Address result;
  unsigned *ri = result.data32();
  ri[0] = ~ai[0]; ri[1] = ~ai[1]; ri[2] = ~ai[2]; ri[3] = ~ai[3];
  return result;
}

inline IP6Address &
IP6Address::operator=(const click_in6_addr &a)
{
  _addr = a;
  return *this;
}

#endif
