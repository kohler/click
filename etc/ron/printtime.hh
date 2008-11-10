#ifndef IPPRINTTIME_HH
#define IPPRINTTIME_HH
#include <click/element.hh>
#include <click/string.hh>

/*
=c

IPPrintTime([TAG, I<KEYWORDS>])

=s IP, debugging

pretty-prints IP packets

=d

Expects IP packets as input.  Should be placed downstream of a
CheckIPHeader or equivalent element.

Prints out IP packets in a human-readable tcpdump-like format, preceded by
the TAG text.

=a Print, CheckIPHeader */

class IPPrintTime : public Element { public:

  IPPrintTime();
  ~IPPrintTime();

  const char *class_name() const		{ return "IPPrintTime"; }
  const char *port_count() const		{ return "1/1"; }
  const char *processing() const		{ return AGNOSTIC; }

  int configure(Vector<String> &, ErrorHandler *);
  int initialize(ErrorHandler *);
  void uninitialize();

  Packet *simple_action(Packet *);

 private:

  bool _swap;
  String _label;
  unsigned _bytes;		// Number of bytes to dump
  bool _print_id : 1;		// Print IP ID?
  bool _print_timestamp : 1;
  bool _print_paint : 1;
  bool _print_tos : 1;
  bool _print_ttl : 1;
  bool _print_len : 1;
  unsigned _contents : 2;	// Whether to dump packet contents

#if CLICK_USERLEVEL
  String _outfilename;
  FILE *_outfile;
#endif
  ErrorHandler *_errh;

};

#endif
