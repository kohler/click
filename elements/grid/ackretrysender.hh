#ifndef CLICK_ACKRETRYSENDER_HH
#define CLICK_ACKRETRYSENDER_HH
#include <click/element.hh>
#include <click/etheraddress.hh>
#include <click/packet.hh>
#include <click/task.hh>
#include <click/timer.hh>
CLICK_DECLS

/*
 * =c
 * ACKRetrySender(I<KEYWORDS>)
 *
 * =s Grid
 * Resend packets until a positive acknowledgement is received.
 *
 * =d
 *
 * Input 0 should be Ethernet packets.  Input 1 should be
 * acknowledgements.  When a packet is pulled in on input 0, it is
 * pushed on output 0, and cached until a positive acknowledgement
 * (ACK) is received.  If no ACK is received before the resend timer
 * expires, the packet is resent.  If the packet has been resent too
 * many times, it is pushed to output 1.  If output 1 is not
 * connected, it is dropped.
 * 
 * Keyword arguments are:
 *
 * =over 8
 *
 * =item MAX_RETRIES
 *
 * Unsigned integer, > 0.  Resend the packet up to this many times
 * before giving up.  Default is 16.  This includes the initial
 * transmission.
 *
 * =item TIMEOUT
 *
 * Unsigned integer, > 0.  Milliseconds.  Wait this long before
 * resending the packet.  Default is 10.
 *
 * =item VERBOSE
 *
 * Boolean.  Be noisy.  True by default.
 *
 * =a 
 * ACKResponder */

class ACKRetrySender : public Element {
public:
  ACKRetrySender();
  ~ACKRetrySender();

  const char *class_name() const { return "ACKRetrySender"; }
  const char *processing() const { return "la/hh"; }
  const char *flow_code()  const { return "xy/xx"; }
  ACKRetrySender *clone()  const { return new ACKRetrySender; }

  int configure(Vector<String> &, ErrorHandler *);
  int initialize(ErrorHandler *errh);

  void notify_ninputs(int n)  { set_ninputs(n); }
  void notify_noutputs(int n) { set_noutputs(n); }

  void run_scheduled();
  void push(int, Packet *);

private:
  unsigned int _timeout; // msecs
  unsigned int _max_retries;
  unsigned int _num_retries;

  Packet *_waiting_packet;
  bool _verbose;

  static void static_timer_hook(Timer *, void *v) { ((ACKRetrySender *) v)->timer_hook(); }
  void timer_hook();

  Timer _timer;
  Task _task;

  void check();
};

CLICK_ENDDECLS
#endif
