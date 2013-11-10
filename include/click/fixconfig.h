#ifndef CLICK_FIXCONFIG_H
#define CLICK_FIXCONFIG_H

#if CLICK_LINUXMODULE && __cplusplus
// __builtin_choose_expr is not available on C++; create an alternative
// using templates.
template <bool B, typename T, typename F> struct click_conditional;
template <typename T, typename F> struct click_conditional<true, T, F> {
    typedef T type;
};
template <typename T, typename F> struct click_conditional<false, T, F> {
    typedef F type;
};
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
