#ifndef CLICK_TOYTCP_HH
#define CLICK_TOYTCP_HH
#include <click/element.hh>
#include <click/glue.hh>
#include <click/timer.hh>
#include <click/ipaddress.hh>
CLICK_DECLS

/*
 * =c
 * ToyTCP(dport)
 * =d
 * =e
 * FromDevice(...)
 *   -> Strip(34)
 *   -> ToyTCP(80)
 *   -> IPEncap(6, 1.0.0.1, 1.0.0.2)
 *   -> SetTCPChecksum
 *   -> EtherEncap(0x0800, ..., ...)
 *   -> ToDevice(...)
 */

class ToyTCP : public Element {
 public:
  
  ToyTCP();
  ~ToyTCP();
  
  const char *class_name() const		{ return "ToyTCP"; }
  const char *processing() const		{ return PUSH; }
  int configure(Vector<String> &, ErrorHandler *);
  int initialize(ErrorHandler *);
  ToyTCP *clone() const;

  Packet *simple_action(Packet *);
  void run_scheduled();

private:
  Timer _timer;

  unsigned short _sport; // network byte order.
  unsigned short _dport;

  unsigned _iss;
  unsigned _snd_nxt;
  unsigned _irs;
  unsigned _rcv_nxt;

  int _state;
  int _grow;
  int _wc;
  int _reset;

  int _ingood;
  int _inbad;
  int _out;

  void tcp_output(Packet *);
  void tcp_input(Packet *);
  void restart();
};

CLICK_ENDDECLS
#endif
