/*
 * hashmapi.cc -- hash table instantiations
 * Eddie Kohler
 *
 * Copyright (c) 1999 Massachusetts Institute of Technology.
 *
 * This software is being provided by the copyright holders under the GNU
 * General Public License, either version 2 or, at your discretion, any later
 * version. For more information, see the `COPYRIGHT' file in the source
 * distribution.
 */

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
