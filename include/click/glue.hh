#ifndef GLUE_HH
#define GLUE_HH

/*
 * Junk required to compile as a Linux kernel module as well as in user-space.
 * Removes many common #include <header>s and abstracts differences between
 * kernel and user space.
 */

// produce debugging output on the console or stderr
void click_chatter(const char *fmt, ...);

#if defined(CLICK_LINUXMODULE) || defined(CLICK_BSDMODULE)

#if CLICK_DMALLOC
extern int click_dmalloc_where;
# define CLICK_DMALLOC_REG(s) do { const unsigned char *__str = reinterpret_cast<const unsigned char *>(s); click_dmalloc_where = (__str[0]<<24) | (__str[1]<<16) | (__str[2]<<8) | __str[3]; } while (0)
#else
# define CLICK_DMALLOC_REG(s)
#endif

#endif	/* CLICK_LINUXMODULE || CLICK_BSDMODULE */

#ifdef CLICK_LINUXMODULE

/*
 * Glue specific to the Linux kernel module
 */

// ask for ino_t, off_t, &c to be defined
#define _LOOSE_KERNEL_NAMES 1
#undef __KERNEL_STRICT_NAMES

#ifndef __OPTIMIZE__
# define __OPTIMIZE__ /* get ntohl() macros. otherwise undefined. */
#endif

#include <click/cxxprotect.h>
CLICK_CXX_PROTECT
#if defined(CLICK_PACKAGE) || defined(WANT_MOD_USE_COUNT)
# define __NO_VERSION__
# include <linux/module.h>
# define HAVE_MOD_USE_COUNT 1
#endif
#include <linux/config.h>
#include <linux/kernel.h>
#include <linux/version.h>
#include <linux/string.h>
#include <linux/skbuff.h>
#include <linux/malloc.h>
#include <linux/ctype.h>
#include <linux/time.h>
#include <linux/errno.h>
CLICK_CXX_UNPROTECT
#include <click/cxxunprotect.h>

// provide a definition for net_device for kernel compatibility
#if LINUX_VERSION_CODE < KERNEL_VERSION(2, 4, 0)
typedef struct device net_device;
#define dev_get_by_name dev_get
#endif

extern "C" {

extern uint32_t click_random_seed;

__inline__ uint32_t
random()
{
  click_random_seed = click_random_seed*69069L + 1;
  return click_random_seed ^ jiffies;
}

extern void srandom(uint32_t);

__inline__ uint64_t
click_get_cycles()
{
#if __i386__
  uint32_t low, high;
  uint64_t x;
  __asm__ __volatile__("rdtsc":"=a" (low), "=d" (high));
  x = high;
  x <<= 32;
  x |= low;
  return x;
#else
  // add other architectures here
  return 0;
#endif
}

long strtol(const char *, char **, int);

__inline__ unsigned long
strtoul(const char *nptr, char **endptr, int base)
{
  return simple_strtoul(nptr, endptr, base);
}

#if LINUX_VERSION_CODE >= KERNEL_VERSION(2, 4, 0) && __GNUC__ == 2 && __GNUC_MINOR__ == 96
int click_strcmp(const char *, const char *);

inline int
strcmp(const char *a, const char *b)
{
  return click_strcmp(a, b);
}
#endif

}

#define CLICK_HZ HZ
#define click_gettimeofday(tvp) (do_gettimeofday(tvp))
#define click_jiffies()		((unsigned)jiffies)

#elif defined(CLICK_BSDMODULE)

/*
 * Glue specific to the BSD kernel module.
 */

#include <click/cxxprotect.h>
CLICK_CXX_PROTECT
#include <sys/ctype.h>
#include <sys/systm.h>
#include <sys/time.h>
#include <sys/param.h>
#include <sys/kernel.h>
#include <sys/mbuf.h>
#include <sys/malloc.h>
#include <sys/libkern.h>
#include <sys/proc.h>
#include <sys/sysproto.h>
CLICK_CXX_UNPROTECT
#include <click/cxxunprotect.h>

#define CLICK_DMALLOC_REG(s)

/* Char-type glue */

#define _U	0x01	/* upper */
#define	_L	0x02	/* lower */
#define	_D	0x04	/* digit */
#define _C	0x08	/* cntrl */
#define _P	0x10	/* punct */
#define _S	0x20	/* white space (space/lf/tab) */
#define	_X	0x40	/* hex digit */
#define	_SP	0x80	/* hard space (0x20) */

extern unsigned char _ctype[];

#define	__ismask(x)	(_ctype[(int)(unsigned char)(x)])
#define	isalnum(c)	((__ismask(c)&(_U|_L|_D)) != 0)

#define	strchr(s, c)			index(s, c)

__inline__ uint64_t
click_get_cycles()
{
  uint32_t low, high;
  uint64_t x;
  __asm__ __volatile__("rdtsc":"=a" (low), "=d" (high));
  x = high;
  x <<= 32;
  x |= low;
  return x;
}

#define	CLICK_HZ hz
#define click_gettimeofday(tvp)	(microtime(tvp))
extern unsigned click_jiffies();

#define	memmove(dst, src, len)		bcopy((src), (dst), (len))

typedef struct ifnet net_device;

#else /* not CLICK_LINUXMODULE || CLICK_BSDMODULE */

/*
 * User-space glue.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <errno.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <sys/time.h>

#define CLICK_DMALLOC_REG(s)

__inline__ unsigned long long
click_get_cycles()
{
  return(0);
}

#define click_gettimeofday(tvp) (gettimeofday(tvp, (struct timezone *) 0))
#define CLICK_HZ 100		// click_jiffies rate
extern unsigned click_jiffies();

// provide a definition for net_device
typedef struct device net_device;

#endif /* CLICK_LINUXMODULE */

#ifndef timercmp
/* Convenience macros for operations on timevals.
   NOTE: `timercmp' does not work for >= or <=.  */
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

/* static assert, for compile-time assertion checking */
#define static_assert(c) switch (c) case 0: case (c):

#ifndef HAVE_MOD_USE_COUNT
# define MOD_INC_USE_COUNT
# define MOD_DEC_USE_COUNT
# define MOD_IN_USE		0
#endif

#endif
