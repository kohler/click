#ifndef IPFLOWID_HH
#define IPFLOWID_HH
#include <click/ipaddress.hh>
class Packet;

class IPFlowID {
 protected:
  
  // note: several functions depend on this field order!
  IPAddress _saddr;
  IPAddress _daddr;
  unsigned short _sport;		// network byte order
  unsigned short _dport;		// network byte order

 public:

  IPFlowID();
  IPFlowID(IPAddress, unsigned short, IPAddress, unsigned short);
  explicit IPFlowID(Packet *);

  operator bool() const;

  IPAddress saddr() const		{ return _saddr; }
  IPAddress daddr() const		{ return _daddr; }
  unsigned short sport() const		{ return _sport; }
  unsigned short dport() const		{ return _dport; }

  void set_saddr(IPAddress a)		{ _saddr = a; }
  void set_daddr(IPAddress a)		{ _daddr = a; }
  void set_sport(unsigned short p)	{ _sport = p; }
  void set_dport(unsigned short p)	{ _dport = p; }
  
  IPFlowID rev() const;

  String unparse() const;
  operator String() const		{ return unparse(); }
  String s() const			{ return unparse(); }
  
};

inline
IPFlowID::IPFlowID()
  : _saddr(), _daddr(), _sport(0), _dport(0)
{
}

inline
IPFlowID::IPFlowID(IPAddress saddr, unsigned short sport,
		   IPAddress daddr, unsigned short dport)
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
  unsigned short s = ntohs(f.sport());
  unsigned short d = ntohs(f.dport());
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
