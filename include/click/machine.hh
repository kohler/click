#ifndef CLICK_MACHINE_HH
#define CLICK_MACHINE_HH 1
#if CLICK_LINUXMODULE
# include <click/cxxprotect.h>
CLICK_CXX_PROTECT
# include <linux/threads.h>
# include <linux/sched.h>
# if defined(CONFIG_SMP) && LINUX_VERSION_CODE < KERNEL_VERSION(2, 6, 0)
#  define num_possible_cpus()	smp_num_cpus
# endif
CLICK_CXX_UNPROTECT
# include <click/cxxunprotect.h>
#endif

/** @brief Compiler fence.

    Prevents reordering of loads and stores by the compiler. Not intended to
    synchronize the processor's caches. */
inline void
click_compiler_fence()
{
    asm volatile("" : : : "memory");
}

/** @brief Compiler fence that relaxes the processor.

    Use this in spinloops, for example. */
inline void
click_relax_fence()
{
#if CLICK_LINUXMODULE
    cpu_relax();
#elif HAVE_MULTITHREAD && (defined(__i386__) || defined(__arch_um__) || defined(__x86_64__))
    asm volatile("rep; nop" : : : "memory"); // equivalent to "pause"
#else
    click_compiler_fence();
#endif
}

/** @brief Full memory fence. */
inline void
click_fence()
{
#if CLICK_LINUXMODULE
    smp_mb();
#elif HAVE_MULTITHREAD && (defined(__i386__) || defined(__arch_um__) || defined(__x86_64__))
    // GCC 4.2.1 on Mac has __sync_synchronize, but it doesn't work!
    asm volatile("mfence" : : : "memory");
#elif HAVE_MULTITHREAD && HAVE___SYNC_SYNCHRONIZE
    __sync_synchronize();
#else
    click_compiler_fence();
#endif
}

#endif
