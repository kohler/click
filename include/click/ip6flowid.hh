// -*- c-basic-offset: 2; related-file-name: "../../lib/ip6flowid.cc" -*-
#ifndef CLICK_IP6FLOWID_HH
#define CLICK_IP6FLOWID_HH
#include <click/ip6address.hh>
CLICK_DECLS
class Packet;

class IP6FlowID { public:

  IP6FlowID();
  IP6FlowID(const IP6Address &, uint16_t, const IP6Address &, uint16_t);
  explicit IP6FlowID(Packet *);

  operator bool() const;

  const IP6Address &saddr() const	{ return _saddr; }
  const IP6Address &daddr() const	{ return _daddr; }
  uint16_t sport() const		{ return _sport; }
  uint16_t dport() const		{ return _dport; }

  void set_saddr(const IP6Address &a)	{ _saddr = a; }
  void set_daddr(const IP6Address &a)	{ _daddr = a; }
  void set_sport(uint16_t p)		{ _sport = p; }
  void set_dport(uint16_t p)		{ _dport = p; }
  
  IP6FlowID rev() const;

  operator String() const		{ return s(); }
  String s() const;
  
 protected:
  
  // note: several functions depend on this field order!
  IP6Address _saddr;
  IP6Address _daddr;
  uint16_t _sport;			// network byte order
  uint16_t _dport;			// network byte order

};

inline
IP6FlowID::IP6FlowID()
  : _saddr(), _daddr(), _sport(0), _dport(0)
{
}

inline
IP6FlowID::IP6FlowID(const IP6Address &saddr, uint16_t sport,
		     const IP6Address &daddr, uint16_t dport)
  : _saddr(saddr), _daddr(daddr), _sport(sport), _dport(dport)
{
}

inline
IP6FlowID::operator bool() const
{
  return _saddr || _daddr;
}

inline IP6FlowID
IP6FlowID::rev() const
{
  return IP6FlowID(_daddr, _dport, _saddr, _sport);
}


#define ROT(v, r) ((v)<<(r) | ((unsigned)(v))>>(32-(r)))

#if 0
inline unsigned
IP6FlowID::hashcode() const
{ 
  return (ROT(_saddr.hashcode(), 13) 
	  ^ ROT(_daddr.hashcode(), 23) ^ (_sport | (_dport<<16)));
}
#endif

inline unsigned
hashcode(const IP6FlowID &f)
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
operator==(const IP6FlowID &a, const IP6FlowID &b)
{
  return a.dport() == b.dport() && a.sport() == b.sport()
    && a.daddr() == b.daddr() && a.saddr() == b.saddr();
}

CLICK_ENDDECLS
#endif
