#ifndef CLICK_PHYERRFILTER_HH
#define CLICK_PHYERRFILTER_HH
#include <click/element.hh>
#include <click/etheraddress.hh>
#include <click/bighashmap.hh>
#include <click/glue.hh>
CLICK_DECLS

/*
=c
PhyErrFilter([I<KEYWORDS>])

=s Wifi

Filters packets that failed the 802.11 CRC check.

=d
Filters out packets that have the phy err annotation set
in the wifi_extra_header, and sends these packets to output 1 
if it is present.

=a ExtraDecap, ExtraEncap
*/


class PhyErrFilter : public Element { public:
  
  PhyErrFilter();
  ~PhyErrFilter();
  
  const char *class_name() const		{ return "PhyErrFilter"; }
  const char *processing() const		{ return "a/ah"; }

  void notify_noutputs(int);
  int configure(Vector<String> &, ErrorHandler *);

  void add_handlers();
  Packet *simple_action(Packet *);  


  int _drops;


};

CLICK_ENDDECLS
#endif
