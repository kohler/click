#ifndef WEBGEN_HH
#define WEBGEN_HH

/*
 * =c
 * WebGen()
 * =d
 * Ask for a random web pages over and over with repeated HTTP/1.0
 * connections.
 * =e
 * FromDevice(...)
 *   -> Strip(34)
 *   -> WebGen()
 *   -> IPEncap(6, 1.0.0.1, 1.0.0.2)
 *   -> SetTCPChecksum
 *   -> EtherEncap(0x0800, ..., ...)
 *   -> ToDevice(...)
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

  Packet *simple_action(Packet *);
  void run_scheduled();

private:
  Timer _timer;
  int _next_port;

  // TCP Control Block
  class CB {
  public:
    CB();

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

    void reset(int np);
  };

  CB *_cbs[100];
  int _ncbs;

  void tcp_output(CB *, Packet *);
  void tcp_input(Packet *);
  CB *find_cb(unsigned short sport, unsigned short dport);
};

#endif
