// -*- c-basic-offset: 2; related-file-name: "../../lib/ipflowid.cc" -*-
#ifndef CLICK_IPFLOWID_HH
#define CLICK_IPFLOWID_HH
#include <click/ipaddress.hh>
class Packet;

class IPFlowID { public:

  IPFlowID();
  IPFlowID(IPAddress, uint16_t, IPAddress, uint16_t);
  explicit IPFlowID(Packet *);

  operator bool() const;

  IPAddress saddr() const		{ return _saddr; }
  IPAddress daddr() const		{ return _daddr; }
  uint16_t sport() const		{ return _sport; }
  uint16_t dport() const		{ return _dport; }

  void set_saddr(IPAddress a)		{ _saddr = a; }
  void set_daddr(IPAddress a)		{ _daddr = a; }
  void set_sport(uint16_t p)		{ _sport = p; }	// network order
  void set_dport(uint16_t p)		{ _dport = p; }	// network order
  
  IPFlowID rev() const;

  String unparse() const;
  operator String() const		{ return unparse(); }
  String s() const			{ return unparse(); }

 protected:
  
  // note: several functions depend on this field order!
  IPAddress _saddr;
  IPAddress _daddr;
  uint16_t _sport;			// network byte order
  uint16_t _dport;			// network byte order

};

inline
IPFlowID::IPFlowID()
  : _saddr(), _daddr(), _sport(0), _dport(0)
{
}

inline
IPFlowID::IPFlowID(IPAddress saddr, uint16_t sport,
		   IPAddress daddr, uint16_t dport)
  : _saddr(saddr), _daddr(daddr), _sport(sport), _dport(dport)
{
}

inline
IPFlowID::operator bool() const
{
  return _saddr || _daddr;
}

inline IPFlowID
IPFlowID::rev() const
{
  return IPFlowID(_daddr, _dport, _saddr, _sport);
}


#define ROT(v, r) ((v)<<(r) | ((unsigned)(v))>>(32-(r)))

inline unsigned
hashcode(const IPFlowID &f)
{ 
  // more complicated hashcode, but causes less collision
  uint16_t s = ntohs(f.sport());
  uint16_t d = ntohs(f.dport());
  return (ROT(hashcode(f.saddr()), s%16)
          ^ ROT(hashcode(f.daddr()), 31-d%16))
	  ^ ((d << 16) | s);
}

#undef ROT

inline bool
operator==(const IPFlowID &a, const IPFlowID &b)
{
  return a.dport() == b.dport() && a.sport() == b.sport()
    && a.daddr() == b.daddr() && a.saddr() == b.saddr();
}

inline bool
operator!=(const IPFlowID &a, const IPFlowID &b)
{
  return a.dport() != b.dport() || a.sport() != b.sport()
    || a.daddr() != b.daddr() || a.saddr() != b.saddr();
}

StringAccum &operator<<(StringAccum &, const IPFlowID &);

#endif
