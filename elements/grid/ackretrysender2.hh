#ifndef CLICK_ACKRETRYSENDER2_HH
#define CLICK_ACKRETRYSENDER2_HH
#include <click/element.hh>
#include <click/etheraddress.hh>
#include <click/packet.hh>
#include <click/task.hh>
#include <click/timer.hh>
#include <click/ipaddress.hh>
#include <click/deque.hh>
CLICK_DECLS

/*
 * =c
 * ACKRetrySender2(I<KEYWORDS>)
 *
 * =s Grid
 * Resend packets until a positive acknowledgement is received.
 *
 * =d
 *
 * This element (and ACKResponder2) is essentially the same as
 * ACKRetrySender, except it encapsulates the data packets with a
 * mini-header so that link-layer broadcast packets can be used to
 * avoid link-layer retransmissions (as in 802.11).
 *
 * Input 0 should be packets with a destination IP address annotation
 * set.  Input 1 should be acknowledgements from an ACKResponder2.
 * When a packet is pulled in on input 0, it is encapsulated with a
 * Retry Header, pushed on output 0, and cached until a positive
 * acknowledgement (ACK) is received.  If no ACK is received before
 * the resend timer expires, the packet is resent.  If the packet has
 * been resent too many times, it is pushed to output 1.  If output 1
 * is not connected, it is dropped.
 *
 * Output 0 should pass through an EtherEncap which puts encapsulated
 * the packet with this node's source ether address, the broadcast
 * ether dest, and the ACKRetry data ethertype (typically 0x7ffb).
 *
 * Keyword arguments are:
 *
 * =over 8
 *
 * =item IP
 *
 * This node's IP address.  Required argument.
 *
 * =item MAX_TRIES
 *
 * Unsigned integer, > 0.  Send the packet up to this many times
 * before giving up.  Default is 16.  This includes the initial
 * transmission.
 *
 * =item TIMEOUT
 *
 * Unsigned integer, > 0.  Milliseconds.  Wait this long before
 * resending the packet.  Default is 10.
 *
 * =item HISTORY_SZ
 *
 * Unsigned integer.  Number of most recent packets for which to
 * remember retry data.  Defaults to 500.
 *
 * =item VERBOSE
 *
 * Boolean.  Be noisy.  True by default.
 *
 * =back
 *
 * =h summary read-only
 * Print summary of packet retry statistics
 *
 * =h history read-only
 * Print packet retry history.
 *
 * =h clear write-only
 * Clear out packet retry history.
 *
 * =h reset write-only
 * Reset packet retry statistics.
 * =a
 * ACKResponder2, ACKRetrySender, ACKResponder, EtherEncap */

/* packet formats:

Data/ACK packet formats:

(pushed by later EtherEncap) ether dest [6]
(pushed by later EtherEncap) ether src  [6]
(pushed by later EtherEncap) ether type [2] (0x7ffb for data, 0x7ffc for ACK)
src IP
dst IP
<encapsulated data packet> (not for ACK)

*/

class ACKRetrySender2 : public Element {
public:
  ACKRetrySender2();
  ~ACKRetrySender2();

  const char *class_name() const { return "ACKRetrySender2"; }
  const char *port_count() const { return "-/-"; }
  const char *processing() const { return "la/hh"; }
  const char *flow_code()  const { return "xy/xx"; }

  int configure(Vector<String> &, ErrorHandler *);
  int initialize(ErrorHandler *errh);

  bool run_task(Task *);
  void run_timer(Timer *);
  void push(int, Packet *);

  void add_handlers();

private:
  unsigned int _timeout; // msecs
  unsigned int _max_tries; // max number of times to tx before quitting
  unsigned int _num_tries; // number of times current packet has been sent
  unsigned int _history_length;

  struct tx_result_t {
    tx_result_t(const Timestamp &t, unsigned n, bool s)
      : pkt_time(t), num_tx(n), success(s) { }
    Timestamp pkt_time;
    unsigned num_tx;
    bool success;
  };

  typedef Deque<tx_result_t> HistQ;
  HistQ _history;

  IPAddress _ip;

  Packet *_waiting_packet;
  bool _verbose;

  Timer _timer;
  Task _task;

  unsigned sum_tx;
  unsigned num_pkts;
  unsigned num_fail;
  unsigned max_txc, min_txc;

  void check();

  void add_stat(const Timestamp &t, unsigned num_tx, bool succ);

  static String print_history(Element *e, void *);
  static String print_summary(Element *e, void *);
  static int clear_history(const String &, Element *, void *, ErrorHandler *);
  static int reset_stats(const String &, Element *, void *, ErrorHandler *);
};

CLICK_ENDDECLS
#endif
