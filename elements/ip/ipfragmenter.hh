#ifndef CLICK_IPFRAGMENTER_HH
#define CLICK_IPFRAGMENTER_HH
#include <click/element.hh>
#include <click/glue.hh>
#include <click/atomic.hh>
CLICK_DECLS

/*
 * =c
 * IPFragmenter(MTU, [I<keywords> HONOR_DF])
 * =s IP
 * fragments large IP packets
 * =d
 *
 * Expects IP packets as input. If the IP packet size is <= MTU, just emits
 * the packet on output 0. If the size is greater than MTU and the
 * don't-fragment bit (DF) isn't set, IPFragmenter splits the packet into
 * fragments emitted on output 0. If DF is set and the packet size is greater
 * than MTU, sends the packet to output 1 (but see HONOR_DF below). Ordinarily
 * output 1 is connected to an ICMPError element with type 3 (UNREACH) and
 * code 4 (NEEDFRAG).
 *
 * Only the mac_broadcast annotation is copied into the fragments.
 *
 * Sends the first fragment last.
 *
 * Keyword arguments are:
 *
 * =over 8
 *
 * =item HONOR_DF
 *
 * Boolean. If HONOR_DF is false, IPFragmenter will ignore the don't-fragment
 * (DF) bit and fragment every packet larger than MTU. Default is true.
 *
 * =e
 *   ... -> fr::IPFragmenter(1024) -> Queue(20) -> ...
 *   fr[1] -> ICMPError(18.26.4.24, 3, 4) -> ...
 *
 * =a ICMPError, CheckLength
 */

class IPFragmenter : public Element {

  bool _honor_df;
  unsigned _mtu;
  uatomic32_t _drops;
  uatomic32_t _fragments;

  void fragment(Packet *);
  int optcopy(const click_ip *ip1, click_ip *ip2);
  
 public:


  IPFragmenter();
  ~IPFragmenter();
  
  const char *class_name() const		{ return "IPFragmenter"; }
  const char *processing() const		{ return PUSH; }
  void notify_noutputs(int);
  int configure(Vector<String> &, ErrorHandler *);
  
  uint32_t drops() const			{ return _drops; }
  uint32_t fragments() const			{ return _fragments; }
  
  IPFragmenter *clone() const;
  void add_handlers();

  void push(int, Packet *);
  
};

CLICK_ENDDECLS
#endif
