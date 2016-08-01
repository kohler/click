#ifndef CLICK_FIXCONFIG_H
#define CLICK_FIXCONFIG_H

#if CLICK_LINUXMODULE && __cplusplus
#include <click/type_traits.hh>

#define __builtin_types_compatible_p(a, b) \
    (types_compatible<typeof(a), typeof(b)>::value)
#endif

#if CLICK_LINUXMODULE && HAVE_LINUX_ASM_ALTERNATIVE_H
// The .smp_locks section and C++-style weak linkage interact badly.
# if CONFIG_SMP && (defined(__i386__) || defined(__x86_64__))
#  include <click/cxxprotect.h>
CLICK_CXX_PROTECT
#  include <asm/alternative.h>
#  undef LOCK_PREFIX
#  define LOCK_PREFIX "lock ; "
CLICK_CXX_UNPROTECT
#  include <click/cxxunprotect.h>
# endif
#endif

#endif
