#ifdef HAVE_CONFIG_H
# include <config.h>
#endif
#include "vector.cc"
#include "string.hh"
#include "router.hh"
template class Vector<Router::Hookup>;
template class Vector<int>;
template class Vector<unsigned int>;
template class Vector<String>;
