#ifdef HAVE_CONFIG_H
# include <config.h>
#endif
#include "checkpattern.hh"
#include "glue.hh"
#include "confparse.hh"

CheckPattern::CheckPattern()
{
  _len = 1;
  add_input();
  add_output();
}

CheckPattern::~CheckPattern()
{
}

int
CheckPattern::configure(const String &conf, Router *router, ErrorHandler *errh)
{
  return cp_va_parse(conf, this, router, errh,
		     cpUnsigned, "packet length", &_len,
		     0);
}

Packet *
CheckPattern::simple_action(Packet *p)
{
  unsigned i;

  if(p->length() != _len)
    click_chatter("CheckPattern: p->length() %d _len %d",
                p->length(), _len);

  for(i = 0; i < _len && i < p->length(); i++){
    if((p->data()[i]&0xff) != (i&0xff)){
      click_chatter("CheckPattern: i=%d pkt %02x wanted %02x",
                  i, p->data()[i] & 0xff, i & 0xff);
      break;
    }
  }

  return(p);
}

EXPORT_ELEMENT(CheckPattern)
