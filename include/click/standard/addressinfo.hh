#ifndef ADDRESSINFO_HH
#define ADDRESSINFO_HH
#include <click/element.hh>
#include <click/hashmap.hh>
#ifdef HAVE_IP6
# include <click/ip6address.hh>
#endif

/*
 * =c
 * AddressInfo(NAME ADDRESS [ADDRESS...], ...)
 * =s information
 * specifies address information
 * =io
 * None
 * =d
 *
 * Lets you use mnemonic names for IPv4 and IPv6 addresses, IPv4 and IPv6
 * address prefixes, and Ethernet addresses. Each argument has the form `NAME
 * ADDRESS [ADDRESS...]', which associates the given ADDRESSes with NAME. For
 * example, if a configuration contains this AddressInfo element,
 *
 *    AddressInfo(mauer 10.0.0.1, mazu 10.0.0.10);
 *
 * then other configuration strings can use C<mauer> and C<mazu> as mnemonics
 * for the IP addresses 10.0.0.1 and 10.0.0.10, respectively.
 *
 * The mnemonic names introduced by AddressInfo elements are local with
 * respect to compound elements. That is, names created inside a compound
 * element apply only within that compound element and its subelements. For
 * example:
 *
 *    AddressInfo(mauer 10.0.0.1);
 *    compound :: {
 *      AddressInfo(mazu 10.0.0.10);
 *      ... -> IPEncap(6, mauer, mazu) -> ...  // OK
 *    };
 *    ... -> IPEncap(6, mauer, mazu) -> ...    // error: `mazu' undefined
 *
 * Any name can be simultaneously associated with an IP address, an IP network
 * address, and an Ethernet address. The kind of address that is returned is
 * generally determined from context. For example:
 *
 *    AddressInfo(mauer 10.0.0.1/8 00:50:BA:85:84:A9);
 *    ... -> IPEncap(6, mauer, ...)                  // as IP address
 *        -> EtherEncap(0x0800, mauer, ...) -> ...   // as Ethernet address
 *    ... -> ARPResponder(mauer) -> ...              // as IP prefix AND Ethernet address!
 *
 * An optional suffix makes the context unambiguous. C<NAME> is an ambiguous
 * reference to some address, but C<NAME:ip> is always an IPv4 address,
 * C<NAME:ipnet> is always an IPv4 network address (IPv4 address prefix),
 * C<NAME:ip6> is always an IPv6 address, C<NAME:ip6net> is always an IPv6
 * network address, and C<NAME:eth> is always an Ethernet address. */

class AddressInfo : public Element { public:
  
  AddressInfo();
  ~AddressInfo();
  
  const char *class_name() const	{ return "AddressInfo"; }
  
  AddressInfo *clone() const		{ return new AddressInfo; }
  int configure_phase() const		{ return CONFIGURE_PHASE_FIRST; }
  int configure(const Vector<String> &, ErrorHandler *);

  static bool query_ip(String, unsigned char *, Element *);
  static bool query_ip_prefix(String, unsigned char *, unsigned char *, Element *);
#ifdef HAVE_IP6
  static bool query_ip6(String, unsigned char *, Element *);
  static bool query_ip6_prefix(String, unsigned char *, int *, Element *);
#endif
  static bool query_ethernet(String, unsigned char *, Element *);

 private:

  static const unsigned INFO_IP = 1;
  static const unsigned INFO_IP_PREFIX = 2;
  static const unsigned INFO_IP6 = 4;
  static const unsigned INFO_IP6_PREFIX = 8;
  static const unsigned INFO_ETHER = 16;
  
  struct Info {
    unsigned have;
    union {
      unsigned u;
      unsigned char c[4];
    } ip, ip_mask;
#ifdef HAVE_IP6
    IP6Address ip6;
    int ip6_prefix;
#endif
    unsigned char ether[6];
    Info() : have(0) { }
  };
  
  HashMap<String, int> _map;
  Vector<Info> _as;

  int add_info(const Vector<String> &, const String &, ErrorHandler *);
  const Info *query(const String &, unsigned, const String &) const;
  static AddressInfo *find_element(Element *);
  
};

#endif
