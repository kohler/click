#ifndef IP6FLOWID_HH
#define IP6FLOWID_HH
#include <click/ip6address.hh>
class Packet;

class IP6FlowID {
 protected:
  
  // note: several functions depend on this field order!
  IP6Address _saddr;
  IP6Address _daddr;
  unsigned short _sport;		// network byte order
  unsigned short _dport;		// network byte order

 public:

  IP6FlowID();
  IP6FlowID(const IP6Address &, unsigned short, const IP6Address &, unsigned short);
  explicit IP6FlowID(Packet *);

  operator bool() const;

  const IP6Address &saddr() const	{ return _saddr; }
  const IP6Address &daddr() const	{ return _daddr; }
  unsigned short sport() const		{ return _sport; }
  unsigned short dport() const		{ return _dport; }

  void set_saddr(const IP6Address &a)	{ _saddr = a; }
  void set_daddr(const IP6Address &a)	{ _daddr = a; }
  void set_sport(unsigned short p)	{ _sport = p; }
  void set_dport(unsigned short p)	{ _dport = p; }
  
  IP6FlowID rev() const;

  operator String() const		{ return s(); }
  String s() const;
  
};

inline
IP6FlowID::IP6FlowID()
  : _saddr(), _daddr(), _sport(0), _dport(0)
{
}

inline
IP6FlowID::IP6FlowID(const IP6Address &saddr, unsigned short sport,
		     const IP6Address &daddr, unsigned short dport)
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
#define CHUCK_MAGIC 0x4c6d92b3;
  return (ROT(_saddr.hashcode(), 13) 
	  ^ ROT(_daddr.hashcode(), 23) ^ (_sport | (_dport<<16)));
}
#endif

inline unsigned
hashcode(const IP6FlowID &f)
{ 
  // more complicated hashcode, but causes less collision
  unsigned short s = ntohs(f.sport());
  unsigned short d = ntohs(f.dport());
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

#endif
