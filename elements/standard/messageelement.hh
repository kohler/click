// -*- c-basic-offset: 4 -*-
#ifndef CLICK_MESSAGEELEMENT_HH
#define CLICK_MESSAGEELEMENT_HH
#include <click/element.hh>
CLICK_DECLS

/*
=c

Message([MESSAGE, I<keywords> MESSAGE, WARNING, ERROR])

=s debugging

prints a message on configuration

=d

The Message element prints a message, warning, or error when configured.  It
can be used to provide configuration-level documentation.

Keyword arguments are:

=over 8

=item MESSAGE

Print this message (a string).

=item WARNING

Print this warning message (a string).

=item ERROR

Print this error message (a string), and cause the router to fail to
initialize.

=back

=e

   Message(WARNING "This configuration is deprecated; use test-tun.click instead.")
   tun :: KernelTap(1.0.0.1/8);
   ...

=a

Error
 */

class MessageElement : public Element { public:
  
    MessageElement();
    ~MessageElement();
  
    const char *class_name() const		{ return "Message"; }
    int configure(Vector<String> &, ErrorHandler *);

};

CLICK_ENDDECLS
#endif
