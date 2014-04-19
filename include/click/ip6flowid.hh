// -*- c-basic-offset: 2; related-file-name: "../../lib/ip6flowid.cc" -*-
#ifndef CLICK_IP6FLOWID_HH
#define CLICK_IP6FLOWID_HH
#include <click/ip6address.hh>
#include <click/ipflowid.hh>
#include <click/hashcode.hh>
CLICK_DECLS
class Packet;

class IP6FlowID { public:

    typedef uninitialized_type uninitialized_t;

  /** @brief Construct an empty flow ID.
   *
   * The empty flow ID has zero-valued addresses and ports. */
  IP6FlowID();

  /** @brief Construct a flow ID with the given parts.
   * @param saddr source address
   * @param sport source port, in network order
   * @param daddr destination address
   * @param dport destination port, in network order */
  inline IP6FlowID(const IP6Address &, uint16_t, const IP6Address &, uint16_t);

  /** @brief Construct an IPv4-Mapped flow ID with the given parts.
   * @param saddr source address
   * @param sport source port, in network order
   * @param daddr destination address
   * @param dport destination port, in network order
   *
   * The IPv4 addresses are converted to IPv4-mapped IPv6 addresses
   * in the flow. */
  inline IP6FlowID(const IPAddress &, uint16_t, const IPAddress &, uint16_t);

  /** @brief Construct a flow ID from @a p's ip/ip6_header() and udp_header().
   * @param p input packet
   * @param reverse if true, use the reverse of @a p's flow ID
   *
   * @pre @a p's ip/ip6_header() must point to an IPv4 or IPv6
   * header respectively, and @a p's transport header should have
   * source and destination ports in the UDP-like positions; TCP,
   * UDP, and DCCP fit the bill. If the packet is IPv4 the IPv4
   * addresses are converted to IPv4-mapped IPv6 addresses in the flow.*/
  explicit IP6FlowID(const Packet *, bool reverse = false);

  /** @brief Construct a flow ID from @a ip6h and the following TCP/UDP header.
   * @param iph IP header
   * @param reverse if true, use the reverse of @a p's flow ID
   *
   * This assumes a single IPv6 header with no extension headers.The
   * transport header should have source and destination ports in
   * the UDP-like positions; TCP, UDP, and DCCP fit the bill. */
  explicit IP6FlowID(const click_ip6 *ip6h, bool reverse = false);

  /** @brief Construct an IPv4-Mapped flow ID from @a iph and the
   * following TCP/UDP header.
   * @param iph IP header
   * @param reverse if true, use the reverse of @a p's flow ID
   *
   * The IPv4 header's header length, @a iph->ip_hl, is used to find
   * the following transport header.  This transport header should
   * have source and destination ports in the UDP-like positions;
   * TCP, UDP, and DCCP fit the bill. The IPv4 addresses are
   * converted ip IPv4-mapped IPv6 addresses in the flow.*/
  explicit IP6FlowID(const click_ip *iph, bool reverse = false);

  /** @brief Construct an IPv4-Mapped flow ID from the given IPv4 @a flow.
   * @param flow and IPv4 IPFlowID
   *
   * The parameters IPv4 addresses in the IPFlowID converted to
   * IPv4-mapped IPv6 addresses. */
  explicit IP6FlowID(const IPFlowID &);


  /** @brief Construct an uninitialized flow ID. */
  inline IP6FlowID(const uninitialized_type &unused) {
    (void) unused;
  }

  typedef const IP6Address &(IP6FlowID::*unspecified_bool_type)() const;
  inline operator unspecified_bool_type() const;

  const IP6Address &saddr() const	{ return _saddr; }
  const IP6Address &daddr() const	{ return _daddr; }
  const IPAddress saddr4() const	{ return _saddr.ip4_address(); }
  const IPAddress daddr4() const	{ return _daddr.ip4_address(); }
  uint16_t sport() const		{ return _sport; }
  uint16_t dport() const		{ return _dport; }

  void set_saddr(const IP6Address &a)	{ _saddr = a; }
  void set_daddr(const IP6Address &a)	{ _daddr = a; }
  void set_saddr(const IPAddress &a)	{ _saddr = a; }
  void set_daddr(const IPAddress &a)	{ _daddr = a; }
  void set_sport(uint16_t p)		{ _sport = p; }
  void set_dport(uint16_t p)		{ _dport = p; }

  /** @brief Set this flow to the given value.
   * @param saddr source address
   * @param sport source port, in network order
   * @param daddr destination address
   * @param dport destination port, in network order */
  void assign(IP6Address saddr, uint16_t sport, IP6Address daddr, uint16_t dport) {
    _saddr = saddr;
    _daddr = daddr;
    _sport = sport;
    _dport = dport;
  }

  /** @brief Set this flow to the given values using IPv4-mapped addresses.
   * @param saddr source address
   * @param sport source port, in network order
   * @param daddr destination address
   * @param dport destination port, in network order */
  void assign(IPAddress saddr, uint16_t sport, IPAddress daddr, uint16_t dport) {
    _saddr = saddr;
    _daddr = daddr;
    _sport = sport;
    _dport = dport;
  }

  inline IP6FlowID reverse() const;
  inline IP6FlowID rev() const CLICK_DEPRECATED;

  /** @brief Indicate if this flow is IPv4.
   * @return true iff the flow is IPv4 */
  inline bool is_ip4_mapped() const { return _saddr.is_ip4_mapped(); }

  /** @brief Return IPv4 version of a IPv4-mapped IP6FlowID.
   *
   * @return non-empty IPFlowID address iff this flow is using
   *  IPv4-mapped addresses.  IPFlowID() otherwise */
  IPFlowID flow_id4() const;

  inline IP6FlowID &operator=(const IPFlowID &);

  inline hashcode_t hashcode() const;

  String unparse() const;
  operator String() const		{ return unparse(); }
  String s() const			{ return unparse(); }

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
IP6FlowID::IP6FlowID(const IPAddress &saddr, uint16_t sport,
		     const IPAddress &daddr, uint16_t dport)
  : _saddr(saddr), _daddr(daddr), _sport(sport), _dport(dport)
{
}

inline
IP6FlowID::IP6FlowID(const IPFlowID &flow)
{
  *this = flow;
}

inline
IP6FlowID::operator unspecified_bool_type() const
{
  return _saddr || _daddr ? &IP6FlowID::saddr : 0;
}

inline IP6FlowID
IP6FlowID::reverse() const
{
  return IP6FlowID(_daddr, _dport, _saddr, _sport);
}

inline IP6FlowID
IP6FlowID::rev() const
{
  return reverse();
}

inline IP6FlowID &
IP6FlowID::operator=(const IPFlowID &f)
{
  assign(f.saddr(),f.sport(),f.daddr(),f.dport());
  return *this;
}

#define ROT(v, r) ((v)<<(r) | ((unsigned)(v))>>(32-(r)))

#if 0
inline hashcode_t
IP6FlowID::hashcode() const
{
  return (ROT(_saddr.hashcode(), 13)
          ^ ROT(_daddr.hashcode(), 23) ^ (_sport | (_dport<<16)));
}
#endif

inline hashcode_t IP6FlowID::hashcode() const
{
  // more complicated hashcode, but causes less collision
  uint16_t s = ntohs(sport());
  uint16_t d = ntohs(dport());
  hashcode_t sx = CLICK_NAME(hashcode)(saddr());
  hashcode_t dx = CLICK_NAME(hashcode)(daddr());
  return (ROT(sx, s%16)
          ^ ROT(dx, 31-d%16))
    ^ ((d << 16) | s);
}

#undef ROT

StringAccum &operator<<(StringAccum &sa, const IP6FlowID &flow_id);

inline bool
operator==(const IP6FlowID &a, const IP6FlowID &b)
{
  return a.dport() == b.dport() && a.sport() == b.sport()
    && a.daddr() == b.daddr() && a.saddr() == b.saddr();
}

inline bool
operator!=(const IP6FlowID &a, const IP6FlowID &b)
{
  return !(a == b);
}

CLICK_ENDDECLS
#endif
