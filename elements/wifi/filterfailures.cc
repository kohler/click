#include <click/config.h>
#include <click/error.hh>
#include <click/confparse.hh>
#include <click/standard/scheduleinfo.hh>
#include <click/packet_anno.hh>
#include <click/straccum.hh>
#include <clicknet/wifi.h>
#include "filterfailures.hh"

CLICK_DECLS


FilterFailures::FilterFailures()
  : Element(1, 1),
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

    if (cp_va_parse(conf, this, errh, 
		    cpKeywords,
		    cpEnd) < 0) {
      return -1;
    }
  return 0;
}

Packet *
FilterFailures::simple_action(Packet *p)
{
  
  struct click_wifi_extra *ceha = (struct click_wifi_extra *) p->all_user_anno();  
  struct click_wifi_extra *cehp = (struct click_wifi_extra *) p->data();
  
  
  if ((ceha->magic == WIFI_EXTRA_MAGIC && ceha->flags & WIFI_EXTRA_TX_FAIL) ||
      (cehp->magic == WIFI_EXTRA_MAGIC && cehp->flags & WIFI_EXTRA_TX_FAIL)) {
    if (noutputs() == 2) {
      output(1).push(p);
    } else {
      p->kill();
    }
    _drops++;
    return 0;
  }
  return p;
}

enum {H_DROPS };

static String
FilterFailures_read_param(Element *e, void *thunk)
{
  FilterFailures *td = (FilterFailures *)e;
  switch ((uintptr_t) thunk) {
  case H_DROPS: 
    return String(td->_drops) + "\n";
  default:
    return String();
  }

}

void
FilterFailures::add_handlers()
{
  add_read_handler("drops", FilterFailures_read_param, (void *) H_DROPS);
}

CLICK_ENDDECLS


EXPORT_ELEMENT(FilterFailures)



