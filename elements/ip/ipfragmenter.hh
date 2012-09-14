#ifndef CLICK_IPFRAGMENTER_HH
#define CLICK_IPFRAGMENTER_HH
#include <click/element.hh>
#include <click/glue.hh>
#include <click/atomic.hh>
CLICK_DECLS

/*
 * =c
 * IPFragmenter(MTU, [I<keywords> HONOR_DF, VERBOSE])
 * =s ip
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
 * Copies all annotations to the fragments.
 *
 * Sends the fragments in order, starting with the first.
 *
 * It is best to Strip() the MAC header from a packet before sending it to
 * IPFragmenter, since any MAC header is not copied to second and subsequent
 * fragments.
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
 * =item VERBOSE
 *
 * Boolean.  If true, IPFragmenter will print a message every time it sees a
 * packet with DF; otherwise, it will print a message only the first 5 times.
 * Default is false.
 *
 * =item HEADROOM
 *
 * Unsigned.  Sets the headroom on the output packets to an explicit value,
 * rather than the default (which is usually about 28 bytes).
 *
 * =e
 *   ... -> fr::IPFragmenter(1024) -> Queue(20) -> ...
 *   fr[1] -> ICMPError(18.26.4.24, 3, 4) -> ...
 *
 * =a ICMPError, CheckLength
 */

class IPFragmenter : public Element { public:

  IPFragmenter() CLICK_COLD;
  ~IPFragmenter() CLICK_COLD;

  const char *class_name() const		{ return "IPFragmenter"; }
  const char *port_count() const		{ return PORTS_1_1X2; }
  const char *processing() const		{ return PUSH; }
  int configure(Vector<String> &, ErrorHandler *) CLICK_COLD;

  uint32_t drops() const			{ return _drops; }
  uint32_t fragments() const			{ return _fragments; }

  void add_handlers() CLICK_COLD;

  void push(int, Packet *);

 private:

  bool _honor_df;
  bool _verbose;
  unsigned _mtu;
  unsigned _headroom;
  atomic_uint32_t _drops;
  atomic_uint32_t _fragments;

  void fragment(Packet *);
  int optcopy(const click_ip *ip1, click_ip *ip2);

};

CLICK_ENDDECLS
#endif
