// -*- related-file-name: "../../lib/ip6address.cc" -*-
#ifndef CLICK_IP6ADDRESS_HH
#define CLICK_IP6ADDRESS_HH
#include <click/string.hh>
#include <clicknet/ip6.h>
#include <click/ipaddress.hh>
#include <click/etheraddress.hh>
#if !CLICK_TOOL
# include <click/packet.hh>
# include <click/packet_anno.hh>
#endif
CLICK_DECLS

class IP6Address { public:

    typedef uninitialized_type uninitialized_t;

    /** @brief Construct a zero-valued IP6Address (equal to ::). */
    inline IP6Address() {
	memset(&_addr, 0, sizeof(_addr));
    }

    /** @brief Construct an IP6Address from a sixteen-byte buffer. */
    explicit inline IP6Address(const unsigned char *x) {
	memcpy(&_addr, x, sizeof(_addr));
    }

    /** @brief Construct an IP6Address that represents an IPAddress.
     *
     * The address has format ::@a x. */
    explicit inline IP6Address(IPAddress x) {
	memset(&_addr, 0, 12);
	data32()[3] = x.addr();
    }

    /** @brief Construct an IP6Address from a human-readable string. */
    explicit IP6Address(const String &x);		// "fec0:0:0:1::1"

    /** @brief Construct an IP6Address from a C structure. */
    explicit inline IP6Address(const struct in6_addr &x)
	: _addr(x) {
    }

    /** @brief Construct an uninitialized IP6Address. */
    inline IP6Address(const uninitialized_type &unused) {
	(void) unused;
    }

    /** @brief Return an IP6Address equal to the prefix mask of length @a
     * prefix_len.
     * @param prefix_len prefix length; 0 <= @a prefix_len <= 128
     *
     * For example, make_prefix(0) is ::, make_prefix(12) is fff0::, and
     * make_prefix(128) is ffff:ffff:ffff:ffff:ffff:ffff:ffff:ffff.  Causes an
     * assertion failure if @a prefix_len is out of range.
     *
     * @sa mask_to_prefix_len, make_inverted_prefix */
    static IP6Address make_prefix(int prefix_len);

    /** @brief Return an IP6Address equal to the inversion of make_prefix(@a
     * prefix_len).
     * @param prefix_len prefix length; 0 <= @a prefix_len <= 128
     * @return ~make_prefix(@a prefix_len)
     * @sa make_prefix */
    static IP6Address make_inverted_prefix(int prefix_len);

    typedef uint32_t (IP6Address::*unspecified_bool_type)() const;
    inline operator unspecified_bool_type() const;

    operator const struct in6_addr &() const	{ return _addr; }
    operator struct in6_addr &()			{ return _addr; }
    const struct in6_addr &in6_addr() const	{ return _addr;	}
    struct in6_addr &in6_addr()			{ return _addr;	}

    unsigned char *data()			{ return &_addr.s6_addr[0]; }
    const unsigned char *data() const		{ return &_addr.s6_addr[0]; }
    uint16_t *data16()				{ return (uint16_t *)&_addr.s6_addr[0]; }
    const uint16_t *data16() const		{ return (uint16_t *)&_addr.s6_addr[0]; }
    uint32_t *data32()				{ return (uint32_t *)&_addr.s6_addr[0]; }
    const uint32_t *data32() const		{ return (uint32_t *)&_addr.s6_addr[0]; }

    inline uint32_t hashcode() const;

    int mask_to_prefix_len() const;
    inline bool matches_prefix(const IP6Address &addr, const IP6Address &mask) const;
    inline bool mask_as_specific(const IP6Address &) const;

    /** @brief Test if this address contains an embedded Ethernet address.
     *
     * An IPv6 address with embedded Ethernet address has format
     * "nnnn:nnnn:nnnn:nnnn:uuvv:wwFF:FExx:yyzz", where the embedded Ethernet
     * address is "uu-vv-ww-xx-yy-zz". */
    bool has_ether_address() const {
	return _addr.s6_addr[11] == 0xFF && _addr.s6_addr[12] == 0xFE;
    }

    /** @brief Extract embedded Ethernet address into @a x.
     * @param[out] x Ethernet address
     * @return true iff has_ether_address() */
    bool ether_address(EtherAddress &x) const;

    /** @brief Test if this address contains an embedded IPv4 address.
     *
     * An IPv6 address with embedded IPv4 address has format either
     * "::w.x.y.z" or "::FFFF:w.x.y.z", where the embedded IPv4 address is
     * "w.x.y.z". */
    bool has_ip4_address() const {
	return data32()[0] == 0 && data32()[1] == 0
	    && (data32()[2] == 0 || data32()[2] == htonl(0x0000FFFFU));
    }

    /** @brief Return true iff the address is a link-local address.
     * fe80::/64
     *
     */
    inline bool is_link_local() const {
        return data32()[0] == htonl(0xfe800000) && data32()[1] == 0;
    }

    /** @brief Extract embedded IPv4 address into @a x.
     * @param[out] x IPv4 address
     * @return true iff has_ip4_address() */
    bool ip4_address(IPAddress &x) const;

    // bool operator==(const IP6Address &, const IP6Address &);
    // bool operator!=(const IP6Address &, const IP6Address &);

    // IP6Address operator&(const IP6Address &, const IP6Address &);
    // IP6Address operator|(const IP6Address &, const IP6Address &);
    // IP6Address operator~(const IP6Address &);

    inline IP6Address &operator&=(const IP6Address &);
    inline IP6Address &operator&=(const struct in6_addr &);
    inline IP6Address &operator|=(const IP6Address &);
    inline IP6Address &operator|=(const struct in6_addr &);

    inline IP6Address &operator=(const struct in6_addr &);

    void unparse(StringAccum &sa) const;
    String unparse() const;
    String unparse_expanded() const;

    String s() const			{ return unparse(); }
    inline operator String() const CLICK_DEPRECATED;

    typedef const IP6Address &parameter_type;

  private:

    struct in6_addr _addr;

};

inline
IP6Address::operator unspecified_bool_type() const
{
    const uint32_t *ai = data32();
    return ai[0] || ai[1] || ai[2] || ai[3] ? &IP6Address::hashcode : 0;
}

inline
IP6Address::operator String() const
{
    return unparse();
}

inline bool
operator==(const IP6Address &a, const IP6Address &b)
{
    const uint32_t *ai = a.data32(), *bi = b.data32();
    return ai[0] == bi[0] && ai[1] == bi[1] && ai[2] == bi[2] && ai[3] == bi[3];
}

inline bool
operator!=(const IP6Address &a, const IP6Address &b)
{
    const uint32_t *ai = a.data32(), *bi = b.data32();
    return ai[0] != bi[0] || ai[1] != bi[1] || ai[2] != bi[2] || ai[3] != bi[3];
}

inline StringAccum &
operator<<(StringAccum &sa, const IP6Address &a) {
    a.unparse(sa);
    return sa;
}

inline bool
IP6Address::matches_prefix(const IP6Address &addr, const IP6Address &mask) const
{
    const uint32_t *xi = data32(), *ai = addr.data32(), *mi = mask.data32();
    return ((xi[0] ^ ai[0]) & mi[0]) == 0
	&& ((xi[1] ^ ai[1]) & mi[1]) == 0
	&& ((xi[2] ^ ai[2]) & mi[2]) == 0
	&& ((xi[3] ^ ai[3]) & mi[3]) == 0;
}

inline bool
IP6Address::mask_as_specific(const IP6Address &mask) const
{
    const uint32_t *xi = data32(), *mi = mask.data32();
    return ((xi[0] & mi[0]) == mi[0] && (xi[1] & mi[1]) == mi[1]
	    && (xi[2] & mi[2]) == mi[2] && (xi[3] & mi[3]) == mi[3]);
}

inline IP6Address &
IP6Address::operator&=(const IP6Address &x)
{
    uint32_t *ai = data32();
    const uint32_t *bi = x.data32();
    ai[0] &= bi[0];
    ai[1] &= bi[1];
    ai[2] &= bi[2];
    ai[3] &= bi[3];
    return *this;
}

inline IP6Address &
IP6Address::operator&=(const struct in6_addr &x)
{
    uint32_t *ai = data32();
    const uint32_t *bi = (const uint32_t *)&x.s6_addr[0];
    ai[0] &= bi[0];
    ai[1] &= bi[1];
    ai[2] &= bi[2];
    ai[3] &= bi[3];
    return *this;
}

inline IP6Address &
IP6Address::operator|=(const IP6Address &x)
{
    uint32_t *ai = data32();
    const uint32_t *bi = x.data32();
    ai[0] |= bi[0];
    ai[1] |= bi[1];
    ai[2] |= bi[2];
    ai[3] |= bi[3];
    return *this;
}

inline IP6Address &
IP6Address::operator|=(const struct in6_addr &x)
{
    uint32_t *ai = data32();
    const uint32_t *bi = (const uint32_t *)&x.s6_addr[0];
    ai[0] |= bi[0];
    ai[1] |= bi[1];
    ai[2] |= bi[2];
    ai[3] |= bi[3];
    return *this;
}

inline IP6Address
operator&(const IP6Address &a, const IP6Address &b)
{
    const uint32_t *ai = a.data32(), *bi = b.data32();
    IP6Address result = IP6Address::uninitialized_t();
    uint32_t *ri = result.data32();
    ri[0] = ai[0] & bi[0];
    ri[1] = ai[1] & bi[1];
    ri[2] = ai[2] & bi[2];
    ri[3] = ai[3] & bi[3];
    return result;
}

inline IP6Address
operator&(const struct in6_addr &a, const IP6Address &b)
{
    const uint32_t *ai = (const uint32_t *)&a.s6_addr[0], *bi = b.data32();
    IP6Address result = IP6Address::uninitialized_t();
    uint32_t *ri = result.data32();
    ri[0] = ai[0] & bi[0];
    ri[1] = ai[1] & bi[1];
    ri[2] = ai[2] & bi[2];
    ri[3] = ai[3] & bi[3];
    return result;
}

inline IP6Address
operator|(const IP6Address &a, const IP6Address &b)
{
    const uint32_t *ai = a.data32(), *bi = b.data32();
    IP6Address result = IP6Address::uninitialized_t();
    uint32_t *ri = result.data32();
    ri[0] = ai[0] | bi[0];
    ri[1] = ai[1] | bi[1];
    ri[2] = ai[2] | bi[2];
    ri[3] = ai[3] | bi[3];
    return result;
}

inline IP6Address
operator~(const IP6Address &x)
{
    const uint32_t *ai = x.data32();
    IP6Address result = IP6Address::uninitialized_t();
    uint32_t *ri = result.data32();
    ri[0] = ~ai[0];
    ri[1] = ~ai[1];
    ri[2] = ~ai[2];
    ri[3] = ~ai[3];
    return result;
}

inline IP6Address &
IP6Address::operator=(const struct in6_addr &a)
{
    _addr = a;
    return *this;
}

inline uint32_t
IP6Address::hashcode() const
{
    return (data32()[2] << 1) + data32()[3];
}

#if !CLICK_TOOL
inline const IP6Address &
DST_IP6_ANNO(Packet *p)
{
    return *reinterpret_cast<IP6Address *>(p->anno_u8() + DST_IP6_ANNO_OFFSET);
}

inline void
SET_DST_IP6_ANNO(Packet *p, const IP6Address &a)
{
    memcpy(p->anno_u8() + DST_IP6_ANNO_OFFSET, a.data(), DST_IP6_ANNO_SIZE);
}

inline void
SET_DST_IP6_ANNO(Packet *p, const struct in6_addr &a)
{
    memcpy(p->anno_u8() + DST_IP6_ANNO_OFFSET, &a, DST_IP6_ANNO_SIZE);
}
#endif


/** @class IP6AddressArg
  @brief Parser class for IPv6 addresses. */
struct IP6AddressArg {
    static const char *basic_parse(const String &str, IP6Address &result,
				   const ArgContext &args = blank_args);
    static bool parse(const String &str, IP6Address &result,
		      const ArgContext &args = blank_args);
};

/** @class IP6PrefixArg
  @brief Parser class for IPv6 address prefixes. */
class IP6PrefixArg { public:
    IP6PrefixArg(bool allow_bare_address_ = false)
	: allow_bare_address(allow_bare_address_) {
    }
    bool parse(const String &str, IP6Address &addr, int &prefix_len,
	       const ArgContext &args = blank_args) const;
    bool parse(const String &str, IP6Address &addr, IP6Address &prefix,
	       const ArgContext &args = blank_args) const;
    bool allow_bare_address;
};

template<> struct DefaultArg<IP6Address> : public IP6AddressArg {};
template<> struct has_trivial_copy<IP6Address> : public true_type {};

CLICK_ENDDECLS
#endif
