#ifndef IPFRAGMENTER_HH
#define IPFRAGMENTER_HH

/*
 * =c
 * IPFragmenter(mtu)
 * =d
 * Expects IP packets as input.
 * If the IP packet size is <= mtu, just emits the packet on output 0.
 * If the size is greater than mtu and DF isn't set, splits into
 * fragments emitted on output 0.
 * If DF is set and size is greater than mtu, sends to output 1.
 * 
 * Ordinarily output 1 is connected to an ICMP error packet generator
 * with type 3 (UNREACH) and code 4 (NEEDFRAG).
 *
 * Only the mac_broadcast annotation is copied into the fragments.
 *
 * Sends the first fragment last.
 *
 * =e
 * Example:
 *
 * = ... -> fr::IPFragmenter -> Queue(20) -> ...
 * = fr[1] -> ICMPError(18.26.4.24, 3, 4) -> ...
 *
 * =a ICMPError
 */

#include "element.hh"
#include "glue.hh"

class IPFragmenter : public Element {

  unsigned _mtu;
  int _drops;
  int _fragments;
  
 public:
  
  IPFragmenter();
  ~IPFragmenter();
  
  const char *class_name() const		{ return "IPFragmenter"; }
  Processing default_processing() const		{ return PUSH; }
  void notify_noutputs(int);
  int configure(const String &, ErrorHandler *);
  
  int drops() const				{ return _drops; }
  int fragments() const				{ return _fragments; }
  
  IPFragmenter *clone() const;
  void add_handlers(HandlerRegistry *fcr);

  inline Packet *smaction(Packet *);
  void push(int, Packet *p);
  Packet *pull(int);
  int optcopy(click_ip *ip1, click_ip *ip2);
  
};

#endif
