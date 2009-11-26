#ifndef CLICK_FIXCONFIG_H
#define CLICK_FIXCONFIG_H

#if CLICK_LINUXMODULE && HAVE_LINUX_ASM_ALTERNATIVE_H
// The .smp_locks section and C++-style weak linkage interact badly.
# if CONFIG_SMP && (defined(__i386__) || defined(__x86_64__))
#  include <asm/alternative.h>
#  undef LOCK_PREFIX
#  define LOCK_PREFIX "lock ; "
# endif
#endif

#endif
