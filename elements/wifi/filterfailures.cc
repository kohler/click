#include <linux/click_wifi.h>
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

FilterFailures *
FilterFailures::clone() const
{
  return new FilterFailures();
}

void
FilterFailures::notify_noutputs(int n) 
{
  set_noutputs(n < 2 ? 1 : 2);
}

int
FilterFailures::configure(Vector<String> &conf, ErrorHandler *errh)
{

  _max_failures = 1;
    if (cp_va_parse(conf, this, errh, 
		    cpKeywords,
		    "MAX_FAILURES", cpInteger, "MaxFailures to drop", &_max_failures,
		    cpEnd) < 0) {
      return -1;
    }
  return 0;
}

Packet *
FilterFailures::simple_action(Packet *p)
{
   if (WIFI_NUM_FAILURES(p) > _max_failures) {
     _drops++;
     if (noutputs() == 2){
       output(1).push(p);
     }
     p->kill();
     return (0);
   }
   return p;
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


void
FilterFailures::add_handlers()
{
  add_write_handler("max_failures", static_write_max_failures, 0);
  add_read_handler("max_failures", static_print_max_failures, 0);
  add_read_handler("drops", static_print_drops, 0);
}

CLICK_ENDDECLS
ELEMENT_REQUIRES(linuxmodule)

EXPORT_ELEMENT(FilterFailures)



