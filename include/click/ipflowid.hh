// -*- related-file-name: "../../lib/ipflowid.cc" -*-
#ifndef CLICK_IPFLOWID_HH
#define CLICK_IPFLOWID_HH
#include <click/ipaddress.hh>
#include <click/hashcode.hh>
CLICK_DECLS
class Packet;

class IPFlowID { public:

    typedef uninitialized_type uninitialized_t;


    /** @brief Construct an empty flow ID.
     *
     * The empty flow ID has zero-valued addresses and ports. */
    IPFlowID()
	: _saddr(), _daddr(), _sport(0), _dport(0) {
    }

    /** @brief Construct a flow ID with the given parts.
     * @param saddr source address
     * @param sport source port, in network order
     * @param daddr destination address
     * @param dport destination port, in network order */
    IPFlowID(IPAddress saddr, uint16_t sport, IPAddress daddr, uint16_t dport)
	: _saddr(saddr), _daddr(daddr), _sport(sport), _dport(dport) {
    }

    /** @brief Construct a flow ID from @a p's ip_header() and udp_header().
     * @param p input packet
     * @param reverse if true, use the reverse of @a p's flow ID
     *
     * @pre @a p's ip_header() must point to a first-fragment IPv4 header, and
     * @a p's transport header should have source and destination ports in the
     * UDP-like positions; TCP, UDP, and DCCP fit the bill. */
    explicit IPFlowID(const Packet *p, bool reverse = false);

    /** @brief Construct a flow ID from @a iph and the following TCP/UDP header.
     * @param iph IP header
     * @param reverse if true, use the reverse of @a p's flow ID
     *
     * The IP header's header length, @a iph->ip_hl, is used to find the
     * following transport header.  This transport header should have source
     * and destination ports in the UDP-like positions; TCP, UDP, and DCCP fit
     * the bill. */
    explicit IPFlowID(const click_ip *iph, bool reverse = false);

    /** @brief Construct an uninitialized flow ID. */
    inline IPFlowID(const uninitialized_type &unused) {
	(void) unused;
    }


    typedef IPAddress (IPFlowID::*unspecified_bool_type)() const;
    /** @brief Return true iff the addresses of this flow ID are zero. */
    operator unspecified_bool_type() const {
	return _saddr || _daddr ? &IPFlowID::saddr : 0;
    }


    /** @brief Return this flow's source address. */
    IPAddress saddr() const {
	return _saddr;
    }
    /** @brief Return this flow's source port, in network order. */
    uint16_t sport() const {
	return _sport;
    }
    /** @brief Return this flow's destination address. */
    IPAddress daddr() const {
	return _daddr;
    }
    /** @brief Return this flow's destination port, in network order. */
    uint16_t dport() const {
	return _dport;
    }

    /** @brief Set this flow's source address to @a a. */
    void set_saddr(IPAddress a) {
	_saddr = a;
    }
    /** @brief Set this flow's source port to @a p.
     * @note @a p should be in network order. */
    void set_sport(uint16_t p) {
	_sport = p;
    }
    /** @brief Set this flow's destination address to @a a. */
    void set_daddr(IPAddress a) {
	_daddr = a;
    }
    /** @brief Set this flow's destination port to @a p.
     * @note @a p should be in network order. */
    void set_dport(uint16_t p) {
	_dport = p;
    }

    /** @brief Set this flow to the given value.
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


    /** @brief Return this flow's reverse, which swaps sources and destinations.
     * @return IPFlowID(daddr(), dport(), saddr(), sport()) */
    IPFlowID reverse() const {
	return IPFlowID(_daddr, _dport, _saddr, _sport);
    }
    inline IPFlowID rev() const CLICK_DEPRECATED;

    /** @brief Hash function.
     * @return The hash value of this IPFlowID.
     *
     * Equal IPFlowID objects always have equal hashcode() values. */
    inline hashcode_t hashcode() const;

    /** @brief Unparse this address into a String.
     *
     * Returns a string with formatted like "(SADDR, SPORT, DADDR, DPORT)". */
    String unparse() const;

    inline operator String() const CLICK_DEPRECATED;
    inline String s() const CLICK_DEPRECATED;

  protected:

    // note: several functions depend on this field order!
    IPAddress _saddr;
    IPAddress _daddr;
    uint16_t _sport;			// network byte order
    uint16_t _dport;			// network byte order

    int unparse(char *s) const;
    friend StringAccum &operator<<(StringAccum &sa, const IPFlowID &flow_id);

};


inline IPFlowID IPFlowID::rev() const
{
    return reverse();
}


#define ROT(v, r) ((v)<<(r) | ((unsigned)(v))>>(32-(r)))

inline hashcode_t IPFlowID::hashcode() const
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

inline bool operator==(const IPFlowID &a, const IPFlowID &b)
{
    return a.sport() == b.sport() && a.dport() == b.dport()
	&& a.saddr() == b.saddr() && a.daddr() == b.daddr();
}

inline bool operator!=(const IPFlowID &a, const IPFlowID &b)
{
    return a.sport() != b.sport() || a.dport() != b.dport()
	|| a.saddr() != b.saddr() || a.daddr() != b.daddr();
}

StringAccum &operator<<(StringAccum &, const IPFlowID &);

inline IPFlowID::operator String() const
{
    return unparse();
}

inline String IPFlowID::s() const
{
    return unparse();
}

CLICK_ENDDECLS
#endif
