// -*- c-basic-offset: 4; related-file-name: "../../lib/ipaddress.cc" -*-
#ifndef CLICK_IPADDRESS_HH
#define CLICK_IPADDRESS_HH
#include <click/string.hh>
#include <click/glue.hh>
#include <clicknet/ip.h>
CLICK_DECLS
class StringAccum;

class IPAddress { public:
  
    /** @brief Constructs an IPAddress equal to 0.0.0.0. */
    inline IPAddress()
	: _addr(0) {
    }

    /** @brief Constructs an IPAddress from an integer in network byte order.
     * @param a the address, in network byte order */
    inline IPAddress(unsigned int a)
	: _addr(a) {
    }

    /** @brief Constructs an IPAddress from an integer in network byte order.
     * @param a the address, in network byte order */
    explicit inline IPAddress(int a)
	: _addr(a) {
    }

    /** @brief Constructs an IPAddress from an integer in network byte order.
     * @param a the address, in network byte order */
    explicit inline IPAddress(unsigned long a)
	: _addr(a) {
    }

    /** @brief Constructs an IPAddress from an integer in network byte order.
     * @param a the address, in network byte order */
    explicit inline IPAddress(long a)
	: _addr(a) {
    }

    /** @brief Constructs an IPAddress from a struct in_addr.
     * @param ina the address */
    inline IPAddress(struct in_addr ina)
	: _addr(ina.s_addr) {
    }

    explicit IPAddress(const unsigned char*);
    explicit IPAddress(const String&);	// "18.26.4.99"

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


    typedef uint32_t (IPAddress::*unspecified_bool_type)() const;
    /** @brief Returns true iff the address is not 0.0.0.0. */
    inline operator unspecified_bool_type() const {
	return _addr != 0 ? &IPAddress::addr : 0;
    }

    /** @brief Return true iff the address is a multicast address.
     *
     * These are the class D addresses, 224.0.0.0-239.255.255.255. */
    inline bool is_multicast() const {
	return (_addr & htonl(0xF0000000)) == htonl(0xE0000000);
    }

    inline uint32_t addr() const;
    inline operator uint32_t() const;
  
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

  private:
  
    uint32_t _addr;

};


/** @brief Returns the address as a uint32_t in network byte order. */
inline
IPAddress::operator uint32_t() const
{
    return _addr;
}

/** @brief Returns the address as a uint32_t in network byte order. */
inline uint32_t
IPAddress::addr() const
{
    return _addr;
}

/** @relates IPAddress
    @brief Compares two IPAddress objects for equality. */
inline bool
operator==(IPAddress a, IPAddress b)
{
    return a.addr() == b.addr();
}

/** @relates IPAddress
    @brief Compares an IPAddress with a network-byte-order address value for
    equality.
    @param a an address
    @param b an address value in network byte order */
inline bool
operator==(IPAddress a, uint32_t b)
{
    return a.addr() == b;
}

/** @relates IPAddress
    @brief Compares two IPAddress objects for inequality. */
inline bool
operator!=(IPAddress a, IPAddress b)
{
    return a.addr() != b.addr();
}

/** @relates IPAddress
    @brief Compares an IPAddress with a network-byte-order address value for
    inequality.
    @param a an address
    @param b an address value in network byte order */
inline bool
operator!=(IPAddress a, uint32_t b)
{
    return a.addr() != b;
}

/** @brief Returns a pointer to the address data.
    
    Since the address is stored in network byte order, data()[0] is the top 8
    bits of the address, data()[1] the next 8 bits, and so forth. */
inline const unsigned char*
IPAddress::data() const
{
    return reinterpret_cast<const unsigned char*>(&_addr);
}

/** @brief Returns a pointer to the address data.
    
    Since the address is stored in network byte order, data()[0] is the top 8
    bits of the address, data()[1] the next 8 bits, and so forth. */
inline unsigned char*
IPAddress::data()
{
    return reinterpret_cast<unsigned char*>(&_addr);
}

/** @brief Returns a struct in_addr corresponding to the address. */
inline struct in_addr
IPAddress::in_addr() const
{
    struct in_addr ia;
    ia.s_addr = _addr;
    return ia;
}

/** @brief Returns a struct in_addr corresponding to the address. */
inline
IPAddress::operator struct in_addr() const
{
    return in_addr();
}

StringAccum& operator<<(StringAccum&, IPAddress);

/** @brief Returns true iff this address matches the address prefix
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

/** @brief Returns true iff this address, considered as a prefix mask, is at
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

/** @brief Returns true iff this prefix mask is more specific than @a mask.
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
    @brief Calculates the IPAddress representing the bitwise-and of @a a and
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
    @brief Calculates the IPAddress representing the bitwise-or of @a a and
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
    @brief Calculates the IPAddress representing the bitwise-xor of @a a and
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
    @brief Calculates the IPAddress representing the bitwise complement
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

/** @brief Unparses this address into a dotted-quad format String.
    @deprecated The unparse() function should be preferred to this cast.
    @sa unparse */
inline
IPAddress::operator String() const
{
    return unparse();
}

/** @brief Unparses this address into a dotted-quad format String.
    @deprecated The unparse() function should be preferred to s().
    @sa unparse */
inline String
IPAddress::s() const
{
    return unparse();
}

CLICK_ENDDECLS
#endif
