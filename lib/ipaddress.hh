#ifndef IPADDRESS_HH
#define IPADDRESS_HH
#include "string.hh"
#include "glue.hh"

class IPAddress {
  
  unsigned _s_addr;
  
 public:
  
  IPAddress()			: _s_addr(0) { }
  explicit IPAddress(unsigned char *);
  IPAddress(unsigned);	// network byte order IP address
  explicit IPAddress(const String &);	// "18.26.4.99"
  explicit IPAddress(struct in_addr);
  
  operator bool() const		{ return _s_addr != 0; }
  unsigned s_addr() const	{ return _s_addr; }
  
  operator struct in_addr() const;
  struct in_addr in_addr() const;
  
  unsigned char *data() const	{ return (unsigned char *)&_s_addr; }
  
  unsigned hashcode() const	{ return _s_addr; }
  
  operator String() const	{ return(s()); }
  String s() const;
  void print();
  
};

inline
IPAddress::IPAddress(unsigned int n)
  : _s_addr(n)
{
}

inline
IPAddress::IPAddress(struct in_addr ina)
  : _s_addr(ina.s_addr)
{
}

inline bool
operator==(IPAddress a, IPAddress b)
{
  return a.s_addr() == b.s_addr();
}

inline struct in_addr
IPAddress::in_addr() const
{
  struct in_addr ia;
  ia.s_addr = _s_addr;
  return ia;
}

inline
IPAddress::operator struct in_addr() const
{
  return in_addr();
}

#endif
