// -*- c-basic-offset: 4; related-file-name: "../include/click/glue.hh" -*-
/*
 * glue.{cc,hh} -- minimize portability headaches, and miscellany
 * Robert Morris, Eddie Kohler
 *
 * Copyright (c) 1999-2000 Massachusetts Institute of Technology
 *
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files (the "Software"),
 * to deal in the Software without restriction, subject to the conditions
 * listed in the Click LICENSE file. These conditions include: you must
 * preserve this copyright notice, and you cannot mention the copyright
 * holders in advertising related to the Software without their permission.
 * The Software is provided WITHOUT ANY WARRANTY, EXPRESS OR IMPLIED. This
 * notice is a summary of the Click LICENSE file; the license in that file is
 * legally binding.
 */

#include <click/config.h>

#include <click/glue.hh>
#include <click/error.hh>

#ifdef CLICK_USERLEVEL
# include <limits.h>
# include <stdarg.h>
# include <unistd.h>
# include <sys/types.h>
# include <sys/stat.h>
# include <fcntl.h>
#endif

// Include header structures so we can check their sizes with static_assert.
#include <clicknet/ether.h>
#include <clicknet/fddi.h>
#include <clicknet/ip.h>
#include <clicknet/ip6.h>
#include <clicknet/icmp.h>
#include <clicknet/tcp.h>
#include <clicknet/udp.h>
#include <clicknet/rfc1483.h>

void
click_check_header_sizes()
{
    // <clicknet/ether.h>
    static_assert(sizeof(click_ether) == 14);
    static_assert(sizeof(click_arp) == 8);
    static_assert(sizeof(click_ether_arp) == 28);
    static_assert(sizeof(click_nd_sol) == 32);
    static_assert(sizeof(click_nd_adv) == 32);
    static_assert(sizeof(click_nd_adv2) == 24);

    // <clicknet/ip.h>
    static_assert(sizeof(click_ip) == 20);

    // <clicknet/icmp.h>
    static_assert(sizeof(icmp_generic) == 8);
    static_assert(sizeof(icmp_param) == 8);
    static_assert(sizeof(icmp_redirect) == 8);
    static_assert(sizeof(icmp_sequenced) == 8);
    static_assert(sizeof(icmp_time) == 20);

    // <clicknet/tcp.h>
    static_assert(sizeof(click_tcp) == 20);

    // <clicknet/udp.h>
    static_assert(sizeof(click_udp) == 8);

    // <clicknet/ip6.h>
    static_assert(sizeof(click_ip6) == 40);

    // <clicknet/fddi.h>
    static_assert(sizeof(click_fddi) == 13);
    static_assert(sizeof(click_fddi_8022_1) == 16);
    static_assert(sizeof(click_fddi_8022_2) == 17);
    static_assert(sizeof(click_fddi_snap) == 21);

    // <clicknet/rfc1483.h>
    static_assert(sizeof(click_rfc1483) == 8);
}


// DEBUGGING OUTPUT

void
click_chatter(const char *fmt, ...)
{
  va_list val;
  va_start(val, fmt);

  if (ErrorHandler::has_default_handler()) {
    ErrorHandler *errh = ErrorHandler::default_handler();
    errh->verror(ErrorHandler::ERR_MESSAGE, "", fmt, val);
  } else {
#if CLICK_LINUXMODULE
# if __MTCLICK__
    static char buf[NR_CPUS][512];	// XXX
    int i = vsprintf(buf[current->processor], fmt, val);
    printk("<1>%s\n", buf[current->processor]);
# else
    static char buf[512];		// XXX
    int i = vsprintf(buf, fmt, val);
    printk("<1>%s\n", buf);
# endif
#elif CLICK_BSDMODULE
    vprintf(fmt, val);
#else /* User-space */
    vfprintf(stderr, fmt, val);
    fprintf(stderr, "\n");
#endif
  }
  
  va_end(val);
}


// DEBUG MALLOC

unsigned click_new_count = 0;
unsigned click_outstanding_news = 0;
uint32_t click_dmalloc_where = 0x3F3F3F3F;

#if CLICK_LINUXMODULE || CLICK_BSDMODULE

# if CLICK_LINUXMODULE
#  define CLICK_ALLOC(size)	kmalloc((size), GFP_ATOMIC)
#  define CLICK_FREE(ptr)	kfree((ptr))
# else
#  define CLICK_ALLOC(size)	malloc((size), M_TEMP, M_WAITOK)
#  define CLICK_FREE(ptr)	free(ptr, M_TEMP)
# endif

# if CLICK_DMALLOC
#  define CHUNK_MAGIC		0xffff3f7f	/* -49281 */
#  define CHUNK_MAGIC_FREED	0xc66b04f5
struct Chunk {
    uint32_t magic;
    uint32_t size;
    uint32_t where;
    Chunk *prev;
    Chunk *next;
};
static Chunk chunks = {
    CHUNK_MAGIC, 0, 0, &chunks, &chunks
};

static char *
printable_where(Chunk *c)
{
  static char wherebuf[13];
  const char *hexstr = "0123456789ABCDEF";
  char *s = wherebuf;
  for (int i = 24; i >= 0; i -= 8) {
    int ch = (c->where >> i) & 0xFF;
    if (ch >= 32 && ch < 127)
      *s++ = ch;
    else {
      *s++ = '%';
      *s++ = hexstr[(ch>>4) & 0xF];
      *s++ = hexstr[ch & 0xF];
    }
  }
  *s++ = 0;
  return wherebuf;
}
# endif

void *
operator new(unsigned int sz) throw ()
{
  click_new_count++;
  click_outstanding_news++;
# if CLICK_DMALLOC
  void *v = CLICK_ALLOC(sz + sizeof(Chunk));
  Chunk *c = (Chunk *)v;
  c->magic = CHUNK_MAGIC;
  c->size = sz;
  c->where = click_dmalloc_where;
  c->prev = &chunks;
  c->next = chunks.next;
  c->next->prev = chunks.next = c;
  return (void *)((unsigned char *)v + sizeof(Chunk));
# else
  return CLICK_ALLOC(sz);
# endif
}

void *
operator new[](unsigned int sz) throw ()
{
  click_new_count++;
  click_outstanding_news++;
# if CLICK_DMALLOC
  void *v = CLICK_ALLOC(sz + sizeof(Chunk));
  Chunk *c = (Chunk *)v;
  c->magic = CHUNK_MAGIC;
  c->size = sz;
  c->where = click_dmalloc_where;
  c->prev = &chunks;
  c->next = chunks.next;
  c->next->prev = chunks.next = c;
  return (void *)((unsigned char *)v + sizeof(Chunk));
# else
  return CLICK_ALLOC(sz);
# endif
}

void
operator delete(void *addr)
{
  if (addr) {
    click_outstanding_news--;
# if CLICK_DMALLOC
    Chunk *c = (Chunk *)((unsigned char *)addr - sizeof(Chunk));
    if (c->magic == CHUNK_MAGIC_FREED) {
      click_chatter("click error: double-free of memory at %p (%u @ %s)\n",
		    addr, c->size, printable_where(c));
      return;
    }
    if (c->magic != CHUNK_MAGIC) {
      click_chatter("click error: memory corruption on delete %p\n", addr);
      return;
    }
    c->magic = CHUNK_MAGIC_FREED;
    c->prev->next = c->next;
    c->next->prev = c->prev;
    CLICK_FREE((void *)c);
# else
    CLICK_FREE(addr);
# endif
  }
}

void
operator delete[](void *addr)
{
  if (addr) {
    click_outstanding_news--;
# if CLICK_DMALLOC
    Chunk *c = (Chunk *)((unsigned char *)addr - sizeof(Chunk));
    if (c->magic == CHUNK_MAGIC_FREED) {
      click_chatter("click error: double-free of memory at %p (%u @ %s)\n",
		    addr, c->size, printable_where(c));
      return;
    }
    if (c->magic != CHUNK_MAGIC) {
      click_chatter("click error: memory corruption on delete[] %p\n", addr);
      return;
    }
    c->magic = CHUNK_MAGIC_FREED;
    c->prev->next = c->next;
    c->next->prev = c->prev;
    CLICK_FREE((void *)c);
# else
    CLICK_FREE(addr);
# endif
  }
}

void
click_dmalloc_cleanup()
{
# if CLICK_DMALLOC
  while (chunks.next != &chunks) {
    Chunk *c = chunks.next;
    chunks.next = c->next;
    c->next->prev = &chunks;

    click_chatter("  chunk at %p size %d alloc[%s] data ",
		  (void *)(c + 1), c->size, printable_where(c));
    unsigned char *d = (unsigned char *)(c + 1);
    for (int i = 0; i < 20 && i < c->size; i++)
      click_chatter("%02x", d[i]);
    click_chatter("\n");
    CLICK_FREE((void *)c);
  }
# endif
}

#endif /* CLICK_LINUXMODULE || CLICK_BSDMODULE */


// RANDOMNESS

void
click_random_srandom()
{
  static const int bufsiz = 16;
  uint32_t buf[bufsiz];
  int pos = 0;
  click_gettimeofday((struct timeval *)(buf + pos));
  pos += sizeof(struct timeval) / sizeof(uint32_t);
#ifdef CLICK_USERLEVEL
# ifdef O_NONBLOCK
  int fd = open("/dev/random", O_RDONLY | O_NONBLOCK);
# elif defined(O_NDELAY)
  int fd = open("/dev/random", O_RDONLY | O_NDELAY);
# else
  int fd = open("/dev/random", O_RDONLY);
# endif
  if (fd >= 0) {
    int amt = read(fd, buf + pos, sizeof(uint32_t) * (bufsiz - pos));
    close(fd);
    if (amt > 0)
      pos += (amt / sizeof(uint32_t));
  }
  if (pos < bufsiz)
    buf[pos++] = getpid();
  if (pos < bufsiz)
    buf[pos++] = getuid();
#endif

  uint32_t result = 0;
  for (int i = 0; i < pos; i++) {
    result ^= buf[i];
    result = (result << 1) | (result >> 31);
  }
  srandom(result);
}

#if CLICK_LINUXMODULE
extern "C" {
uint32_t click_random_seed = 152;

void
srandom(uint32_t seed)
{
    click_random_seed = seed;
}
}
#endif


// SORTING

#if CLICK_LINUXMODULE || CLICK_BSDMODULE
extern "C" {

static int
click_qsort_partition(void *base_v, size_t size, int left, int right,
		      int (*compar)(const void *, const void *),
		      int &split_left, int &split_right)
{
    if (size >= 64) {
	printk("<1>click_qsort_partition: elements too large!\n");
	return -E2BIG;
    }
    
    uint8_t pivot[64], tmp[64];
    uint8_t *base = reinterpret_cast<uint8_t *>(base_v);

    // Dutch national flag algorithm
    int middle = left;
    memcpy(&pivot[0], &base[size * ((left + right) / 2)], size);

    // loop invariant:
    // base[i] < pivot for all left_init <= i < left
    // base[i] > pivot for all right < i <= right_init
    // base[i] == pivot for all left <= i < middle
    while (middle <= right) {
	int cmp = compar(&base[size * middle], &pivot[0]);
	if (cmp < 0) {
	    memcpy(&tmp[0], &base[size * left], size);
	    memcpy(&base[size * left], &base[size * middle], size);
	    memcpy(&base[size * middle], &tmp[0], size);
	    left++;
	    middle++;
	} else if (cmp > 0) {
	    memcpy(&tmp[0], &base[size * right], size);
	    memcpy(&base[size * right], &base[size * middle], size);
	    memcpy(&base[size * middle], &tmp[0], size);
	    right--;
	} else
	    middle++;
    }

    // afterwards, middle == right + 1
    // so base[i] == pivot for all left <= i <= right
    split_left = left - 1;
    split_right = right + 1;
}

static void
click_qsort_subroutine(void *base, size_t size, int left, int right, int (*compar)(const void *, const void *))
{
    // XXX recursion
    if (left < right) {
	int split_left, split_right;
	click_qsort_partition(base, size, left, right, compar, split_left, split_right);
	click_qsort_subroutine(base, size, left, split_left, compar);
	click_qsort_subroutine(base, size, split_right, right, compar);
    }
}

void
click_qsort(void *base, size_t n, size_t size, int (*compar)(const void *, const void *))
{
    click_qsort_subroutine(base, size, 0, n - 1, compar);
}

}
#endif


// TIMEVALS AND JIFFIES

#if CLICK_USERLEVEL

# if CLICK_HZ != 100
#  error "CLICK_HZ must be 100"
# endif

unsigned
click_jiffies()
{
  struct timeval tv;
  click_gettimeofday(&tv);
  return (tv.tv_sec * 100) + (tv.tv_usec / 10000);
}

#endif


// OTHER

#if defined(CLICK_LINUXMODULE) || defined(CLICK_BSDMODULE)

#ifdef CLICK_BSDMODULE

/*
 * Character types glue for isalnum() et al, from Linux.
 */

/*
 *  From linux/lib/ctype.c
 *
 *  Copyright (C) 1991, 1992  Linus Torvalds
 */

unsigned char _ctype[] = {
_C,_C,_C,_C,_C,_C,_C,_C,			/* 0-7 */
_C,_C|_S,_C|_S,_C|_S,_C|_S,_C|_S,_C,_C,		/* 8-15 */
_C,_C,_C,_C,_C,_C,_C,_C,			/* 16-23 */
_C,_C,_C,_C,_C,_C,_C,_C,			/* 24-31 */
_S|_SP,_P,_P,_P,_P,_P,_P,_P,			/* 32-39 */
_P,_P,_P,_P,_P,_P,_P,_P,			/* 40-47 */
_D,_D,_D,_D,_D,_D,_D,_D,			/* 48-55 */
_D,_D,_P,_P,_P,_P,_P,_P,			/* 56-63 */
_P,_U|_X,_U|_X,_U|_X,_U|_X,_U|_X,_U|_X,_U,	/* 64-71 */
_U,_U,_U,_U,_U,_U,_U,_U,			/* 72-79 */
_U,_U,_U,_U,_U,_U,_U,_U,			/* 80-87 */
_U,_U,_U,_P,_P,_P,_P,_P,			/* 88-95 */
_P,_L|_X,_L|_X,_L|_X,_L|_X,_L|_X,_L|_X,_L,	/* 96-103 */
_L,_L,_L,_L,_L,_L,_L,_L,			/* 104-111 */
_L,_L,_L,_L,_L,_L,_L,_L,			/* 112-119 */
_L,_L,_L,_P,_P,_P,_P,_C,			/* 120-127 */
0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,		/* 128-143 */
0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,		/* 144-159 */
_S|_SP,_P,_P,_P,_P,_P,_P,_P,_P,_P,_P,_P,_P,_P,_P,_P,   /* 160-175 */
_P,_P,_P,_P,_P,_P,_P,_P,_P,_P,_P,_P,_P,_P,_P,_P,       /* 176-191 */
_U,_U,_U,_U,_U,_U,_U,_U,_U,_U,_U,_U,_U,_U,_U,_U,       /* 192-207 */
_U,_U,_U,_U,_U,_U,_U,_P,_U,_U,_U,_U,_U,_U,_U,_L,       /* 208-223 */
_L,_L,_L,_L,_L,_L,_L,_L,_L,_L,_L,_L,_L,_L,_L,_L,       /* 224-239 */
_L,_L,_L,_L,_L,_L,_L,_P,_L,_L,_L,_L,_L,_L,_L,_L};      /* 240-255 */

extern "C" void __assert(const char *file, int line, const char *cond) {
    printf("Failed assertion at %s:%d: %s\n", file, line, cond);
}

#endif

extern "C" {

void
__assert_fail(const char *__assertion,
	      const char *__file,
	      unsigned int __line,
	      const char *__function)
{
  click_chatter("assertion failed %s %s %d %s\n",
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
  click_chatter("pure virtual method called\n");
}

void *
__rtti_si()
{
  click_chatter("rtti_si\n");
  return(0);
}

void *
__rtti_user()
{
  click_chatter("rtti_user\n");
  return(0);
}

#ifdef CLICK_LINUXMODULE

/*
 * Convert a string to a long integer. use simple_strtoul, which is in the
 * kernel
 */

long
strtol(const char *nptr, char **endptr, int base)
{
  // XXX should check for overflow and underflow, but strtoul doesn't, so why
  // bother?
  if (*nptr == '-')
    return -simple_strtoul(nptr + 1, endptr, base);
  else if (*nptr == '+')
    return simple_strtoul(nptr + 1, endptr, base);
  else
    return simple_strtoul(nptr, endptr, base);
}

#if LINUX_VERSION_CODE >= KERNEL_VERSION(2, 4, 0) && __GNUC__ == 2 && __GNUC_MINOR__ == 96
int
click_strcmp(const char *a, const char *b)
{
int d0, d1;
register int __res;
__asm__ __volatile__(
	"1:\tlodsb\n\t"
	"scasb\n\t"
	"jne 2f\n\t"
	"testb %%al,%%al\n\t"
	"jne 1b\n\t"
	"xorl %%eax,%%eax\n\t"
	"jmp 3f\n"
	"2:\tsbbl %%eax,%%eax\n\t"
	"orb $1,%%al\n"
	"3:"
	:"=a" (__res), "=&S" (d0), "=&D" (d1)
		     :"1" (a),"2" (b));
return __res;
}
#endif
#endif

}

#endif /* !__KERNEL__ */

#if CLICK_LINUXMODULE && !defined(__HAVE_ARCH_STRLEN) && !defined(HAVE_LINUX_STRLEN_EXPOSED)
// Need to provide a definition of 'strlen'. This one is taken from Linux.
extern "C" {
size_t strlen(const char * s)
{
    const char *sc;
    for (sc = s; *sc != '\0'; ++sc)
	/* nothing */;
    return sc - s;
}
}
#endif
