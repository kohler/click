#ifndef IPFRAGMENTER_HH
#define IPFRAGMENTER_HH

/*
 * =c
 * IPFragmenter(MTU, [HONOR_DF])
 * =s IP
 * fragments large IP packets
 * =d
 *
 * Expects IP packets as input. If the IP packet size is <= MTU, just emits
 * the packet on output 0. If the size is greater than MTU and the
 * don't-fragment bit (DF) isn't set or HONOR_DF is false, IPFragmenter
 * splits the packet into fragments emitted on output 0. If DF is set,
 * HONOR_DF is true, and the packet size is greater than MTU, sends to 
 * output 1.
 * 
 * The default value for HONOR_DF is true.
 *
 * Ordinarily output 1 is connected to an ICMPError element
 * with type 3 (UNREACH) and code 4 (NEEDFRAG).
 *
 * Only the mac_broadcast annotation is copied into the fragments.
 *
 * Sends the first fragment last.
 *
 * =e
 *   ... -> fr::IPFragmenter -> Queue(20) -> ...
 *   fr[1] -> ICMPError(18.26.4.24, 3, 4) -> ...
 *
 * =a ICMPError, CheckLength
 */

#include <click/element.hh>
#include <click/glue.hh>
#include <click/atomic.hh>

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
  int configure(const Vector<String> &, ErrorHandler *);
  
  uint32_t drops() const			{ return _drops; }
  uint32_t fragments() const			{ return _fragments; }
  
  IPFragmenter *clone() const;
  void add_handlers();

  void push(int, Packet *);
  
};

#endif
