#ifndef TXSTATSHH
#define TXSTATSHH

/*
 * =c
 * TXFeedbackStats([I<KEYWORDS>])
 * =s Grid
 * Track link transmissions statistics, such as number of retries, failures, etc. 
 *
 * =d
 *
 * Expects Ethernet packets as input, with their timestamps and
 * Transmission Feedback annotations set.  Packets are passed through
 * untouched.
 *
 * Keyword arguments are:
 *
 * =over 8
 *
 * =item TAU
 *
 * Unsigned integer.  Number of millisecons for which to remember
 * packets.  Defaults to 10,000 (10 seconds).
 *
 * =item MIN_PKTS
 *
 * Unsigned integer.  Minimum number of packets required during the
 * last TAU seconds in order to generate a transmission count
 * estimate.  Defaults to 10 packets.  Must be >= 1.
 *
 * =back
 *
 * =h tau read-only
 * =h min_pkts read-only
 * =h stats read-only
 * Returns current stats, one line per destination, in this format:
 * <eth> <num_data_tries> <num_rts_tries> <num_pkts> <est_tx_count>
 * =a
 * LinkStat */

#include <click/bighashmap.hh>
#include <click/element.hh>
#include <click/glue.hh>
#include <click/etheraddress.hh>
#include <click/dequeue.hh>

CLICK_DECLS

class TXFeedbackStats : public Element {
 public:

  // eventually move into FooTxFeedback els, once we sort out exactly
  // what the annotations should look like
  enum tx_result_t {
    TxOk               = 0,
    TxLifetimeExceeded = 1,
    TxMaxRetriesExceed = 2,
    TxUnknownResult    = 99
  };

  TXFeedbackStats();
  ~TXFeedbackStats();
  
  const char *class_name() const		{ return "TXFeedbackStats"; }
  const char *processing() const		{ return AGNOSTIC; }
  const char *flow_code()  const                { return "x/x"; }
  
  void add_handlers();

  int configure(Vector<String> &, ErrorHandler *);
  int initialize(ErrorHandler *);

  Packet *simple_action(Packet *);

  // Estimate the transmission count ETX to DEST.  If there is enough
  // data (more than MIN_PKTS packet to DEST in the last TAU
  // milliseconds), calculate ETX as a percentage (1 tx = 100), and
  // return true.  Else return false.
  bool est_tx_count(const EtherAddress &dest, unsigned &etx);

private:
  
  static String read_params(Element *, void *);
  
  struct stat_t {
    struct timeval when;
    int sz;
    tx_result_t res;
    unsigned n_data; // number data transmissions
    unsigned n_rts;  // number rts transmissions
    stat_t(const timeval &w, int s, tx_result_t r, unsigned nl, unsigned ns) :
      when(w), sz(s), res(r), n_data(nl), n_rts(ns) { }
  };

  // most recent data goes on to head, oldest is at tail
  typedef DEQueue<stat_t> StatQ;
  typedef HashMap<EtherAddress, StatQ> StatMap;
  StatMap _stat_map;

  void add_stat(const EtherAddress &, int, const timeval &, tx_result_t, unsigned, unsigned);
  bool get_counts(const EtherAddress &, unsigned &, unsigned  &, unsigned &);

  StatQ *cleanup_map(const EtherAddress &);

  String print_stats();

  unsigned _tau;
  struct timeval _tau_tv;
  unsigned _min_pkts;
};

CLICK_ENDDECLS
#endif
