#include <click/config.h>
#include <click/error.hh>
#include <click/confparse.hh>
#include <click/standard/scheduleinfo.hh>
#include <click/packet_anno.hh>
#include <click/straccum.hh>
#include "filterfailures.hh"

CLICK_DECLS


FilterFailures::FilterFailures()
  : Element(1, 1),
    _max_failures(0),
    _drops(0)
{
  MOD_INC_USE_COUNT;
}

FilterFailures::~FilterFailures()
{
  MOD_DEC_USE_COUNT;
}

void
FilterFailures::notify_noutputs(int n) 
{
  set_noutputs((n > 3 || n < 1) ? 1 : n);
}

int
FilterFailures::configure(Vector<String> &conf, ErrorHandler *errh)
{

  _max_failures = 1;
  _allow_success = false;
    if (cp_va_parse(conf, this, errh, 
		    cpKeywords,
		    "MAX_FAILURES", cpInteger, "MaxFailures to drop", &_max_failures,
		    "ALLOW_SUCCESS", cpBool, "Filter successfull packets", &_allow_success,
		    cpEnd) < 0) {
      return -1;
    }
  return 0;
}

Packet *
FilterFailures::simple_action(Packet *p)
{

  int success = WIFI_SUCCESS(WIFI_TX_STATUS_ANNO(p));
  if (success) {
    if (_allow_success) {
      return p;
    }
    if (noutputs() == 3) {
      output(2).push(p);
    } else {
      p->kill();
    }
    _drops++;
    return 0;
  }


  if (WIFI_NUM_FAILURES(p) <= _max_failures) {
    return p;
  }

  _drops++;
  if (noutputs() > 1){
    output(1).push(p);
  } else {
    p->kill();
  }
  return (0);
}
String
FilterFailures::static_print_max_failures(Element *f, void *)
{
  StringAccum sa;
  FilterFailures *d = (FilterFailures *) f;
  sa << d->_max_failures << "\n";
  return sa.take_string();
}
String
FilterFailures::static_print_drops(Element *f, void *)
{
  StringAccum sa;
  FilterFailures *d = (FilterFailures *) f;
  sa << d->_drops << "\n";
  return sa.take_string();
}


String
FilterFailures::static_print_allow_success(Element *f, void *)
{
  StringAccum sa;
  FilterFailures *d = (FilterFailures *) f;
  sa << d->_allow_success << "\n";
  return sa.take_string();
}

int
FilterFailures::static_write_max_failures(const String &arg, Element *e,
					  void *, ErrorHandler *errh) 
{
  FilterFailures *n = (FilterFailures *) e;
  int b;

  if (!cp_integer(arg, &b))
    return errh->error("`max_failures' must be an int");

  n->_max_failures = b;
  return 0;
}



int
FilterFailures::static_write_allow_success(const String &arg, Element *e,
					   void *, ErrorHandler *errh) 
{
  FilterFailures *n = (FilterFailures *) e;
  bool b;

  if (!cp_bool(arg, &b))
    return errh->error("`allow_success' must be an bool");

  n->_allow_success = b;
  return 0;
}


void
FilterFailures::add_handlers()
{
  add_write_handler("max_failures", static_write_max_failures, 0);
  add_read_handler("max_failures", static_print_max_failures, 0);


  add_write_handler("allow_success", static_write_allow_success, 0);
  add_read_handler("allow_success", static_print_allow_success, 0);

  add_read_handler("drops", static_print_drops, 0);
}

CLICK_ENDDECLS


EXPORT_ELEMENT(FilterFailures)



