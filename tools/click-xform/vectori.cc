#ifdef HAVE_CONFIG_H
# include <config.h>
#endif
#include "vector.cc"
#include "string.hh"
#include "elementt.hh"
template class Vector<int>;
template class Vector<String>;
template class Vector<Hookup>;
template class Vector<ElementT>;
