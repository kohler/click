#ifndef IPADDRESS_HH
#define IPADDRESS_HH
#include <click/string.hh>
#include <click/click_ip.h>

class IPAddress {
  
  unsigned _addr;
  
 public:
  
  IPAddress()			: _addr(0) { }
  explicit IPAddress(const unsigned char *);
  IPAddress(unsigned);	// network byte order IP address
  explicit IPAddress(const String &);	// "18.26.4.99"
  explicit IPAddress(struct in_addr);
  
  operator bool() const		{ return _addr != 0; }
  operator unsigned() const	{ return _addr; }
  unsigned addr() const		{ return _addr; }
  
  operator struct in_addr() const;
  struct in_addr in_addr() const;

  unsigned char *data();
  const unsigned char *data() const;
  
  unsigned hashcode() const	{ return _addr; }
  
  operator String() const	{ return s(); }
  String s() const;
  
};

inline
IPAddress::IPAddress(unsigned int n)
  : _addr(n)
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

#endif
