/*
 * vectori.cc -- Vector template instantiations
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
#include "vector.cc"
#include "string.hh"
#include "routert.hh"
template class Vector<int>;
template class Vector<String>;
template class Vector<Hookup>;
template class Vector<ElementT>;
template class Vector<RouterT::Pair>;
