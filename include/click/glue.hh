// -*- c-basic-offset: 2; related-file-name: "../../lib/glue.cc" -*-
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
# if defined(CLICK_PACKAGE) || defined(WANT_MOD_USE_COUNT)
#  define __NO_VERSION__
#  include <linux/module.h>
#  define HAVE_MOD_USE_COUNT 1
# endif
# include <linux/config.h>
# include <linux/kernel.h>
# include <linux/version.h>
# include <linux/string.h>
# include <linux/skbuff.h>
# include <linux/malloc.h>
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
# include <sys/socket.h>
# include <netinet/in.h>
# include <sys/time.h>

#endif


// DEBUGGING OUTPUT

void click_chatter(const char *fmt, ...);


// DEBUG MALLOC

#if CLICK_DMALLOC && (CLICK_LINUXMODULE || CLICK_BSDMODULE)
extern int click_dmalloc_where;
# define CLICK_DMALLOC_REG(s) do { const unsigned char *__str = reinterpret_cast<const unsigned char *>(s); click_dmalloc_where = (__str[0]<<24) | (__str[1]<<16) | (__str[2]<<8) | __str[3]; } while (0)
#else
# define CLICK_DMALLOC_REG(s)
#endif


// RANDOMNESS

extern void click_random_srandom(); // srand(), but use true randomness

#if CLICK_LINUXMODULE
extern "C" {
extern uint32_t click_random_seed;
extern void srandom(uint32_t);
inline uint32_t
random()
{
  click_random_seed = click_random_seed*69069L + 1;
  return click_random_seed ^ jiffies;
}
}
#endif


// SORTING

#if CLICK_LINUXMODULE || CLICK_BSDMODULE
extern "C" {
void click_qsort(void *base, size_t n, size_t size, int (*compar)(const void *, const void *));
}
#else
# define click_qsort qsort
#endif


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


// MODULE USE COUNTS

#ifndef HAVE_MOD_USE_COUNT
# define MOD_INC_USE_COUNT
# define MOD_DEC_USE_COUNT
# define MOD_IN_USE		0
#endif


// TIMEVALS AND JIFFIES

#if CLICK_LINUXMODULE
# define click_gettimeofday(tvp)	(do_gettimeofday(tvp))
# define click_jiffies()		((unsigned)jiffies)
# define CLICK_HZ			HZ
#elif CLICK_BSDMODULE
# define click_gettimeofday(tvp)	(getmicrotime(tvp))
# define click_jiffies()		((unsigned)ticks)
# define CLICK_HZ			hz
#else
# define click_gettimeofday(tvp)	(gettimeofday(tvp, (struct timezone *)0))
unsigned click_jiffies();
# define CLICK_HZ			100
#endif


// TIMEVAL OPERATIONS

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

#ifndef CLICK_TIMEVAL_OPERATORS

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


// CYCLE COUNTS

#ifdef HAVE_INT64_TYPES
# if CLICK_LINUXMODULE

inline uint64_t
click_get_cycles()
{
#  if __i386__
  uint32_t low, high;
  uint64_t x;
  __asm__ __volatile__("rdtsc":"=a" (low), "=d" (high));
  x = high;
  x <<= 32;
  x |= low;
  return x;
#  else
  // add other architectures here
  return 0;
#  endif
}

# elif CLICK_BSDMODULE

#  define click_get_cycles()		rdtsc()

# else

inline uint64_t
click_get_cycles()
{
  return 0;
}

# endif
#endif

#endif
