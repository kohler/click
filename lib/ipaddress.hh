#ifndef IPADDRESS_HH
#define IPADDRESS_HH
#include "string.hh"
#include "glue.hh"

class IPAddress {
  
  unsigned _addr;
  
 public:
  
  IPAddress()			: _addr(0) { }
  explicit IPAddress(unsigned char *);
  IPAddress(unsigned);	// network byte order IP address
  explicit IPAddress(const String &);	// "18.26.4.99"
  explicit IPAddress(struct in_addr);
  
  operator bool() const		{ return _addr != 0; }
  unsigned addr() const		{ return _addr; }
  
  operator struct in_addr() const;
  struct in_addr in_addr() const;

  unsigned char *data() const	{ return (unsigned char *)&_addr; }
  
  unsigned hashcode() const	{ return _addr; }
  
  operator String() const	{ return s(); }
  String s() const;
  void print();
  
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

#endif
