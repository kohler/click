#ifndef KERNELHANDLERPROXY_HH
#define KERNELHANDLERPROXY_HH
#include <click/element.hh>

/*
=c

KernelHandlerProxy([I<KEYWORDS>])

=s debugging

proxies kernel module handlers at user level

=io

None

=d

Provides one proxy handler for each handler in a Linux kernel module Click
configuration. The proxy handler for an element C<e>'s handler C<h> is named
`C<e.h>'. Reading KernelHandlerProxy's C<e.h> handler will return the result
of reading kernel element C<e>'s C<h> handler. Similarly, writing a string to
KernelHandlerProxy's C<e.h> handler will cause the proxy to write that string
to kernel element C<e>'s C<h> handler.

Keyword arguments are:

=over 8

=item VERBOSE

Boolean. If true, print chatter messages when read handlers fail. (Errors with
write handlers are reported to the supplied ErrorHandler, but read handlers
don't take an ErrorHandler argument.) Default is false.

=back

=n

KernelHandlerProxy does not decide ahead of time whether a given handler is
active. Thus, for example, KernelHandlerProxy can report that a handler exists
even when no corresponding element exists in the kernel configuration. Any
error will be reported when the handler is actually called.

=a

SocketHandlerProxy

*/

class KernelHandlerProxy : public Element { public:

  KernelHandlerProxy();
  ~KernelHandlerProxy();

  const char *class_name() const	{ return "KernelHandlerProxy"; }
  KernelHandlerProxy *clone() const	{ return new KernelHandlerProxy; }

  int configure(const Vector<String> &, ErrorHandler *);
  
  void add_handlers();
  int local_llrpc(unsigned, void *);
  
 private:

  struct ErrorReceiver {
    void (*function)(const String &, int, void *);
    void *thunk;
  };
  
  bool _detailed_error_message;
  bool _verbose;
  ErrorReceiver *_err_rcvs;
  int _nerr_rcvs;
  
  static String read_handler(Element *, void *);
  static int write_handler(const String &, Element *, void *, ErrorHandler *);
  static int star_write_handler(const String &, Element *, void *, ErrorHandler *);
  void complain_to_err_rcvs(int, const String &elt, const String &hname);
  
};

#endif
