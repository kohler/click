/*
 * templatei.cc -- Template instantiations
 * Eddie Kohler
 *
 * Copyright (c) 1999-2000 Massachusetts Institute of Technology
 *
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files (the "Software"),
 * to deal in the Software without restriction, subject to the following
 * conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * Further elaboration of this license, including a DISCLAIMER OF ANY
 * WARRANTY, EXPRESS OR IMPLIED, is provided in the LICENSE file, which is
 * also accessible at http://www.pdos.lcs.mit.edu/click/license.html
 */

#ifdef HAVE_CONFIG_H
# include <config.h>
#endif
#include "string.hh"
#include "router.hh"
#include "ipflowid.hh"
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

#include "bighashmap.cc"
template class BigHashMap<IPAddress, unsigned>;
template class BigHashMap<IPFlowID, bool>;
template class BigHashMapIterator<IPAddress, unsigned>;
template class BigHashMapIterator<IPFlowID, bool>;

#include "ewma.cc"
template class DirectEWMAX<4, 10>;
