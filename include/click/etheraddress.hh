// -*- related-file-name: "../../lib/etheraddress.cc" -*-
#ifndef CLICK_ETHERADDRESS_HH
#define CLICK_ETHERADDRESS_HH
#include <click/string.hh>
#include <click/glue.hh>
CLICK_DECLS

class EtherAddress { public:
  
    inline EtherAddress();
    explicit EtherAddress(const unsigned char *data);

    typedef bool (EtherAddress::*unspecified_bool_type)() const;
    inline operator unspecified_bool_type() const;
    inline bool is_group() const;
    inline bool is_broadcast() const;
    
    inline unsigned char *data();
    inline const unsigned char *data() const;
    inline const uint16_t *sdata() const;

    inline size_t hashcode() const;
    
    // bool operator==(EtherAddress, EtherAddress);
    // bool operator!=(EtherAddress, EtherAddress);

    String unparse() const;
    String unparse_colon() const;

    inline String s() const;
    inline operator String() const CLICK_DEPRECATED;
  
 private:
  
    uint16_t _data[3];
  
};

/** @brief Constructs an EtherAddress equal to 00-00-00-00-00-00. */
inline
EtherAddress::EtherAddress()
{
    _data[0] = _data[1] = _data[2] = 0;
}

/** @brief Constructs an EtherAddress from data.
    @param data the address data, in network byte order

    The bytes data[0]...data[5] are used to construct the address. */
inline
EtherAddress::EtherAddress(const unsigned char *data)
{
    memcpy(_data, data, 6);
}

/** @brief Returns true iff the address is not 00-00-00-00-00-00. */
inline
EtherAddress::operator unspecified_bool_type() const
{
    return _data[0] || _data[1] || _data[2] ? &EtherAddress::is_group : 0;
}

/** @brief Returns true iff this address is a group address.

    Group addresses have the low-order bit of the first byte set to 1, as in
    01-00-00-00-00-00. */
inline bool
EtherAddress::is_group() const
{
    return data()[0] & 1;
}

/** @brief Returns true iff this address is the broadcast address.

    The Ethernet broadcast address is FF-FF-FF-FF-FF-FF. */
inline bool
EtherAddress::is_broadcast() const
{
    return _data[0] == 0xFFFF && _data[1] == 0xFFFF && _data[2] == 0xFFFF;
}

/** @brief Returns a pointer to the address data. */
inline const unsigned char *
EtherAddress::data() const
{
    return reinterpret_cast<const unsigned char *>(_data);
}

/** @brief Returns a pointer to the address data. */
inline unsigned char *
EtherAddress::data()
{
    return reinterpret_cast<unsigned char *>(_data);
}

/** @brief Returns a pointer to the address data, as an array of uint16_ts. */
inline const uint16_t *
EtherAddress::sdata() const
{
    return _data;
}

/** @brief Unparses this address into a dash-separated hex String.
    @deprecated The unparse() function should be preferred to this cast.
    @sa unparse */
inline
EtherAddress::operator String() const
{
    return unparse();
}

/** @brief Unparses this address into a dash-separated hex String.
    @deprecated The unparse() function should be preferred to s().
    @sa unparse */
inline String
EtherAddress::s() const
{
    return unparse();
}

/** @relates EtherAddress
    @brief Compares two EtherAddress objects for equality. */
inline bool
operator==(const EtherAddress &a, const EtherAddress &b)
{
    return (a.sdata()[0] == b.sdata()[0]
	    && a.sdata()[1] == b.sdata()[1]
	    && a.sdata()[2] == b.sdata()[2]);
}

/** @relates EtherAddress
    @brief Compares two EtherAddress objects for inequality. */
inline bool
operator!=(const EtherAddress &a, const EtherAddress &b)
{
    return !(a == b);
}

class StringAccum;
StringAccum &operator<<(StringAccum &, const EtherAddress &);

inline size_t
EtherAddress::hashcode() const
{
    const uint16_t *d = sdata();
    return (d[2] | (d[1] << 16)) ^ (d[0] << 9);
}

CLICK_ENDDECLS
#endif
