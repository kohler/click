#ifndef ETHERADDRESS_HH
#define ETHERADDRESS_HH

#include "string.hh"

class EtherAddress {
  
  unsigned short _data[3];
  
 public:
  
  EtherAddress()			{ _data[0] = _data[1] = _data[2] = 0; }
  explicit EtherAddress(unsigned char *);
  
  operator bool() const;
  
  unsigned char *data()			{ return (unsigned char *)_data; }
  const unsigned char *data() const	{ return (const unsigned char *)_data; }
  const unsigned short *sdata() const	{ return _data; }

  bool is_group();
  
  unsigned hashcode() const;
  
  String s() const;
};

inline
EtherAddress::operator bool() const
{
  return _data[0] || _data[1] || _data[2];
}

inline unsigned
EtherAddress::hashcode() const
{
  return (_data[2] | (_data[1] << 16)) ^ (_data[0] << 9);
}

inline bool
operator==(const EtherAddress &a, const EtherAddress &b)
{
  return (a.sdata()[0] == b.sdata()[0] && a.sdata()[1] == b.sdata()[1]
	  && a.sdata()[2] == b.sdata()[2]);
}

inline bool
operator!=(const EtherAddress &a, const EtherAddress &b)
{
  return !(a == b);
}

#endif
