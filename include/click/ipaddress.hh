// -*- c-basic-offset: 4; related-file-name: "../../lib/ipaddress.cc" -*-
#ifndef CLICK_IPADDRESS_HH
#define CLICK_IPADDRESS_HH
#include <click/string.hh>
#include <click/glue.hh>
#include <click/type_traits.hh>
#include <clicknet/ip.h>
CLICK_DECLS
class StringAccum;
class ArgContext;
extern const ArgContext blank_args;
class IPAddressArg;
template <typename T> class Vector;

class IPAddress { public:

    struct uninitialized_t {
    };

    /** @brief Construct an IPAddress equal to 0.0.0.0. */
    inline IPAddress()
	: _addr(0) {
    }

    /** @brief Construct an IPAddress from an integer in network byte order. */
    inline IPAddress(unsigned x)
	: _addr(x) {
    }
    /** @overload */
    explicit inline IPAddress(int x)
	: _addr(x) {
    }
    /** @overload */
    explicit inline IPAddress(unsigned long x)
	: _addr(x) {
    }
    /** @overload */
    explicit inline IPAddress(long x)
	: _addr(x) {
    }

    /** @brief Construct an IPAddress from a struct in_addr. */
    inline IPAddress(struct in_addr x)
	: _addr(x.s_addr) {
    }

    /** @brief Construct an IPAddress from data.
     * @param data the address data, in network byte order
     *
     * Bytes data[0]...data[3] are used to construct the address. */
    explicit IPAddress(const unsigned char *data) {
#if HAVE_INDIFFERENT_ALIGNMENT
	_addr = *(reinterpret_cast<const unsigned *>(data));
#else
	memcpy(&_addr, data, 4);
#endif
    }

    /** @brief Constructs an IPAddress from a human-readable dotted-quad
     * representation.
     *
     * If @a x is not a valid dotted-quad address, then the IPAddress is
     * initialized to 0.0.0.0. */
    explicit IPAddress(const String &x);

    /** @brief Construct an uninitialized IPAddress. */
    inline IPAddress(const uninitialized_t &unused) {
	(void) unused;
    }

    /** @brief Return an IPAddress equal to the prefix mask of length @a
     * prefix.
     * @param prefix_len prefix length; 0 <= @a prefix_len <= 32
     *
     * For example, make_prefix(0) is 0.0.0.0, make_prefix(8) is 255.0.0.0, and
     * make_prefix(32) is 255.255.255.255.  Causes an assertion failure if @a
     * prefix_len is out of range.
     * @sa mask_to_prefix_len */
    static IPAddress make_prefix(int prefix_len);

    /** @brief Return the broadcast IP address, 255.255.255.255. */
    static inline IPAddress make_broadcast() {
	return IPAddress(0xFFFFFFFF);
    }

    /** @brief Test if the address is 0.0.0.0. */
    inline bool empty() const {
	return !_addr;
    }

    /** @brief Return the address as a uint32_t in network byte order. */
    inline uint32_t addr() const {
	return _addr;
    }

    /** @brief Return the address as a uint32_t in network byte order.
     *
     * Also suitable for use as an operator bool, returning true iff
     * the address is not 0.0.0.0. */
    inline operator uint32_t() const {
	return _addr;
    }

    /** @brief Return true iff the address is a multicast address.
     *
     * These are the class D addresses, 224.0.0.0-239.255.255.255. */
    inline bool is_multicast() const {
	return (_addr & htonl(0xF0000000U)) == htonl(0xE0000000U);
    }

    inline struct in_addr in_addr() const;
    inline operator struct in_addr() const;

    inline unsigned char* data();
    inline const unsigned char* data() const;

    inline uint32_t hashcode() const;

    int mask_to_prefix_len() const;
    inline bool matches_prefix(IPAddress addr, IPAddress mask) const;
    inline bool mask_as_specific(IPAddress mask) const;
    inline bool mask_more_specific(IPAddress mask) const;

    // bool operator==(IPAddress, IPAddress);
    // bool operator==(IPAddress, uint32_t);
    // bool operator!=(IPAddress, IPAddress);
    // bool operator!=(IPAddress, uint32_t);

    // IPAddress operator&(IPAddress, IPAddress);
    // IPAddress operator|(IPAddress, IPAddress);
    // IPAddress operator^(IPAddress, IPAddress);
    // IPAddress operator~(IPAddress);

    inline IPAddress& operator&=(IPAddress);
    inline IPAddress& operator|=(IPAddress);
    inline IPAddress& operator^=(IPAddress);

    String unparse() const;
    String unparse_mask() const;
    String unparse_with_mask(IPAddress) const;

    inline String s() const;
    inline operator String() const CLICK_DEPRECATED;

    typedef IPAddress parameter_type;

  private:

    uint32_t _addr;

};


/** @relates IPAddress
    @brief Compare two IPAddress objects for equality. */
inline bool
operator==(IPAddress a, IPAddress b)
{
    return a.addr() == b.addr();
}

/** @relates IPAddress
    @brief Compare an IPAddress with a network-byte-order address value for
    equality.
    @param a an address
    @param b an address value in network byte order */
inline bool
operator==(IPAddress a, uint32_t b)
{
    return a.addr() == b;
}

/** @relates IPAddress
    @brief Compare two IPAddress objects for inequality. */
inline bool
operator!=(IPAddress a, IPAddress b)
{
    return a.addr() != b.addr();
}

/** @relates IPAddress
    @brief Compare an IPAddress with a network-byte-order address value for
    inequality.
    @param a an address
    @param b an address value in network byte order */
inline bool
operator!=(IPAddress a, uint32_t b)
{
    return a.addr() != b;
}

/** @brief Return a pointer to the address data.

    Since the address is stored in network byte order, data()[0] is the top 8
    bits of the address, data()[1] the next 8 bits, and so forth. */
inline const unsigned char*
IPAddress::data() const
{
    return reinterpret_cast<const unsigned char*>(&_addr);
}

/** @brief Return a pointer to the address data.

    Since the address is stored in network byte order, data()[0] is the top 8
    bits of the address, data()[1] the next 8 bits, and so forth. */
inline unsigned char*
IPAddress::data()
{
    return reinterpret_cast<unsigned char*>(&_addr);
}

/** @brief Return a struct in_addr corresponding to the address. */
inline struct in_addr
IPAddress::in_addr() const
{
    struct in_addr ia;
    ia.s_addr = _addr;
    return ia;
}

/** @brief Return a struct in_addr corresponding to the address. */
inline
IPAddress::operator struct in_addr() const
{
    return in_addr();
}

StringAccum& operator<<(StringAccum&, IPAddress);

/** @brief Return true iff this address matches the address prefix
    @a addr/@a mask.
    @param addr prefix address
    @param mask prefix mask

    Equivalent to (@a addr & @a mask) == (*this & @a mask).  The prefix address
    @a addr may be nonzero outside the @a mask. */
inline bool
IPAddress::matches_prefix(IPAddress addr, IPAddress mask) const
{
    return ((this->addr() ^ addr.addr()) & mask.addr()) == 0;
}

/** @brief Return true iff this address, considered as a prefix mask, is at
    least as specific as @a mask.
    @param mask prefix mask

    Longer prefix masks are more specific than shorter ones.  For example,
    make_prefix(20).mask_as_specific(make_prefix(18)) is true, but
    make_prefix(10).mask_as_specific(make_prefix(14)) is false.

    Equivalent to (*this & @a mask) == @a mask. */
inline bool
IPAddress::mask_as_specific(IPAddress mask) const
{
    return (addr() & mask.addr()) == mask.addr();
}

/** @brief Return true iff this prefix mask is more specific than @a mask.
    @param mask prefix mask

    Both this address and @a mask must be prefix masks -- i.e.,
    mask_to_prefix_len() returns 0-32.  Returns true iff this address contains
    a longer prefix than @a mask.  For example,
    make_prefix(20).mask_more_specific(make_prefix(18)) is true, but
    make_prefix(20).mask_more_specific(make_prefix(20)) is false. */
inline bool
IPAddress::mask_more_specific(IPAddress mask) const
{
    return ((addr() << 1) & mask.addr()) == mask.addr();
}

/** @relates IPAddress
    @brief Calculate the IPAddress representing the bitwise-and of @a a and
    @a b. */
inline IPAddress
operator&(IPAddress a, IPAddress b)
{
    return IPAddress(a.addr() & b.addr());
}

/** @brief Assign this address to its bitwise-and with @a a. */
inline IPAddress&
IPAddress::operator&=(IPAddress a)
{
    _addr &= a._addr;
    return *this;
}

/** @relates IPAddress
    @brief Calculate the IPAddress representing the bitwise-or of @a a and
    @a b. */
inline IPAddress
operator|(IPAddress a, IPAddress b)
{
    return IPAddress(a.addr() | b.addr());
}

/** @brief Assign this address to its bitwise-or with @a a. */
inline IPAddress&
IPAddress::operator|=(IPAddress a)
{
    _addr |= a._addr;
    return *this;
}

/** @relates IPAddress
    @brief Calculate the IPAddress representing the bitwise-xor of @a a and
    @a b. */
inline IPAddress
operator^(IPAddress a, IPAddress b)
{
    return IPAddress(a.addr() ^ b.addr());
}

/** @brief Assign this address to its bitwise-xor with @a a. */
inline IPAddress&
IPAddress::operator^=(IPAddress a)
{
    _addr ^= a._addr;
    return *this;
}

/** @relates IPAddress
    @brief Calculate the IPAddress representing the bitwise complement
    of @a a. */
inline IPAddress
operator~(IPAddress a)
{
    return IPAddress(~a.addr());
}

/** @brief Hash function.
 * @return The hash value of this IPAddress.
 *
 * Equal IPAddress objects always have equal hashcode() values.
 */
inline uint32_t
IPAddress::hashcode() const
{
    return addr();
}

/** @brief Unparse this address into a dotted-quad format String.
    @deprecated The unparse() function should be preferred to this cast.
    @sa unparse */
inline
IPAddress::operator String() const
{
    return unparse();
}

/** @brief Unparse this address into a dotted-quad format String.
    @deprecated The unparse() function should be preferred to s().
    @sa unparse */
inline String
IPAddress::s() const
{
    return unparse();
}


/** @class IPAddressArg
  @brief Parser class for IPv4 addresses. */
class IPAddressArg { public:
    static const char *basic_parse(const char *begin, const char *end,
				   unsigned char value[4], int &nbytes);
    static bool parse(const String &str, IPAddress &result,
		      const ArgContext &args = blank_args);
    static bool parse(const String &str, struct in_addr &result,
		      const ArgContext &args = blank_args) {
	return parse(str, reinterpret_cast<IPAddress &>(result), args);
    }
    static bool parse(const String &str, Vector<IPAddress> &result,
		      const ArgContext &args = blank_args);
};

/** @class IPPrefixArg
  @brief Parser class for IPv4 prefixes. */
class IPPrefixArg { public:
    IPPrefixArg(bool allow_bare_address_ = false)
	: allow_bare_address(allow_bare_address_) {
    }
    bool parse(const String &str,
	       IPAddress &result_addr, IPAddress &result_mask,
	       const ArgContext &args = blank_args) const;
    bool parse(const String &str,
	       struct in_addr &result_addr, struct in_addr &result_mask,
	       const ArgContext &args = blank_args) const {
	return parse(str, reinterpret_cast<IPAddress &>(result_addr),
		     reinterpret_cast<IPAddress &>(result_mask), args);
    }
    bool allow_bare_address;
};

template<> struct DefaultArg<IPAddress> : public IPAddressArg {};
template<> struct DefaultArg<struct in_addr> : public IPAddressArg {};
template<> struct DefaultArg<Vector<IPAddress> > : public IPAddressArg {};
/* template<> struct has_trivial_copy<IPAddress> : public true_type {}; -- in type_traits.hh */


/** @class IPPortArg
  @brief Parser class for TCP/UDP ports.

  The constructor argument is the relevant IP protocol. */
class IPPortArg { public:
    IPPortArg(int p)
	: ip_p(p) {
	assert(ip_p > 0 && ip_p < 256);
    }
    bool parse(const String &str, uint16_t &result,
	       const ArgContext &args = blank_args) const;
    int ip_p;
};

CLICK_ENDDECLS
#endif
