#ifndef WEBGEN_HH
#define WEBGEN_HH

/*
 * =c
 * WebGen(PREFIX/LEN, DST)
 * =d
 * Ask for a random web pages over and over with repeated HTTP
 * connections. Generate them with random source IP addresses
 * starting with PREFIX.
 * =e
 * kt :: KernelTap(11.11.0.0/16);
 * kt -> Strip(14)
 *    -> WebGen(11.11.0.0/16, 10.0.0.1)
 *    -> EtherEncap(0x0800, 1:1:1:1:1:1, 2:2:2:2:2:2)
 *    -> kt;
 */

#include <click/element.hh>
#include <click/glue.hh>
#include <click/timer.hh>
#include <click/ipaddress.hh>

class WebGen : public Element {
 public:
  
  WebGen();
  ~WebGen();
  
  const char *class_name() const		{ return "WebGen"; }
  const char *processing() const		{ return PUSH; }
  int initialize(ErrorHandler *);
  WebGen *clone() const;
  int configure(const Vector<String> &conf, ErrorHandler *errh);

  Packet *simple_action(Packet *);
  void run_scheduled();

private:
  Timer _timer;
  IPAddress _src_prefix;
  IPAddress _mask;
  IPAddress _dst;
  u_atomic32_t _id;

  // TCP Control Block
  class CB {
  public:
    CB();

    IPAddress _src; // Our IP address.
    unsigned short _sport; // network byte order.
    unsigned short _dport;

    unsigned _iss;
    unsigned _snd_una;
    unsigned _snd_nxt;
    unsigned _irs;
    unsigned _rcv_nxt;
    unsigned _fin_seq;

    int _connected; // Got SYN+ACK
    int _got_fin;   // Got FIN
    int _closed;    // Got ACK for our FIN
    int _reset;     // Got RST
    int _do_send;
    int _resends;

    void reset(IPAddress src);
  };

  CB *_cbs[100];
  int _ncbs;

  void tcp_output(CB *, Packet *);
  void tcp_input(Packet *);
  CB *find_cb(unsigned src, unsigned short sport, unsigned short dport);
  IPAddress pick_src();
};

#endif
