#ifndef TOYTCP_HH
#define TOYTCP_HH

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

#include "element.hh"
#include "glue.hh"
#include "timer.hh"
#include "ipaddress.hh"

class ToyTCP : public Element {
 public:
  
  ToyTCP();
  ~ToyTCP();
  
  const char *class_name() const		{ return "ToyTCP"; }
  const char *processing() const		{ return PUSH; }
  int configure(const Vector<String> &, ErrorHandler *);
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

  bool _done;  // Have we received an RST?
  int _ingood;
  int _inbad;
  int _out;

  void tcp_output(Packet *);
  void tcp_input(Packet *);
};

#endif
