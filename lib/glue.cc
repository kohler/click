/*
 * glue.{cc,hh} -- minimize portability headaches, and miscellany
 * Robert Morris, Eddie Kohler
 *
 * Copyright (c) 1999-2000 Massachusetts Institute of Technology.
 *
 * This software is being provided by the copyright holders under the GNU
 * General Public License, either version 2 or, at your discretion, any later
 * version. For more information, see the `COPYRIGHT' file in the source
 * distribution.
 */

#ifdef HAVE_CONFIG_H
# include <config.h>
#endif
#include "glue.hh"
#include "error.hh"

#include <limits.h>

#ifdef __KERNEL__

ErrorHandler *click_chatter_errh;

void
click_chatter(const char *fmt, ...)
{
  //va_list args;
  //int i;
  //char buf[256]; /* XXX */
  //
  //va_start(args, fmt);
  //i = vsprintf(buf, fmt, args);
  //va_end(args);
  //
  //if (click_chatter_errh)
  //click_chatter_errh->message("chatter: %s", buf);
  //else
  //printk("<1>%s\n", buf);

  va_list val;
  va_start(val, fmt);
  if (0 && click_chatter_errh)
    click_chatter_errh->verror(ErrorHandler::Message, "chatter", fmt, val);
  else {
    static char buf[512];		// XXX
    int i = vsprintf(buf, fmt, val);
    printk("<1>%s\n", buf);
  }
  va_end(val);
}

// Just for statistics.
unsigned click_new_count = 0;
unsigned click_outstanding_news = 0;

void *
operator new(unsigned int sz)
{
  click_new_count++;
  click_outstanding_news++;
  void *v = kmalloc(sz, GFP_ATOMIC);
  return v;
}

void *
operator new [] (unsigned int sz)
{
  click_new_count++;
  click_outstanding_news++;
  void *v = kmalloc(sz, GFP_ATOMIC);
  return v;
}

void
operator delete(void *addr)
{
  if (addr) {
    click_outstanding_news--;
    kfree(addr);
  }
}

void
operator delete [] (void *addr)
{
  if (addr) {
    click_outstanding_news--;
    kfree(addr);
  }
}

extern "C" {

void
__assert_fail(const char *__assertion,
	      const char *__file,
	      unsigned int __line,
	      const char *__function)
{
  printk("<1>assertion failed %s %s %d %s\n",
	 __assertion,
	 __file,
	 __line,
	 __function);
  panic("Click assertion failed");
}

/*
 * GCC generates calls to these run-time library routines.
 */

void
__pure_virtual()
{
  printk("<1>pure virtual method called\n");
}

void *
__rtti_si()
{
  printk("<1>rtti_si\n");
  return(0);
}

void *
__rtti_user()
{
  printk("<1>rtti_user\n");
  return(0);
}


/*
 * Convert a string to a long integer. use simple_strtoul, which is in the
 * kernel
 */

long
strtol(const char *nptr, char **endptr, int base)
{
  const char *orig_nptr = nptr;
  while (isspace(*nptr))
    nptr++;
  bool negative = (*nptr == '-');
  if (*nptr == '-' || *nptr == '+')
    nptr++;
  if (!isdigit(*nptr)) {
    *endptr = *orig_nptr;
    return INT_MIN;
  }
  unsigned long ul = simple_strtoul(nptr, endptr, base);
  if (ul > LONG_MAX) {
    if (negative && ul == (unsigned long)(LONG_MAX) + 1)
      return LONG_MIN;
    // XXX overflow
    if (negative)
      return LONG_MIN;
    else
      return LONG_MAX;
  } else if (negative)
    return -((long)ul);
  else
    return (long)ul;
}

};

#else /* !__KERNEL__ */

#include <stdarg.h>

void
click_chatter(const char *fmt, ...)
{
  va_list args;
  
  va_start(args, fmt);
  vfprintf(stderr, fmt, args);
  va_end(args);
  
  fprintf(stderr, "\n");
}

int click_new_count; /* dummy */

int
click_jiffies()
{
  struct timeval tv;
  click_gettimeofday(&tv);
  return (tv.tv_sec * 100) + (tv.tv_usec / 10000);
}

#endif /* __KERNEL__ */
