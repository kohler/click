#ifndef CLICK_FILTERFAILURES_HH
#define CLICK_FILTERFAILURES_HH
#include <click/element.hh>
#include <click/etheraddress.hh>
#include <click/bighashmap.hh>
#include <click/glue.hh>
CLICK_DECLS

/*
 * =c
 * 
 * FilterFailures([I<KEYWORDS>])
 *
 * =s wifi
 * Filters out packets that have failed at least MAX_FAILURES times.
 * Sends these packets to output 1 if it is present, 
 * otherwise it drops the packets.
 * 
 * Arguments:
 * =item MAX_FAILURES
 * The default MAX_FAILURES is 1
 *
 * =s wifi
 *
 * =a WifiTXFeedback
 * 
 */


class FilterFailures : public Element { public:
  
  FilterFailures();
  ~FilterFailures();
  
  const char *class_name() const		{ return "FilterFailures"; }
  const char *processing() const		{ return "a/ah"; }
  FilterFailures *clone() const;

  void notify_noutputs(int);
  int configure(Vector<String> &, ErrorHandler *);

  void add_handlers();
  static String static_print_drops(Element *, void *);
  static String static_print_max_failures(Element *, void *);
  static int static_write_max_failures(const String &arg, Element *e,
				void *, ErrorHandler *errh); 
  Packet *simple_action(Packet *);

 private:

  int _max_failures;
  int _drops;
};

CLICK_ENDDECLS
#endif
