#include <click/config.h>
#include <click/confparse.hh>
#include <click/error.hh>
#include "pct.hh"

CLICK_DECLS

Pct::Pct()
{
}

Pct::~Pct()
{
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
