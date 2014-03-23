// -*- related-file-name: "../../../elements/standard/addressinfo.cc" -*-
#ifndef CLICK_ADDRESSINFO_HH
#define CLICK_ADDRESSINFO_HH
#include <click/element.hh>
CLICK_DECLS

/*
=c

AddressInfo(NAME ADDRESS [ADDRESS...], ...)

=s information

specifies address information

=io

None

=d

Lets you use mnemonic names for IPv4 and IPv6 addresses, IPv4 and IPv6
address prefixes, and Ethernet addresses. Each argument has the form `NAME
ADDRESS [ADDRESS...]', which associates the given ADDRESSes with NAME. For
example, if a configuration contains this AddressInfo element,

   AddressInfo(mauer 10.0.0.1, mazu 10.0.0.10);

then other configuration strings can use C<mauer> and C<mazu> as mnemonics
for the IP addresses 10.0.0.1 and 10.0.0.10, respectively.

The mnemonic names introduced by AddressInfo elements are local with
respect to compound elements. That is, names created inside a compound
element apply only within that compound element and its subelements. For
example:

   AddressInfo(mauer 10.0.0.1);
   compound :: {
     AddressInfo(mazu 10.0.0.10);
     ... -> IPEncap(6, mauer, mazu) -> ...  // OK
   };
   ... -> IPEncap(6, mauer, mazu) -> ...    // error: `mazu' undefined

Any name can be simultaneously associated with an IP address, an IP network
address, and an Ethernet address. The kind of address that is returned is
generally determined from context. For example:

   AddressInfo(mauer 10.0.0.1 00-50-BA-85-84-A9);
   ... -> IPEncap(6, mauer, ...)                  // as IP address
       -> EtherEncap(0x0800, mauer, ...) -> ...   // as Ethernet address
   ... -> ARPResponder(mauer) -> ...              // as IP address and Ethernet address!

An optional suffix makes the context unambiguous. C<NAME> is an ambiguous
reference to some address, but C<NAME:ip> is always an IPv4 address,
C<NAME:ipnet> is always an IPv4 network address (IPv4 address prefix),
C<NAME:ip6> is always an IPv6 address, C<NAME:ip6net> is always an IPv6
network address, and C<NAME:eth> is always an Ethernet address.

Names with both address and prefix definitions are preferentially parsed as
addresses.  For example:

   AddressInfo(boojum 10.0.0.1/24);         // defines address and prefix
   a1 :: ARPResponder(boojum 00-01-02-03-04-05);
   // a1 will have the same configuration as:
   a2 :: ARPResponder(10.0.0.1/32 00-01-02-03-04-05);

To prefer the network prefix, use C<NAME:ipnet>.

If C<NAME:ipnet> is a valid IPv4 network address, then C<NAME:bcast> is a
valid IPv4 address equaling the broadcast address for that network.  This is
obtained by setting all the host bits in the address prefix to 1.

C<NAME:gw> usually equals the default gateway address for network
C<NAME>. If C<NAME> is a device, then C<NAME:gw> can sometimes be
looked up explicitly; otherwise, C<NAME:gw> is calculated from
C<NAME:ipnet> by setting the lowest-order bit in the network address.

=head1 DEFAULT ADDRESSES

If you do not define an address for a given NAME, AddressInfo will use the
default, if any.  Defaults are as follows:

=over 2

=item *

For Ethernet addresses, Click checks for an Ethernet device named NAME, and
uses its address. (At userlevel, this works only on BSD and Linux.)

=item *

For IPv4 addresses, Click checks for a network device named NAME, and uses
its primary IPv4 address.

=item *

For C<NAME:gw>, Click checks for a network device named NAME, and uses its
default gateway.

=back

These defaults are not available on all platforms.

=a

PortInfo */

class AddressInfo : public Element { public:

    AddressInfo();

    const char *class_name() const	{ return "AddressInfo"; }

    int configure_phase() const		{ return CONFIGURE_PHASE_FIRST; }
    int configure(Vector<String> &conf, ErrorHandler *errh);

    enum {
        f_nodevice = 1
    };
    static bool query_ip(const String &s, unsigned char *store, const Element *context, int flags = 0);
    static bool query_ip_prefix(String s, unsigned char *store_addr, unsigned char *store_mask, const Element *context, int flags = 0);
#if HAVE_IP6
    static bool query_ip6(String s, unsigned char *store, const Element *context, int flags = 0);
    static bool query_ip6_prefix(String s, unsigned char *store_addr, int *store_prefixlen, const Element *context, int flags = 0);
#endif
    static bool query_ethernet(String s, unsigned char *store, const Element *context, int flags = 0);

  private:

    static bool query_netdevice(const String &name, unsigned char *store, int type, int len, const Element *context, int flags);

};

CLICK_ENDDECLS
#endif
