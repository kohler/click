// -*- related-file-name: "../../lib/etheraddress.cc" -*-
#ifndef CLICK_ETHERADDRESS_HH
#define CLICK_ETHERADDRESS_HH
#include <click/string.hh>
#include <click/glue.hh>
#include <click/type_traits.hh>
CLICK_DECLS

class EtherAddress { public:

    typedef uninitialized_type uninitialized_t;

    /** @brief Construct an EtherAddress equal to 00-00-00-00-00-00. */
    inline EtherAddress() {
	_data[0] = _data[1] = _data[2] = 0;
    }

    /** @brief Construct an EtherAddress from data.
     * @param data the address data, in network byte order
     *
     * The bytes data[0]...data[5] are used to construct the address. */
    explicit inline EtherAddress(const unsigned char *data) {
	memcpy(_data, data, 6);
    }

    /** @brief Construct an uninitialized EtherAddress. */
    inline EtherAddress(const uninitialized_type &unused) {
	(void) unused;
    }

    /** @brief Return the broadcast EtherAddress, FF-FF-FF-FF-FF-FF. */
    static EtherAddress make_broadcast() {
	return EtherAddress(0xFFFF);
    }

    static inline EtherAddress broadcast() CLICK_DEPRECATED;


    typedef bool (EtherAddress::*unspecified_bool_type)() const;
    /** @brief Return true iff the address is not 00-00-00-00-00-00. */
    inline operator unspecified_bool_type() const {
	return _data[0] || _data[1] || _data[2] ? &EtherAddress::is_group : 0;
    }

    /** @brief Return true iff this address is a group address.
     *
     * Group addresses have the low-order bit of the first byte set to 1, as
     * in 01-00-00-00-00-00 or 03-00-00-02-04-09. */
    inline bool is_group() const {
	return data()[0] & 1;
    }

    /** @brief Return true iff this address is a "local" address.
     *
     * Local addresses have the next-to-lowest-order bit of the first byte set
     * to 1. */
    inline bool is_local() const {
	return data()[0] & 2;
    }

    /** @brief Return true iff this address is the broadcast address.
     *
     * The Ethernet broadcast address is FF-FF-FF-FF-FF-FF. */
    inline bool is_broadcast() const {
	return _data[0] + _data[1] + _data[2] == 0x2FFFD;
    }

    /** @brief Return true if @a data points to a broadcast address. */
    static inline bool is_broadcast(const unsigned char *data) {
#if HAVE_INDIFFERENT_ALIGNMENT
	return reinterpret_cast<const EtherAddress *>(data)->is_broadcast();
#else
	return data[0] + data[1] + data[2] + data[3] + data[4] + data[5] == 0x5FA;
#endif
    }

    /** @brief Return a pointer to the address data. */
    inline unsigned char *data() {
	return reinterpret_cast<unsigned char *>(_data);
    }

    /** @overload */
    inline const unsigned char *data() const {
	return reinterpret_cast<const unsigned char *>(_data);
    }

    /** @brief Return a pointer to the address data, as an array of
     * uint16_ts. */
    inline const uint16_t *sdata() const {
	return _data;
    }

    /** @brief Hash function. */
    inline size_t hashcode() const {
	return (_data[2] | ((size_t) _data[1] << 16))
	    ^ ((size_t) _data[0] << 9);
    }

    // bool operator==(EtherAddress, EtherAddress);
    // bool operator!=(EtherAddress, EtherAddress);

    /** @brief Unparse this address into a dash-separated hex String.
     *
     * Examples include "00-00-00-00-00-00" and "00-05-4E-50-3C-1A".
     *
     * @note The IEEE standard for printing Ethernet addresses uses dashes as
     * separators, not colons.  Use unparse_colon() to unparse into the
     * nonstandard colon-separated form. */
    inline String unparse() const {
	return unparse_dash();
    }

    /** @brief Unparse this address into a colon-separated hex String.
     *
     * Examples include "00:00:00:00:00:00" and "00:05:4E:50:3C:1A".
     *
     * @note Use unparse() to create the IEEE standard dash-separated form. */
    String unparse_colon() const;

    /** @brief Unparse this address into a dash-separated hex String.
     *
     * Examples include "00-00-00-00-00-00" and "00-05-4E-50-3C-1A".
     *
     * @note This is the IEEE standard for printing Ethernet addresses.
     * @sa unparse_colon */
    String unparse_dash() const;

    /** @brief Unparse this address into a dash-separated hex String.
     * @deprecated The unparse() function should be preferred to s().
     * @sa unparse */
    inline String s() const CLICK_DEPRECATED;

    /** @brief Unparse this address into a dash-separated hex String.
     * @deprecated The unparse() function should be preferred to this cast.
     * @sa unparse */
    inline operator String() const CLICK_DEPRECATED;

    typedef const EtherAddress &parameter_type;

 private:

    uint16_t _data[3];

    EtherAddress(uint16_t m) {
	_data[0] = _data[1] = _data[2] = m;
    }

} CLICK_SIZE_PACKED_ATTRIBUTE;

inline EtherAddress EtherAddress::broadcast() {
    return make_broadcast();
}

inline
EtherAddress::operator String() const
{
    return unparse();
}

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


class ArgContext;
class Args;
extern const ArgContext blank_args;

/** @class EtherAddressArg
  @brief Parser class for Ethernet addresses.

  This is the default parser for objects of EtherAddress type.  For 6-byte
  arrays like "click_ether::ether_shost" and "click_ether::ether_dhost", it is
  necessary use Args::read_with:

  @code
  struct click_ether ethh;
  ... Args(...) ...
      .read_mp_with("SRC", EtherAddressArg(), ethh.ether_shost)
      ...
  @endcode */
class EtherAddressArg { public:
    typedef void enable_direct_parse;
    static bool parse(const String &str, EtherAddress &value, const ArgContext &args = blank_args);
    static bool parse(const String &str, unsigned char *value, const ArgContext &args = blank_args) {
	return parse(str, *reinterpret_cast<EtherAddress *>(value), args);
    }
    static bool direct_parse(const String &str, EtherAddress &value, Args &args);
    static bool direct_parse(const String &str, unsigned char *value, Args &args) {
	return direct_parse(str, *reinterpret_cast<EtherAddress *>(value), args);
    }
};

template<> struct DefaultArg<EtherAddress> : public EtherAddressArg {};
template<> struct has_trivial_copy<EtherAddress> : public true_type {};

CLICK_ENDDECLS
#endif
