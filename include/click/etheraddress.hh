#ifndef ETHERADDRESS_HH
#define ETHERADDRESS_HH
#include <click/string.hh>

class EtherAddress {
  
  unsigned short _data[3];
  
 public:
  
  EtherAddress()			{ _data[0] = _data[1] = _data[2] = 0; }
  explicit EtherAddress(unsigned char *);
  
  operator bool() const;
  bool is_group() const;
    
  unsigned char *data();
  const unsigned char *data() const;
  const unsigned short *sdata() const	{ return _data; }

  String s() const;
};

inline
EtherAddress::operator bool() const
{
  return _data[0] || _data[1] || _data[2];
}

inline const unsigned char *
EtherAddress::data() const
{
  return reinterpret_cast<const unsigned char *>(_data);
}

inline unsigned char *
EtherAddress::data()
{
  return reinterpret_cast<unsigned char *>(_data);
}

inline bool
EtherAddress::is_group() const
{
  return data()[0] & 1;
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

class StringAccum;
StringAccum &operator<<(StringAccum &, const EtherAddress &);

inline int
hashcode(const EtherAddress &ea)
{
  const unsigned char *d = ea.data();
  return (d[2] | (d[1] << 16)) ^ (d[0] << 9);
}

#endif
