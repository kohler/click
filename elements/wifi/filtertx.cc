#include <click/config.h>
#include <click/error.hh>
#include <click/confparse.hh>
#include <click/standard/scheduleinfo.hh>
#include <clicknet/wifi.h>
#include <click/straccum.hh>
#include "filtertx.hh"

CLICK_DECLS


FilterTX::FilterTX()
  : Element(1, 1),
    _drops(0)
{
  MOD_INC_USE_COUNT;
}

FilterTX::~FilterTX()
{
  MOD_DEC_USE_COUNT;
}

void
FilterTX::notify_noutputs(int n) 
{
  set_noutputs((n > 2 || n < 1) ? 1 : n);
}

int
FilterTX::configure(Vector<String> &conf, ErrorHandler *errh)
{

    if (cp_va_parse(conf, this, errh, 
		    cpKeywords,
		    cpEnd) < 0) {
      return -1;
    }
  return 0;
}

Packet *
FilterTX::simple_action(Packet *p)
{
  struct click_wifi_extra *ceh = (struct click_wifi_extra *) p->all_user_anno();  
  
  if (ceh->flags & WIFI_EXTRA_TX) {
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
  add_read_handler("drops", FilterTX_read_param, (void *) H_DROPS);
}

CLICK_ENDDECLS


EXPORT_ELEMENT(FilterTX)



