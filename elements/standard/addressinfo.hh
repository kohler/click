#ifndef ADDRESSINFO_HH
#define ADDRESSINFO_HH
#include "element.hh"
#include "hashmap.hh"

/*
 * =c
 * AddressInfo(NAME ADDRESS [ADDRESS...], ...)
 * =s
 * specifies address information
 * =io
 * None
 * =d
 *
 * Lets you use mnemonic names for IPv4 and IPv6 addresses, IPv4 network
 * addresses, and Ethernet addresses. Each argument has the form `NAME
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
 *    AddressInfo(mauer 10.0.0.1 00:50:BA:85:84:A9);
 *    ... -> IPEncap(6, mauer, ...)                  // as IP address
 *        -> EtherEncap(0x0800, mauer, ...) -> ...   // as Ethernet address
 *
 * An optional suffix makes the context unambiguous. C<NAME> is an ambiguous
 * reference to some address, but C<NAME:ip> is always an IPv4 address,
 * C<NAME:ipnet> is always an IP network address, C<NAME:ip6> is always an
 * IPv6 address, and C<NAME:eth> is always an Ethernet address. */

class AddressInfo : public Element {

  static const unsigned INFO_IP = 1;
  static const unsigned INFO_IP_MASK = 2;
  static const unsigned INFO_IP6 = 4;
  static const unsigned INFO_ETHER = 8;
  
  struct Info {
    unsigned have;
    union {
      unsigned u;
      unsigned char c[4];
    } ip, ip_mask;
    unsigned char ip6[16];
    unsigned char ether[6];
    Info() : have(0) { }
  };
  
  HashMap<String, int> _map;
  Vector<Info> _as;

  int add_info(const Vector<String> &, const String &, ErrorHandler *);
  const Info *query(const String &, unsigned, const String &) const;
  static AddressInfo *find_element(Element *);

 public:
  
  AddressInfo();
  
  const char *class_name() const	{ return "AddressInfo"; }
  
  AddressInfo *clone() const		{ return new AddressInfo; }
  int configure_phase() const		{ return CONFIGURE_PHASE_ZERO; }
  int configure(const Vector<String> &, ErrorHandler *);

  static bool query_ip(String, unsigned char *, Element *);
  static bool query_ip_mask(String, unsigned char *, unsigned char *, Element *);
  static bool query_ip6(String, unsigned char *, Element *);
  static bool query_ethernet(String, unsigned char *, Element *);
  
};

#endif
