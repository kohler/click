// -*- c-basic-offset: 2; related-file-name: "../../lib/etheraddress.cc" -*-
#ifndef CLICK_ETHERADDRESS_HH
#define CLICK_ETHERADDRESS_HH
#include <click/string.hh>
CLICK_DECLS

class EtherAddress { public:
  
  EtherAddress()			{ _data[0] = _data[1] = _data[2] = 0; }
  explicit EtherAddress(const unsigned char *);
  
  operator bool() const;
  bool is_group() const;
    
  unsigned char *data();
  const unsigned char *data() const;
  const uint16_t *sdata() const		{ return _data; }

  String unparse() const;

  operator String() const		{ return unparse(); }
  String s() const			{ return unparse(); }
  
 private:
  
  uint16_t _data[3];
  
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

CLICK_ENDDECLS
#endif
