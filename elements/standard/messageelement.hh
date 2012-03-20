// -*- c-basic-offset: 4 -*-
#ifndef CLICK_MESSAGEELEMENT_HH
#define CLICK_MESSAGEELEMENT_HH
#include <click/element.hh>
CLICK_DECLS

/*
=c

Message(MESSAGE [, TYPE])

=s control

prints a message on configuration

=d

The Message element prints a message, warning, or error when configured.  It
can be used to provide configuration-level documentation.  The MESSAGE
argument is the message (a string); TYPE should be MESSAGE, WARNING, or
ERROR.  The default is MESSAGE.

If TYPE is ERROR the router will fail to initialize.

=e

   Message("This configuration is deprecated; use test-tun.click instead.", WARNING)
   tun :: KernelTap(1.0.0.1/8);
   ...

=a

Error
 */

class MessageElement : public Element { public:

    MessageElement();

    const char *class_name() const		{ return "Message"; }
    int configure(Vector<String> &, ErrorHandler *);

};

CLICK_ENDDECLS
#endif
