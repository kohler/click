#ifndef CLICK_CLICKYINFO_HH
#define CLICK_CLICKYINFO_HH
#include <click/element.hh>
CLICK_DECLS

/*
=c
ClickyInfo(STYLE ...)

=s information
stores information for the Clicky GUI

=d

Click drivers ignore this element; it has no inputs or outputs and ignores its
configuration arguments.  The Clicky GUI parses the configuration arguments
and adds them to its styles.
*/

class ClickyInfo : public Element { public:

    ClickyInfo();

    const char *class_name() const	{ return "ClickyInfo"; }
    int configure(Vector<String> &conf, ErrorHandler *errh);

};

CLICK_ENDDECLS
#endif
