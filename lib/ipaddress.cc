#ifdef HAVE_CONFIG_H
# include <config.h>
#endif
#include "ipaddress.hh"
#include "confparse.hh"

IPAddress::IPAddress(unsigned char *data)
{
  _saddr = *((unsigned *)data);
}

IPAddress::IPAddress(const String &str)
{
#ifdef __KERNEL__
  printk("<1>IPAddress::IPAddress?\n");
#else
  if (!cp_ip_address(str, (unsigned char *)&_saddr))
    _saddr = 0;
#endif
}

String
IPAddress::s() const
{
  unsigned char *p = (unsigned char *)&_saddr;
  String s;
  char tmp[64];
  sprintf(tmp, "%d.%d.%d.%d", p[0], p[1], p[2], p[3]);
  return(String(tmp));
}

void
IPAddress::print(void)
{
  unsigned char *p = (unsigned char *)&_saddr;
#ifdef __KERNEL__
  printk("<1>IPAddress::print?\n");
#else
  printf("%d.%d.%d.%d", p[0], p[1], p[2], p[3]);
#endif
}
