#ifdef HAVE_CONFIG_H
# include <config.h>
#endif
#include "hashmap.cc"

#include "string.hh"
template class HashMap<String, int>;

#include "ipaddress.hh"
#include "etheraddress.hh"
template class HashMap<IPAddress, EtherAddress>;
// For IPRouter
template class HashMap<IPAddress, IPAddress>;
