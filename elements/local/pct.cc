#include <click/config.h>
#include <click/confparse.hh>
#include <click/error.hh>
#include "pct.hh"

CLICK_DECLS

Pct::Pct() 
{
  MOD_INC_USE_COUNT;
}

Pct::~Pct()
{
  MOD_DEC_USE_COUNT;
}

int
Pct::configure(Vector<String> &, ErrorHandler *)
{
  return 0;
}

int
Pct::initialize(ErrorHandler *)
{
  click_chatter("hello %u%% there\n", 7);
  return 0;
}


CLICK_ENDDECLS
EXPORT_ELEMENT(Pct)
