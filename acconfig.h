/* Process this file with autoheader to produce config.h.in */
#ifndef CONFIG_H
#define CONFIG_H

/* Name of package */
#define PACKAGE "click"

/* Version number of package */
#define VERSION "0.1"

/* Define to 1 since we have Strings. */
#define HAVE_STRING 1

/* EXPORT_ELEMENT, ELEMENT_REQUIRES and ELEMENT_PROVIDES are noops. */
#define ELEMENT_REQUIRES(x)
#define ELEMENT_PROVIDES(x)
#define EXPORT_ELEMENT(x)

/* Define if <new.h> exists and works. */
#undef HAVE_NEW_H

/* Define if IPsec support is enabled. */
#undef HAVE_IPSEC

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

/* Define if you have -lpcap and pcap.h. */
#undef HAVE_PCAP

/* Define if you have -ldl. */
#undef HAVE_LIBDL

/* Define if you want to use the stride scheduler. */
#undef HAVE_STRIDE_SCHED
#ifndef HAVE_STRIDE_SCHED
# define RR_SCHED
#endif

/* Prototype strerror if we don't have it. */
#ifndef CLICK_LINUXMODULE
# ifndef HAVE_STRERROR
char *strerror(int errno);
# endif
#endif

#ifdef __cplusplus
/* Get rid of inline macro under C++ */
# undef inline
#endif
#endif
