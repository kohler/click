// -*- c-basic-offset: 4; related-file-name: "../../lib/glue.cc" -*-
#ifndef CLICK_GLUE_HH
#define CLICK_GLUE_HH
// Removes many common #include <header>s and abstracts differences between
// kernel and user space, and between operating systems.

// HEADERS

#if CLICK_LINUXMODULE

# define _LOOSE_KERNEL_NAMES 1 /* define ino_t, off_t, etc. */
# undef __KERNEL_STRICT_NAMES
# ifndef __OPTIMIZE__
#  define __OPTIMIZE__ 1 /* get ntohl() macros. otherwise undefined. */
# endif
# include <click/cxxprotect.h>
CLICK_CXX_PROTECT
# ifdef WANT_MOD_USE_COUNT
#  define __NO_VERSION__
#  include <linux/module.h>
#  define HAVE_MOD_USE_COUNT 1
# endif
# include <linux/kernel.h>
# include <linux/version.h>
# include <linux/string.h>
# include <linux/skbuff.h>
# if LINUX_VERSION_CODE < KERNEL_VERSION(2, 6, 0)
#  include <linux/malloc.h>
#  include <linux/vmalloc.h>
#  include <linux/interrupt.h>
# else
#  include <linux/hardirq.h>
# endif
# include <linux/ctype.h>
# include <linux/time.h>
# include <linux/errno.h>
CLICK_CXX_UNPROTECT
# include <click/cxxunprotect.h>

#elif CLICK_BSDMODULE

# include <click/cxxprotect.h>
CLICK_CXX_PROTECT
# include <sys/ctype.h>
# include <sys/systm.h>
# include <sys/time.h>
# include <sys/param.h>
# include <sys/kernel.h>
# include <sys/mbuf.h>
# include <sys/malloc.h>
# include <sys/libkern.h>
# include <sys/proc.h>
# include <sys/sysproto.h>
CLICK_CXX_UNPROTECT
# include <click/cxxunprotect.h>

#else /* CLICK_USERLEVEL */

# include <stdio.h>
# include <stdlib.h>
# include <string.h>
# include <ctype.h>
# include <errno.h>
# include <limits.h>
# include <sys/socket.h>
# include <netinet/in.h>
# include <sys/time.h>
# if CLICK_NS
extern "C" int simclick_gettimeofday(struct timeval *);
# endif
# if HAVE_MULTITHREAD
#  include <pthread.h>
# endif

#endif


// DEBUGGING OUTPUT
extern "C" {
void click_chatter(const char *fmt, ...);
}


// DEBUG MALLOC

#if CLICK_DMALLOC && (CLICK_LINUXMODULE || CLICK_BSDMODULE)
extern uint32_t click_dmalloc_where;
# define CLICK_DMALLOC_REG(s) do { const unsigned char *__str = reinterpret_cast<const unsigned char *>(s); click_dmalloc_where = (__str[0]<<24) | (__str[1]<<16) | (__str[2]<<8) | __str[3]; } while (0)
#else
# define CLICK_DMALLOC_REG(s)
#endif


// LALLOC

#if CLICK_LINUXMODULE
# define CLICK_LALLOC(size)	(click_lalloc((size)))
# define CLICK_LFREE(p, size)	(click_lfree((p), (size)))
extern "C" {
void *click_lalloc(size_t size);
void click_lfree(volatile void *p, size_t size);
}
#else
# define CLICK_LALLOC(size)	((void *)(new uint8_t[(size)]))
# define CLICK_LFREE(p, size)	delete[] ((uint8_t *)(p))
#endif


// RANDOMNESS

CLICK_DECLS

/** @brief Return a number between 0 and CLICK_RAND_MAX, inclusive.
 *
 * CLICK_RAND_MAX is guaranteed to be at least 2^31 - 1. */
uint32_t click_random();

/** @brief Return a number between @a low and @a high, inclusive.
 *
 * Returns @a low if @a low >= @a high. */
uint32_t click_random(uint32_t low, uint32_t high);

/** @brief Set the click_random() seed to @a seed. */
void click_srandom(uint32_t seed);

/** @brief Set the click_random() seed using a source of true randomness,
 * if available. */
void click_random_srandom();

#if CLICK_BSDMODULE
# define CLICK_RAND_MAX 0x7FFFFFFFU
#elif !CLICK_LINUXMODULE && RAND_MAX >= 0x7FFFFFFFU
# define CLICK_RAND_MAX RAND_MAX
#else
# define CLICK_RAND_MAX 0x7FFFFFFFU
extern uint32_t click_random_seed;
#endif

inline uint32_t click_random() {
#if CLICK_BSDMODULE
    return random();
#elif CLICK_LINUXMODULE
    click_random_seed = click_random_seed * 69069L + 5;
    return (click_random_seed ^ jiffies) & RAND_MAX; // XXX jiffies??
#elif HAVE_RANDOM && CLICK_RAND_MAX == RAND_MAX
    return random();
#else
    return rand();
#endif
}

inline void click_srandom(uint32_t seed) {
#if CLICK_BSDMODULE
    srandom(seed);
#elif !CLICK_LINUXMODULE && HAVE_RANDOM && CLICK_RAND_MAX == RAND_MAX
    srandom(seed);
#elif !CLICK_LINUXMODULE && CLICK_RAND_MAX == RAND_MAX
    srand(seed);
#else
    click_random_seed = seed;
#endif
}

CLICK_ENDDECLS


// SORTING

int click_qsort(void *base, size_t n, size_t size, int (*compar)(const void *, const void *, void *), void *thunk);
int click_qsort(void *base, size_t n, size_t size, int (*compar)(const void *, const void *));


// OTHER

#if CLICK_LINUXMODULE

// provide a definition for net_device for kernel compatibility
# if LINUX_VERSION_CODE < KERNEL_VERSION(2, 4, 0)
typedef struct device net_device;
#  define dev_get_by_name dev_get
# endif

extern "C" {

long strtol(const char *, char **, int);

inline unsigned long
strtoul(const char *nptr, char **endptr, int base)
{
    return simple_strtoul(nptr, endptr, base);
}

# if LINUX_VERSION_CODE >= KERNEL_VERSION(2, 4, 0) && __GNUC__ == 2 && __GNUC_MINOR__ == 96
int click_strcmp(const char *, const char *);

inline int
strcmp(const char *a, const char *b)
{
    return click_strcmp(a, b);
}
# endif

}

#elif CLICK_BSDMODULE

/* Char-type glue */

# define _U	0x01	/* upper */
# define _L	0x02	/* lower */
# define _D	0x04	/* digit */
# define _C	0x08	/* cntrl */
# define _P	0x10	/* punct */
# define _S	0x20	/* white space (space/lf/tab) */
# define _X	0x40	/* hex digit */
# define _SP	0x80	/* hard space (0x20) */

extern unsigned char _ctype[];

# define __ismask(x)	(_ctype[(int)(unsigned char)(x)])
# define isalnum(c)	((__ismask(c)&(_U|_L|_D)) != 0)

# define strchr(s, c)	index(s, c)

# define memmove(dst, src, len)		bcopy((src), (dst), (len))

typedef struct ifnet net_device;

#else /* not CLICK_LINUXMODULE || CLICK_BSDMODULE */

// provide a definition for net_device
typedef struct device net_device;

#endif /* CLICK_LINUXMODULE */


// COMPILE-TIME ASSERTION CHECKING

#define static_assert(c) switch (c) case 0: case (c):


// PROCESSOR IDENTITIES

#if CLICK_LINUXMODULE
typedef uint32_t click_processor_t;
#elif CLICK_USERLEVEL && HAVE_MULTITHREAD
typedef pthread_t click_processor_t;
#else
typedef int8_t click_processor_t;
#endif

inline click_processor_t
click_current_processor()
{
#if CLICK_LINUXMODULE
# if LINUX_VERSION_CODE >= KERNEL_VERSION(2, 6, 0)
    return current_thread_info()->cpu;
# else
    return current->processor;
# endif
#elif CLICK_USERLEVEL && HAVE_MULTITHREAD
    return pthread_self();
#else
    return 0;
#endif
}

inline click_processor_t
click_invalid_processor()
{
#if CLICK_LINUXMODULE
    return -1;
#elif CLICK_USERLEVEL && HAVE_MULTITHREAD
    return 0;
#else
    return -1;
#endif
}


// TIMEVALS AND JIFFIES
// click_jiffies_t is the type of click_jiffies() and is usually unsigned.
// click_jiffies_difference_t is the signed version of click_jiffies_t.
// CLICK_JIFFIES_MONOTONIC is true if click_jiffies() never goes backwards.

#if CLICK_LINUXMODULE
# define click_gettimeofday(tvp)	(do_gettimeofday(tvp))
typedef unsigned long click_jiffies_t;
typedef long click_jiffies_difference_t;
# define click_jiffies()		(jiffies)
# define CLICK_JIFFIES_MONOTONIC	1
# define CLICK_HZ			HZ
# define click_jiffies_less(a, b)	((click_jiffies_difference_t) ((a) - (b)) < 0)
#elif CLICK_BSDMODULE
# define click_gettimeofday(tvp)	(getmicrotime(tvp))
typedef int click_jiffies_t;
typedef int click_jiffies_difference_t;
# define click_jiffies()		(ticks)
# define CLICK_HZ			hz
# define click_jiffies_less(a, b)	((click_jiffies_difference_t) ((a) - (b)) < 0)
#else
#if !CLICK_NS
# define click_gettimeofday(tvp)	(gettimeofday(tvp, (struct timezone *)0))
#else
# define click_gettimeofday(tvp)	(simclick_gettimeofday(tvp))
#endif
CLICK_DECLS
typedef unsigned click_jiffies_t;
typedef int click_jiffies_difference_t;
click_jiffies_t click_jiffies();
# define click_jiffies_less(a, b)	((click_jiffies_difference_t) ((a) - (b)) < 0)
CLICK_ENDDECLS
# define CLICK_HZ			1000
#endif


// TIMEVAL OPERATIONS

#ifndef timercmp
// Convenience macros for operations on timevals.
// NOTE: 'timercmp' does not work for >= or <=.
# define timerisset(tvp)	((tvp)->tv_sec || (tvp)->tv_usec)
# define timerclear(tvp)	((tvp)->tv_sec = (tvp)->tv_usec = 0)
# define timercmp(a, b, CMP)			\
  (((a)->tv_sec == (b)->tv_sec) ?		\
   ((a)->tv_usec CMP (b)->tv_usec) :		\
   ((a)->tv_sec CMP (b)->tv_sec))
#endif
#ifndef timeradd
# define timeradd(a, b, result)						      \
  do {									      \
    (result)->tv_sec = (a)->tv_sec + (b)->tv_sec;			      \
    (result)->tv_usec = (a)->tv_usec + (b)->tv_usec;			      \
    if ((result)->tv_usec >= 1000000)					      \
      {									      \
	++(result)->tv_sec;						      \
	(result)->tv_usec -= 1000000;					      \
      }									      \
  } while (0)
#endif
#ifndef timersub
# define timersub(a, b, result)						      \
  do {									      \
    (result)->tv_sec = (a)->tv_sec - (b)->tv_sec;			      \
    (result)->tv_usec = (a)->tv_usec - (b)->tv_usec;			      \
    if ((result)->tv_usec < 0) {					      \
      --(result)->tv_sec;						      \
      (result)->tv_usec += 1000000;					      \
    }									      \
  } while (0)
#endif

#ifndef CLICK_TIMEVAL_OPERATORS

inline struct timeval
make_timeval(int sec, int usec)
{
    struct timeval tv;
    tv.tv_sec = sec;
    tv.tv_usec = usec;
    return tv;
}

inline bool
operator==(const struct timeval &a, const struct timeval &b)
{
    return a.tv_sec == b.tv_sec && a.tv_usec == b.tv_usec;
}

inline bool
operator!=(const struct timeval &a, const struct timeval &b)
{
    return a.tv_sec != b.tv_sec || a.tv_usec != b.tv_usec;
}

inline bool
operator<(const struct timeval &a, const struct timeval &b)
{
    return a.tv_sec < b.tv_sec || (a.tv_sec == b.tv_sec && a.tv_usec < b.tv_usec);
}

inline bool
operator<=(const struct timeval &a, const struct timeval &b)
{
    return a.tv_sec < b.tv_sec || (a.tv_sec == b.tv_sec && a.tv_usec <= b.tv_usec);
}

inline bool
operator>=(const struct timeval &a, const struct timeval &b)
{
    return a.tv_sec > b.tv_sec || (a.tv_sec == b.tv_sec && a.tv_usec >= b.tv_usec);
}

inline bool
operator>(const struct timeval &a, const struct timeval &b)
{
    return a.tv_sec > b.tv_sec || (a.tv_sec == b.tv_sec && a.tv_usec > b.tv_usec);
}

inline struct timeval &
operator+=(struct timeval &a, const struct timeval &b)
{
    a.tv_sec += b.tv_sec;
    a.tv_usec += b.tv_usec;
    if (a.tv_usec >= 1000000) {
	a.tv_sec++;
	a.tv_usec -= 1000000;
    }
    return a;
}

inline struct timeval &
operator-=(struct timeval &a, const struct timeval &b)
{
    a.tv_sec -= b.tv_sec;
    a.tv_usec -= b.tv_usec;
    if (a.tv_usec < 0) {
	a.tv_sec--;
	a.tv_usec += 1000000;
    }
    return a;
}

inline struct timeval
operator+(struct timeval a, const struct timeval &b)
{
    a += b;
    return a;
}

inline struct timeval
operator-(struct timeval a, const struct timeval &b)
{
    a -= b;
    return a;
}

#endif

CLICK_DECLS
class StringAccum;
StringAccum &operator<<(StringAccum &, const struct timeval &);
CLICK_ENDDECLS


// BYTE ORDER

#if CLICK_BYTE_ORDER == CLICK_LITTLE_ENDIAN
# define le16_to_cpu(x) (x)
# define cpu_to_le16(x) (x)
# define le32_to_cpu(x) (x)
# define cpu_to_le32(x) (x)
#elif CLICK_BYTE_ORDER == CLICK_BIG_ENDIAN && defined(__APPLE__)
# include <machine/byte_order.h>
# define le16_to_cpu(x) NXSwapShort((x))
# define cpu_to_le16(x) NXSwapShort((x))
# define le32_to_cpu(x) NXSwapInt((x))
# define cpu_to_le32(x) NXSwapInt((x))
#elif CLICK_BYTE_ORDER == CLICK_BIG_ENDIAN && HAVE_BYTESWAP_H
# include <byteswap.h>
# define le16_to_cpu(x) bswap_16((x))
# define cpu_to_le16(x) bswap_16((x))
# define le32_to_cpu(x) bswap_32((x))
# define cpu_to_le32(x) bswap_32((x))
#else
/* leave them undefined */
#endif


// CYCLE COUNTS

CLICK_DECLS

#if HAVE_INT64_TYPES
typedef uint64_t click_cycles_t;
#else
typedef uint32_t click_cycles_t;
#endif

inline click_cycles_t
click_get_cycles()
{
#if CLICK_LINUXMODULE && HAVE_INT64_TYPES && __i386__
    uint64_t x;
    __asm__ __volatile__ ("rdtsc" : "=A" (x));
    return x;
#elif CLICK_LINUXMODULE && HAVE_INT64_TYPES && __x86_64__
    uint32_t xlo, xhi;
    __asm__ __volatile__ ("rdtsc" : "=a" (xlo), "=d" (xhi));
    return xlo | (((uint64_t) xhi) << 32);
#elif CLICK_LINUXMODULE && __i386__
    uint32_t xlo, xhi;
    __asm__ __volatile__ ("rdtsc" : "=a" (xlo), "=d" (xhi));
    return xlo;
#elif CLICK_BSDMODULE
    return rdtsc();
#elif CLICK_USERLEVEL && HAVE_INT64_TYPES && __i386__
    uint64_t x;
    __asm__ __volatile__ ("rdtsc" : "=A" (x));
    return x;
#elif CLICK_USERLEVEL && HAVE_INT64_TYPES && __x86_64__
    uint32_t xlo, xhi;
    __asm__ __volatile__ ("rdtsc" : "=a" (xlo), "=d" (xhi));
    return xlo | (((uint64_t) xhi) << 32);
#elif CLICK_USERLEVEL && __i386__
    uint32_t xlo, xhi;
    __asm__ __volatile__ ("rdtsc" : "=a" (xlo), "=d" (xhi));
    return xlo;
#else
    // add other architectures here
    return 0;
#endif
}

CLICK_ENDDECLS

#endif
