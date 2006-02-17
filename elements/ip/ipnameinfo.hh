#ifndef CLICK_IPNAMEINFO_HH
#define CLICK_IPNAMEINFO_HH
#include <click/element.hh>
CLICK_DECLS

/*
=c

IPNameInfo()

=s ip

stores name information about IP packets

=d

Contains IP-related name mappings, such as the names for common IP protocols.
This element should not be used in configurations.
*/

class IPNameInfo : public Element { public:
    
    const char *class_name() const		{ return "IPNameInfo"; }

    static void static_initialize();
    static void static_cleanup();
    
};

CLICK_ENDDECLS
#endif
