#ifndef IP6ADDRESS_HH
#define IP6ADDRESS_HH
#include "string.hh"
#include "click_ip6.h"

class IP6Address {
 
  click_in6_addr _addr;
  
 public:
  
  IP6Address();
  explicit IP6Address(const click_in6_addr &a)	: _addr(a) { }
  explicit IP6Address(const unsigned char *);
  explicit IP6Address(const String &);		// "fec0:0:0:1::1"

  operator bool() const;
  
  operator const click_in6_addr &() const	{ return _addr; }
  const click_in6_addr &addr() const	 	{ return _addr;	}
  const click_in6_addr &in6_addr() const 	{ return _addr;	}

  unsigned char *data()			{ return &_addr.s6_addr[0]; }
  const unsigned char *data() const	{ return &_addr.s6_addr[0]; }
  // int *hashcode() const	{ return (unsigned *)&(_addr); }
  bool get_ip4address(unsigned char ip4[4]) const;
  
  operator String() const	{ return s(); }
  String s() const;
  String full_s() const;

  IP6Address &operator=(const click_in6_addr &);
  
};

inline
IP6Address::operator bool() const
{ 
  for (int i = 0; i < 4; i++)
    if (_addr.s6_addr32[i])
      return false;
  return true;
}

inline IP6Address
operator&(const IP6Address &a, const IP6Address &b)
{
  click_in6_addr result;
  for (int i = 0; i < 4; i++)
    result.s6_addr32[i] = a.addr().s6_addr32[i] & b.addr().s6_addr32[i];
  return IP6Address(result);
}

inline bool
operator==(const IP6Address &a, const IP6Address &b)
{  
  for (int i = 0; i < 4; i++)
    if (a.addr().s6_addr32[i] != b.addr().s6_addr32[i])
      return false;
  return true;
}

inline bool
operator!=(const IP6Address &a, const IP6Address &b)
{
  for (int i = 0; i < 4; i++)
    if (a.addr().s6_addr32[i] != b.addr().s6_addr32[i])
      return true;
  return false;
}

inline bool
operator > (const IP6Address &a, const IP6Address &b)
{
  for (int i=0; i<4; i++) {
    if (a.addr().s6_addr32[i] > b.addr().s6_addr32[i]) 
      return true;
    else if (a.addr().s6_addr32[i] < b.addr().s6_addr32[i]) 
      return false;
  }
  return false;
}

inline IP6Address &
IP6Address::operator=(const click_in6_addr &a)
{
  _addr = a;
  return *this;
}

#endif
