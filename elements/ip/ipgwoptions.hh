#ifndef IPGWOPTIONS_HH
#define IPGWOPTIONS_HH

/*
 * =c
 * IPGWOptions(local-ip-addr)
 * =d
 * Process the IP options that should be processed by every router,
 * not just when ip_dst refers to the current router. At the moment
 * that amounts to Record Route and Timestamp (in particular,
 * not the source route options). The local-ip-addr is the router's
 * IP address on the interface downstream from the element.
 *
 * Probably needs to be placed on the output path, since local-ip-addr
 * must be the outgoing interface's IP address (rfc1812 4.2.2.2).
 *
 * Recomputes the IP header checksum if it modifies the packet.
 *
 * Does not fully implement the Timestamp option, since it doesn't
 * know all the current router's IP addresses.
 *
 * The second output may be connected to an ICMPError to produce
 * a parameter problem (type=12,code=0) message. IPGWOptions sets
 * the param_off packet annotation so that ICMPError can set
 * the Parameter Problem pointer to point to the erroneous byte.
 *
 * =a IPDstOptions
 * =a ICMPError
 */

#include "element.hh"
#include "glue.hh"

class IPGWOptions : public Element {
  int _drops;
  struct in_addr _my_ip;

 public:
  
  IPGWOptions();
  ~IPGWOptions();
  
  const char *class_name() const		{ return "IPGWOptions"; }
  int configure(const String &, ErrorHandler *);
  void notify_outputs(int);
  void processing_vector(Vector<int> &, int, Vector<int> &, int) const;
  IPGWOptions *clone() const;
  void add_handlers(HandlerRegistry *fcr);

  int drops() { return(_drops); }
  
  Packet *handle_options(Packet *);
  void push(int, Packet *p);
  Packet *pull(int);
  
};

#endif
