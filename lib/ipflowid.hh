#ifndef IPFLOWID_HH
#define IPFLOWID_HH
#include "ipaddress.hh"
class Packet;

class IPFlowID {

  IPAddress _saddr;
  IPAddress _daddr;
  unsigned short _sport;	// network byte order
  unsigned short _dport;	// network byte order

 public:

  IPFlowID();
  IPFlowID(IPAddress, unsigned short, IPAddress, unsigned short);
  explicit IPFlowID(Packet *);

  operator bool() const;
  unsigned hashcode() const;

  IPAddress saddr() const		{ return _saddr; }
  IPAddress daddr() const		{ return _daddr; }
  unsigned short sport() const		{ return _sport; }
  unsigned short dport() const		{ return _dport; }
  
  IPFlowID rev() const;

  operator String() const		{ return s(); }
  String s() const;
  
};

class IPFlowID2 : public IPFlowID {

  unsigned _protocol;		// only really need one byte

 public:

  IPFlowID2();
  IPFlowID2(IPAddress, unsigned short, IPAddress, unsigned short, unsigned char);
  IPFlowID2(const IPFlowID &, unsigned char);
  explicit IPFlowID2(Packet *);

  operator bool() const;

  unsigned protocol() const		{ return _protocol; }
  
  IPFlowID2 rev() const;
  
};

inline
IPFlowID::IPFlowID()
  : _saddr(), _daddr(), _sport(0), _dport(0)
{
}

inline
IPFlowID2::IPFlowID2()
  : IPFlowID(), _protocol(0)
{
}

inline
IPFlowID::IPFlowID(IPAddress saddr, unsigned short sport,
		   IPAddress daddr, unsigned short dport)
  : _saddr(saddr), _daddr(daddr), _sport(sport), _dport(dport)
{
}

inline
IPFlowID2::IPFlowID2(IPAddress saddr, unsigned short sport,
		     IPAddress daddr, unsigned short dport,
		     unsigned char p)
  : IPFlowID(saddr, sport, daddr, dport), _protocol(p)
{
}

inline
IPFlowID2::IPFlowID2(const IPFlowID &o, unsigned char p)
  : IPFlowID(o), _protocol(p)
{
}

inline
IPFlowID::operator bool() const
{
  return _saddr || _daddr;
}

inline
IPFlowID2::operator bool() const
{
  return _protocol != 0;
}

inline IPFlowID
IPFlowID::rev() const
{
  return IPFlowID(_daddr, _dport, _saddr, _sport);
}

inline IPFlowID2
IPFlowID2::rev() const
{
  return IPFlowID2(daddr(), dport(), saddr(), sport(), _protocol);
}

inline unsigned
IPFlowID::hashcode() const
{ 
#define CHUCK_MAGIC 0x4c6d92b3;
#define ROT(v, r) ((v)<<(r) | ((unsigned)(v))>>(32-(r)))
  return (ROT(_saddr.hashcode(), 13) 
	  ^ ROT(_daddr.hashcode(), 23) ^ (_sport | (_dport<<16)));
}

inline bool
operator==(const IPFlowID &a, const IPFlowID &b)
{
  return a.saddr() == b.saddr() && a.daddr() == b.daddr()
    && a.sport() == b.sport() && a.dport() == b.dport();
}

inline bool
operator==(const IPFlowID2 &a, const IPFlowID2 &b)
{
  return a.saddr() == b.saddr() && a.daddr() == b.daddr()
    && a.sport() == b.sport() && a.dport() == b.dport()
    && a.protocol() == b.protocol();
}

#endif
