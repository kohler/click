/* Process this file with autoheader to produce config.h.in */
#ifndef CLICK_CONFIG_H
#define CLICK_CONFIG_H

/* Version number of package */
#define CLICK_VERSION "0.1"

/* Define to 1 since we have Strings. */
#define HAVE_STRING 1

/* EXPORT_ELEMENT, ELEMENT_REQUIRES, ELEMENT_PROVIDES, and ELEMENT_MT_SAFE
   are noops. */
#define EXPORT_ELEMENT(x)
#define ELEMENT_REQUIRES(x)
#define ELEMENT_PROVIDES(x)
#define ELEMENT_MT_SAFE(x)

/* Define if <new.h> exists and works. */
#undef HAVE_NEW_H

/* Define if IPsec support is enabled. */
#undef HAVE_IPSEC

/* Define if IPv6 support is enabled. */
#undef HAVE_IP6

/* Define if <pcap.h> uses bpf_timeval. */
#undef HAVE_BPF_TIMEVAL

@TOP@
@BOTTOM@

/* Define stuff under a Linux module. */
#ifdef CLICK_LINUXMODULE
# ifndef __linux__
#  error "I must be compiled on a Linux machine"
# endif
# define __KERNEL__
# define MODULE
# define NDEBUG 1
#endif

/* The largest size a write handler is allowed to have. */
#define LARGEST_HANDLER_WRITE 16384

/* Define if a Linux kernel module is a possible target. */
#undef HAVE_LINUXMODULE_TARGET

/* Define if your Linux kernel has Click extensions. */
#undef HAVE_CLICK_KERNEL

/* Define if your Linux kernel has read_net_skbcount. */
#undef HAVE_READ_NET_SKBCOUNT

/* Define if your Linux kernel exposes tcp_prot. */
#undef HAVE_TCP_PROT

/* Define if your Linux kernel has polling extensions. */
#undef HAVE_POLLING

/* Define if your Linux kernel has sk_buff reuse extensions. */
#undef HAVE_REUSE_SKB

/* Define if your Linux kernel has tulip_interrupt_hook. */
#undef HAVE_TULIP_INTERRUPT_HOOK

/* Define if your Linux kernel is running in SMP mode. */
#ifdef __KERNEL__
#undef __SMP__
#endif

/* Define if you both need and have -lpcap and pcap.h. */
#undef HAVE_PCAP

/* Define if you have -ldl. */
#undef HAVE_LIBDL

/* Define if you want to run multithreaded Click. */
#undef __MTCLICK__
#undef NUM_CLICK_CPUS

/* Define if you want to use intel specific instructions */
#undef HAVE_INTEL_CPU

/* Define if you want to use the stride scheduler. */
#undef HAVE_STRIDE_SCHED
#ifndef HAVE_STRIDE_SCHED
# define RR_SCHED
#endif

/* Define for Click memory allocation debugging. */
#undef CLICK_DMALLOC

/* Prototype strerror if we don't have it. */
#ifndef CLICK_LINUXMODULE
# ifndef HAVE_STRERROR
char *strerror(int errno);
# endif
#endif

/* Define u_intXX_t in terms of uintXX_t if necessary. */
#undef SUBSTITUTE_U_INT_TYPES
#ifdef SUBSTITUTE_U_INT_TYPES
# define u_int32_t uint32_t
# define u_int16_t uint16_t
#endif

/* Define if fast checksum functions available. */
#ifdef CLICK_LINUXMODULE
# define HAVE_FAST_CHECKSUM 1
#endif

/* Define if fast checksum functions require correct alignment. */
#ifndef __i386__
# define FAST_CHECKSUM_ALIGNED 1
#endif

#ifdef __cplusplus
/* Get rid of inline macro under C++. */
#undef inline

/* Provide placement new. */
#ifdef __KERNEL__
# include <linux/types.h>
inline void *operator new(size_t, void *v) { return v; }
#elif HAVE_NEW_H
# include <new.h>
#else
inline void *operator new(size_t, void *v) { return v; }
#endif
#define HAVE_PLACEMENT_NEW 1

/* Explicit template instances? */
#if __GNUC__ && __GNUC_MINOR__ < 90
#define EXPLICIT_TEMPLATE_INSTANCES 1
#endif
#endif

#endif CLICK_CONFIG_H
