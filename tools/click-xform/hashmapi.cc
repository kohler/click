#ifdef HAVE_CONFIG_H
# include <config.h>
#endif
#include "hashmap.cc"

#include "string.hh"
template class HashMap<String, int>;
template class HashMap<String, String>;
