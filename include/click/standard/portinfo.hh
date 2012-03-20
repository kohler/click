// -*- c-basic-offset: 4; related-file-name: "../../../elements/standard/portinfo.cc" -*-
#ifndef CLICK_PORTINFO_HH
#define CLICK_PORTINFO_HH
#include <click/element.hh>
CLICK_DECLS

/*
=c

PortInfo(NAME PORT[/PROTOCOL], ...)

=s information

stores named TCP/UDP port information

=io

None

=d

Lets you use mnemonic names for TCP and UDP ports.  Each argument has the form
`NAME PORT[/PROTOCOL]', which associates the given PORT/PROTOCOL pair with the
NAME.  If PROTOCOL is left off, the NAME applies to both TCP and UDP.  For
example, in a configuration containing

   PortInfo(ssh 22, http 80),

configuration strings can use C<ssh> and C<http> as mnemonics for the port
numbers 22 and 80, respectively.

PortInfo names are local with respect to compound elements.  That is, names
created inside a compound element apply only within that compound element and
its subelements.  For example:

   PortInfo(src 10);
   compound :: {
     PortInfo(dst 100);
     ... -> UDPIPEncap(1.0.0.1, src, 2.0.0.1, dst) -> ...  // OK
   };
   ... -> UDPIPEncap(1.0.0.1, src, 2.0.0.1, dst) -> ...
                                         // error: `dst' undefined

=n

If you do not define a port for a given name, PortInfo will use the default,
if any.  At user level, PortInfo uses the L<getservbyname(3)> function to look
up ports by name.  In the kernel, there are no default ports.

PortInfo will parse arguments containing more than one name, as `C<NAME
PORT/PROTOCOL NAME...>', and comments starting with `C<#>' are ignored.  Thus,
lines from F</etc/services> can be used verbatim as PortInfo configuration
arguments.

=a

AddressInfo */

class PortInfo : public Element { public:

    PortInfo();

    const char *class_name() const	{ return "PortInfo"; }

    int configure_phase() const		{ return CONFIGURE_PHASE_FIRST; }
    int configure(Vector<String> &, ErrorHandler *);

};

CLICK_ENDDECLS
#endif
