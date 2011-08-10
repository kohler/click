#include <click/config.h>
#include <click/error.hh>
#include <click/args.hh>
#include <click/standard/scheduleinfo.hh>
#include <clicknet/wifi.h>
#include <click/straccum.hh>
#include <click/packet_anno.hh>
#include "filtertx.hh"

CLICK_DECLS


FilterTX::FilterTX()
  : _drops(0)
{
}

FilterTX::~FilterTX()
{
}

Packet *
FilterTX::simple_action(Packet *p)
{
    struct click_wifi_extra *ceha = WIFI_EXTRA_ANNO(p);
    struct click_wifi_extra *cehp = (struct click_wifi_extra *) p->data();


  if ((ceha->magic == WIFI_EXTRA_MAGIC && ceha->flags & WIFI_EXTRA_TX) ||
      (cehp->magic == WIFI_EXTRA_MAGIC && cehp->flags & WIFI_EXTRA_TX)) {
    if (noutputs() == 2) {
      output(1).push(p);
    } else {
      p->kill();
    }
    return 0;
  }

  return p;
}


enum {H_DROPS };

static String
FilterTX_read_param(Element *e, void *thunk)
{
  FilterTX *td = (FilterTX *)e;
  switch ((uintptr_t) thunk) {
  case H_DROPS:
    return String(td->_drops) + "\n";
  default:
    return String();
  }

}
void
FilterTX::add_handlers()
{
  add_read_handler("drops", FilterTX_read_param, H_DROPS);
}

CLICK_ENDDECLS


EXPORT_ELEMENT(FilterTX)



