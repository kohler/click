#ifndef CLICK_PHYERRFILTER_HH
#define CLICK_PHYERRFILTER_HH
#include <click/element.hh>
#include <click/etheraddress.hh>
#include <click/bighashmap.hh>
#include <click/glue.hh>
CLICK_DECLS

/*
 * =c
 * 
 * PhyErrFilter([I<KEYWORDS>])
 *
 * =s wifi
 * Filters out packets that have the phy err annotation set
 * Sends these packets to output 1 if it is present, 
 * otherwise it drops the packets.
 * 
 * =s wifi
 *
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
