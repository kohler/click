/*
 * templatei.cc -- Template instantiations
 * Eddie Kohler
 *
 * Copyright (c) 1999-2000 Massachusetts Institute of Technology.
 *
 * This software is being provided by the copyright holders under the GNU
 * General Public License, either version 2 or, at your discretion, any later
 * version. For more information, see the `COPYRIGHT' file in the source
 * distribution.
 */

#ifdef HAVE_CONFIG_H
# include <config.h>
#endif
#include "string.hh"
#include "router.hh"
#include "ipaddress.hh"
#include "etheraddress.hh"

#include "vector.cc"
template class Vector<Router::Hookup>;
template class Vector<int>;
template class Vector<unsigned int>;
template class Vector<String>;

#include "hashmap.cc"
template class HashMap<String, int>;
template class HashMapIterator<String, int>;

#include "ewma.cc"
template class DirectEWMAX<4, 10>;
