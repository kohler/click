// -*- c-basic-offset: 2; related-file-name: "../../lib/ipaddress.cc" -*-
#ifndef CLICK_IPADDRESS_HH
#define CLICK_IPADDRESS_HH
#include <click/string.hh>
#include <click/glue.hh>
#include <clicknet/ip.h>
CLICK_DECLS

class IPAddress { public:
  
  IPAddress()			: _addr(0) { }
  explicit IPAddress(const unsigned char *);
  IPAddress(unsigned int);		// network byte order IP address
  explicit IPAddress(int);		// network byte order IP address
  explicit IPAddress(unsigned long);	// network byte order IP address
  explicit IPAddress(long);		// network byte order IP address
  explicit IPAddress(const String &);	// "18.26.4.99"
  IPAddress(struct in_addr);
  static IPAddress make_prefix(int);
  
  operator bool() const		{ return _addr != 0; }
  operator uint32_t() const	{ return _addr; }
  uint32_t addr() const		{ return _addr; }
  
  operator struct in_addr() const;
  struct in_addr in_addr() const;

  unsigned char *data();
  const unsigned char *data() const;
  
  int mask_to_prefix_len() const;
  bool matches_prefix(IPAddress addr, IPAddress mask) const;
  bool mask_as_specific(IPAddress) const;

  // bool operator==(IPAddress, IPAddress);
  // bool operator==(IPAddress, uint32_t);
  // bool operator!=(IPAddress, IPAddress);
  // bool operator!=(IPAddress, uint32_t);
  
  // IPAddress operator&(IPAddress, IPAddress);
  // IPAddress operator|(IPAddress, IPAddress);
  // IPAddress operator~(IPAddress);
  
  IPAddress &operator&=(IPAddress);
  IPAddress &operator|=(IPAddress);

  String unparse() const;
  String unparse_mask() const;
  String unparse_with_mask(IPAddress) const;
  
  operator String() const	{ return unparse(); }
  String s() const		{ return unparse(); }

 private:
  
  uint32_t _addr;

};

inline
IPAddress::IPAddress(unsigned int a)
  : _addr(a)
{
}

inline
IPAddress::IPAddress(int a)
  : _addr(a)
{
}

inline
IPAddress::IPAddress(unsigned long a)
  : _addr(a)
{
}

inline
IPAddress::IPAddress(long a)
  : _addr(a)
{
}

inline
IPAddress::IPAddress(struct in_addr ina)
  : _addr(ina.s_addr)
{
}

inline bool
operator==(IPAddress a, IPAddress b)
{
  return a.addr() == b.addr();
}

inline bool
operator==(IPAddress a, uint32_t b)
{
  return a.addr() == b;
}

inline bool
operator!=(IPAddress a, IPAddress b)
{
  return a.addr() != b.addr();
}

inline bool
operator!=(IPAddress a, uint32_t b)
{
  return a.addr() != b;
}

inline const unsigned char *
IPAddress::data() const
{
  return reinterpret_cast<const unsigned char *>(&_addr);
}

inline unsigned char *
IPAddress::data()
{
  return reinterpret_cast<unsigned char *>(&_addr);
}

inline struct in_addr
IPAddress::in_addr() const
{
  struct in_addr ia;
  ia.s_addr = _addr;
  return ia;
}

inline
IPAddress::operator struct in_addr() const
{
  return in_addr();
}

class StringAccum;
StringAccum &operator<<(StringAccum &, IPAddress);

inline bool
IPAddress::matches_prefix(IPAddress a, IPAddress mask) const
{
  return (addr() & mask.addr()) == a.addr();
}

inline bool
IPAddress::mask_as_specific(IPAddress mask) const
{
  return (addr() & mask.addr()) == mask.addr();
}

inline IPAddress
operator&(IPAddress a, IPAddress b)
{
  return IPAddress(a.addr() & b.addr());
}

inline IPAddress &
IPAddress::operator&=(IPAddress a)
{
  _addr &= a._addr;
  return *this;
}

inline IPAddress
operator|(IPAddress a, IPAddress b)
{
  return IPAddress(a.addr() | b.addr());
}

inline IPAddress &
IPAddress::operator|=(IPAddress a)
{
  _addr |= a._addr;
  return *this;
}

inline IPAddress
operator~(IPAddress a)
{
  return IPAddress(~a.addr());
}

inline unsigned
hashcode(IPAddress a)
{
  return a.addr();
}

CLICK_ENDDECLS
#endif
