#ifndef IPADDRESS_HH
#define IPADDRESS_HH
#include <click/string.hh>
#include <click/glue.hh>

class IPAddress {
  
  u_int32_t _addr;
  
 public:
  
  IPAddress()			: _addr(0) { }
  explicit IPAddress(const unsigned char *);
  IPAddress(u_int32_t);			// network byte order IP address
  explicit IPAddress(int32_t);		// network byte order IP address
  explicit IPAddress(const String &);	// "18.26.4.99"
  IPAddress(struct in_addr);
  static IPAddress make_prefix(int);
  
  operator bool() const		{ return _addr != 0; }
  operator u_int32_t() const	{ return _addr; }
  u_int32_t addr() const	{ return _addr; }
  
  operator struct in_addr() const;
  struct in_addr in_addr() const;

  unsigned char *data();
  const unsigned char *data() const;
  
  unsigned hashcode() const	{ return _addr; }
  int mask_to_prefix_bits() const;

  IPAddress &operator&=(IPAddress);
  IPAddress &operator|=(IPAddress);

  String unparse() const;
  String unparse_mask() const;
  String unparse_with_mask(IPAddress) const;
  
  operator String() const	{ return unparse(); }
  String s() const		{ return unparse(); }
  
};

inline
IPAddress::IPAddress(u_int32_t a)
  : _addr(a)
{
}

inline
IPAddress::IPAddress(int32_t a)
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
operator!=(IPAddress a, IPAddress b)
{
  return a.addr() != b.addr();
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

#endif
