#ifndef GRIDPROBEREPLYRECEIVER_HH
#define GRIDPROBEREPLYRECEIVER_HH

/*
 * =c
 * GridProbeReplyReceiver(CHANNEL)
 * =s Grid
 * Receives Grid route probe replies
 * =d
 * 
 * When a Grid probe reply is received on its inputs, prints a message
 * to the Click chatter channel specified by C<CHANNEL>.  An example
 * message is:
 *
 * C<dest=18.26.7.100 nonce=197355 hop=18.26.7.4 hopcount=3 rtt=0.003456>
 *
 * where C<dest> is the node to which the original probe was headed,
 * C<nonce> is the nonce from that probe, C<hop> is the node which
 * produced this reply from the probe, C<hopcount> is the number of
 * hops away the reply sender is from the probe sender, and C<rtt> is
 * the probe reply's round trip time, in seconds.
 *
 * =a GridProbeSender, GridProbeHandler, ChatterSocket */


#include <click/element.hh>
#include <click/etheraddress.hh>
#include <click/ipaddress.hh>
#include <click/vector.hh>
#include <click/bighashmap.hh>
#include <click/timer.hh>

class GridProbeReplyReceiver : public Element {

 public:
  GridProbeReplyReceiver();
  ~GridProbeReplyReceiver();
  
  const char *class_name() const		{ return "GridProbeReplyReceiver"; }
  const char *processing() const		{ return AGNOSTIC; }
  GridProbeReplyReceiver *clone() const;
  int configure(const Vector<String> &, ErrorHandler *);
  int initialize(ErrorHandler *);

  Packet *simple_action(Packet *);

private:
  ErrorHandler *_repl_errh;
};

#endif
