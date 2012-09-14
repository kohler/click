#ifndef CLICK_IP6PRINT_HH
#define CLICK_IP6PRINT_HH
#include <click/element.hh>
#include <click/string.hh>
CLICK_DECLS

/*
=c

IP6Print([TAG, NBYTES, CONTENTS])

=s ip6

pretty-prints IP6 packets

=d

dumps simple information about ip6 packet.
may someday be as good as IPPrint. TAG
specifies the label at the head of each
line. NBYTES specify how many bytes to print
and CONTENTS specify if the content should
be printed, in hex. NBYTES and CONTENTS
are keywords.

=a IPPrint, CheckIPHeader */

class IP6Print : public Element { public:

  IP6Print();
  ~IP6Print();

  const char *class_name() const		{ return "IP6Print"; }
  const char *port_count() const		{ return PORTS_1_1; }

  int configure(Vector<String> &, ErrorHandler *) CLICK_COLD;

  Packet *simple_action(Packet *);

 private:

  String _label;
  unsigned _bytes;
  bool _contents;
};

CLICK_ENDDECLS
#endif
