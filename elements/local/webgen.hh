#ifndef CLICK_WEBGEN_HH
#define CLICK_WEBGEN_HH
#include <click/element.hh>
#include <click/glue.hh>
#include <click/timer.hh>
#include <click/ipaddress.hh>
CLICK_DECLS

/*
 * =c
 * WebGen(PREFIX, DST, RATE)
 * =s app
 * =d
 * Ask for a random web pages over and over with repeated HTTP
 * connections. Generate them with random source IP addresses
 * starting with PREFIX, and destination address DST.
 * =e
 * kt :: KernelTap(11.11.0.0/16);
 * kt -> Strip(14)
 *    -> WebGen(11.11.0.0/16, 10.0.0.1)
 *    -> EtherEncap(0x0800, 1:1:1:1:1:1, 2:2:2:2:2:2)
 *    -> kt;
 */

class WebGen : public Element {
 public:

  WebGen() CLICK_COLD;
  ~WebGen() CLICK_COLD;

  const char *class_name() const		{ return "WebGen"; }
  const char *port_count() const		{ return PORTS_1_1; }
  const char *processing() const		{ return PUSH; }
  int initialize(ErrorHandler *) CLICK_COLD;
  void cleanup(CleanupStage) CLICK_COLD;
  int configure(Vector<String> &conf, ErrorHandler *errh) CLICK_COLD;

  Packet *simple_action(Packet *);
  void run_timer(Timer *);

private:
  Timer _timer;
  IPAddress _src_prefix;
  IPAddress _mask;
  IPAddress _dst;
  atomic_uint32_t _id;

  // TCP Control Block
  class CB {
  public:
    CB() CLICK_COLD;

    IPAddress _src;		// Our IP address.
    unsigned short _sport;	// network byte order.
    unsigned short _dport;

    unsigned _iss;
    unsigned _snd_una;
    unsigned _snd_nxt;
    unsigned _irs;
    unsigned _rcv_nxt;

    unsigned char
	_connected:1,		// Got SYN+ACK
	_got_fin:1,		// Got FIN
	_sent_fin:1,		// Sent FIN
	_closed:1,		// Got ACK for our FIN
	_do_send:1,
	_spare_bits:3;
    char _resends;

    Timestamp last_send;
    char sndbuf[64];
    unsigned sndlen;

    void reset (IPAddress src);

    void remove_from_list ();
    void add_to_list (CB **phead);

    void rexmit_unlink ();
    void rexmit_update (CB *tail);

    CB *next;
    CB **pprev;

    CB *rexmit_next;
    CB *rexmit_prev;
  };

  static const int htbits = 10;
  static const int htsize = 1 << htbits;
  static const int htmask = htsize - 1;
  CB *cbhash[htsize];
  CB *cbfree;
  CB *rexmit_head, *rexmit_tail;

  // Retransmission
  static const int resend_dt = 1000000;	// rexmit after 1 sec
  static const int resend_max = 5;	// rexmit at most 5 times

  // Scheduling new connections
  int start_interval;			// ms between connections
  Timestamp start_tv;

  // Performance measurement
  static const int perf_dt = 5000000;
  Timestamp perf_tv;
  struct {
    int initiated;
    int completed;
    int reset;
    int timeout;
  } perfcnt;

  void do_perf_stats ();

  void recycle(CB *);
  CB *find_cb(unsigned src, unsigned short sport, unsigned short dport);
  IPAddress pick_src();
  int connhash(unsigned src, unsigned short sport);

  WritablePacket *fixup_packet (Packet *p, unsigned plen);

  void tcp_input(Packet *);
  void tcp_send(CB *, Packet *);
  void tcp_output(WritablePacket *p,
	IPAddress src, unsigned short sport,
	IPAddress dst, unsigned short dport,
	int seq, int ack, char tcpflags,
	char *payload, int paylen);
};

CLICK_ENDDECLS
#endif
