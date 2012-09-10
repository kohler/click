#ifndef LINKTESTER_HH
#define LINKTESTER_HH

#include <click/element.hh>
#include <click/glue.hh>
#include <click/etheraddress.hh>
#include <click/timer.hh>
CLICK_DECLS

/*
 * =c
 * LinkTester(SRC, DST [, KEYWORDS])
 *
 * =s Grid
 *
 * =d
 *
 * Send experimental packets over wireless link to test link
 * characteristics.  Packets are sent with source and destination
 * ethernet address SRC_ETH and DST_ETH.  The ethernet type code is
 * set to 0x7eee.
 *
 * Keyword arguments are:
 *
 * =over 8
 *
 * =item START_TIME
 *
 * Integer.  When to start the experiment, in seconds since the
 * ``epoch'' (time_t).  Defaults to 5 seconds after router
 * initialization.
 *
 * =item ITERATIONS
 *
 * Unsigned integer.  Number of iterations to the run the experiment.
 * In each iteration, each node sends broadcasts, then unicasts, then
 * broadcasts.  Defaults to 1.
 *
 * =item SEND_FIRST
 *
 * Boolean.  Should this element send its packets at the beginning of
 * the iteration?  Otherwise, packets are sent at the end.  Defaults
 * to true.
 *
 * =item PAD_TIME
 *
 * Unsigned integer.  Milliseconds between experiment phases.
 * Defaults to 10,000 (10 seconds).
 *
 * =item UNICAST_SEND_TIME
 * =item BROADCAST_SEND_TIME
 *
 * Unsigned integer.  Milliseconds for which to send unicast or
 * broadcast packets during each iteration.  Defaults to 10,000 (10
 * seconds).
 *
 * =item UNICAST_PACKET_SZ
 * =item BROADCAST_PACKET_SZ
 *
 * Unsigned integer.  Size of unicast or broadcast packets, including
 * ethernet header and experiment header.
 *
 * =item UNICAST_LAMBDA
 * =item BROADCAST_LAMBDA
 *
 * Double.  Lambda parameter for unicast or broadcast exponentially
 * distributed inter-packet spacing, in milliseconds.  Defaults to 1.
 */



class LinkTester : public Element {

public:
  static const uint16_t ETHERTYPE = 0x7eee;

  struct payload_t {
    unsigned short before;
    unsigned short size; // bytes
    unsigned int iteration;
    unsigned int seq_no;
    unsigned int tx_sec;
    unsigned int tx_usec;
  };

  LinkTester() CLICK_COLD;
  ~LinkTester() CLICK_COLD;

  const char *class_name() const { return "LinkTester"; }
  const char *port_count() const { return PORTS_0_1; }
  const char *processing() const { return PUSH; }

  int configure(Vector<String> &, ErrorHandler *) CLICK_COLD;
  int initialize(ErrorHandler *) CLICK_COLD;

private:

  void timer_hook();
  static void static_timer_hook(Timer *, void *e)
  { static_cast<LinkTester *>(e)->timer_hook(); }

  bool experiment_params_ok(ErrorHandler *);

  void handle_timer_waiting(const Timestamp &tv);
  void handle_timer_listening(const Timestamp &tv);
  void handle_timer_bcast(const Timestamp &tv);
  void handle_timer_unicast(const Timestamp &tv);

  // msecs, including any internal pad times, e.g. between broadcast
  // and unicast phases, but not including pads at either end of
  // interval
  unsigned int calc_listen_time();
  unsigned int calc_unicast_time();
  unsigned int calc_bcast_time();

  int calc_pad_time() { return _pad; }

  // time at which (before which) to send the first (last) packet of the iter'th iteration
  // before => first broadcast set, !before => second broadcast set
  Timestamp first_unicast_time(unsigned int iter);
  Timestamp first_bcast_time(unsigned int iter, bool before);
  Timestamp last_unicast_time(unsigned int iter);
  Timestamp last_bcast_time(unsigned int iter, bool before);

  void send_unicast_packet(const Timestamp &,
			   unsigned int seq, unsigned int iter);
  void send_broadcast_packet(unsigned short psz, const Timestamp &,
			     bool before, unsigned int seq, unsigned int iter);

  void finish_experiment();

  bool init_random();
  double draw_random(double lambda);

  unsigned int draw_random_msecs(double lambda)
  { return static_cast<unsigned int>(draw_random(lambda)); }

  enum state_t {
    WAITING_TO_START,
    LISTENING,
    BCAST_1,
    UNICAST,
    BCAST_2,
    DONE
  };

  int _start_time; // unix epoch seconds
  Timestamp _start_time_tv;
  class EtherAddress _src_eth;
  class EtherAddress _dst_eth;
  class Timer _timer;
  state_t _curr_state;

  bool _send_first;

  unsigned int _iterations_done;
  unsigned int _num_iters;

  unsigned int _packets_sent;
  unsigned int _bcast_packets_sent;

  unsigned int _pad;           // milliseconds between phases

  // unicast tx parameters
  unsigned int _packet_size;   // bytes, including ether header
  unsigned int _send_time;     // length of unicast sending period, milliseconds
  double _lambda;              // exponential distribution parameter

  // broadcast tx parameters, as in unicast
  unsigned int _bcast_packet_size;
  unsigned int _bcast_send_time;
  double _bcast_lambda;

  unsigned char *_data_buf;

  Timestamp _last_time;		// when timer was actually last fired
  Timestamp _next_time;		// when we *want* the next packet to be sent

};

CLICK_ENDDECLS
#endif
