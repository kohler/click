// -*- c-basic-offset: 4; related-file-name: "../include/click/glue.hh" -*-
/*
 * glue.{cc,hh} -- minimize portability headaches, and miscellany
 * Robert Morris, Eddie Kohler
 *
 * Copyright (c) 1999-2000 Massachusetts Institute of Technology
 * Copyright (c) 2007 Regents of the University of California
 * Copyright (c) 2008 Mazu Networks, Inc.
 * Copyright (c) 2008-2011 Meraki, Inc.
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
#include <click/timestamp.hh>
#include <click/error.hh>

#ifdef CLICK_USERLEVEL
# include <stdarg.h>
# include <unistd.h>
# include <sys/types.h>
# include <sys/stat.h>
# include <fcntl.h>
#elif CLICK_LINUXMODULE
# if LINUX_VERSION_CODE >= KERNEL_VERSION(2, 6, 0)
#  include <click/cxxprotect.h>
CLICK_CXX_PROTECT
#  include <linux/vmalloc.h>
CLICK_CXX_UNPROTECT
#  include <click/cxxunprotect.h>
# endif
#elif CLICK_BSDMODULE
# include <click/cxxprotect.h>
CLICK_CXX_PROTECT
# include <sys/malloc.h>
CLICK_CXX_UNPROTECT
# include <click/cxxunprotect.h>
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

// Allocate space for static constants from integer_traits.
CLICK_DECLS
#define DO(t) \
    constexpr bool integer_traits<t>::is_numeric; \
    constexpr bool integer_traits<t>::is_integral; \
    constexpr t integer_traits<t>::const_min; \
    constexpr t integer_traits<t>::const_max; \
    constexpr bool integer_traits<t>::is_signed;
DO(unsigned char)
DO(signed char)
constexpr char integer_traits<char>::const_min;
constexpr char integer_traits<char>::const_max;
DO(unsigned short)
DO(short)
DO(unsigned int)
DO(int)
DO(unsigned long)
DO(long)
#if HAVE_LONG_LONG
DO(unsigned long long)
DO(long long)
#endif
#if HAVE_INT64_TYPES && !HAVE_INT64_IS_LONG && !HAVE_INT64_IS_LONG_LONG
DO(uint64_t)
DO(int64_t)
#endif
#undef DO
CLICK_ENDDECLS

void
click_check_header_sizes()
{
    // <clicknet/ether.h>
    static_assert(sizeof(click_ether) == 14, "click_ether has the wrong size.");
    static_assert(sizeof(click_arp) == 8, "click_arp has the wrong size.");
    static_assert(sizeof(click_ether_arp) == 28, "click_ether_arp has the wrong size.");
    static_assert(sizeof(click_nd_sol) == 32, "click_nd_sol has the wrong size.");
    static_assert(sizeof(click_nd_adv) == 32, "click_nd_adv has the wrong size.");
    static_assert(sizeof(click_nd_adv2) == 24, "click_nd_adv2 has the wrong size.");

    // <clicknet/ip.h>
    static_assert(sizeof(click_ip) == 20, "click_ip has the wrong size.");

    // <clicknet/icmp.h>
    static_assert(sizeof(click_icmp) == 8, "click_icmp has the wrong size.");
    static_assert(sizeof(click_icmp_paramprob) == 8, "click_icmp_paramprob has the wrong size.");
    static_assert(sizeof(click_icmp_redirect) == 8, "click_icmp_redirect has the wrong size.");
    static_assert(sizeof(click_icmp_sequenced) == 8, "click_icmp_sequenced has the wrong size.");
    static_assert(sizeof(click_icmp_tstamp) == 20, "click_icmp_tstamp has the wrong size.");

    // <clicknet/tcp.h>
    static_assert(sizeof(click_tcp) == 20, "click_tcp has the wrong size.");

    // <clicknet/udp.h>
    static_assert(sizeof(click_udp) == 8, "click_udp has the wrong size.");

    // <clicknet/ip6.h>
    static_assert(sizeof(click_ip6) == 40, "click_ip6 has the wrong size.");

    // <clicknet/fddi.h>
    static_assert(sizeof(click_fddi) == 13, "click_fddi has the wrong size.");
    static_assert(sizeof(click_fddi_8022_1) == 16, "click_fddi_8022_1 has the wrong size.");
    static_assert(sizeof(click_fddi_8022_2) == 17, "click_fddi_8022_2 has the wrong size.");
    static_assert(sizeof(click_fddi_snap) == 21, "click_fddi_snap has the wrong size.");

    // <clicknet/rfc1483.h>
    static_assert(sizeof(click_rfc1483) == 8, "click_rfc1483 has the wrong size.");
}


// DEBUGGING OUTPUT

CLICK_USING_DECLS

extern "C" {
void
click_chatter(const char *fmt, ...)
{
  va_list val;
  va_start(val, fmt);

  if (ErrorHandler *errh = ErrorHandler::default_handler()) {
    errh->xmessage(ErrorHandler::e_info, fmt, val);
  } else {
#if CLICK_LINUXMODULE
# if __MTCLICK__
    static char buf[NR_CPUS][512];	// XXX
    click_processor_t cpu = click_get_processor();
    int i = vsnprintf(buf[cpu], 512, fmt, val);
    printk(KERN_ALERT "%.*s\n", i, buf[cpu]);
    click_put_processor();
# else
    static char buf[512];		// XXX
    int i = vsnprintf(buf, 512, fmt, val);
    printk(KERN_ALERT "%.*s\n", i, buf);
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
}


// DEBUG MALLOC

uint32_t click_dmalloc_where = 0x3F3F3F3F;
size_t click_dmalloc_curnew = 0;
size_t click_dmalloc_totalnew = 0;
size_t click_dmalloc_failnew = 0;
#if CLICK_DMALLOC
size_t click_dmalloc_curmem = 0;
size_t click_dmalloc_maxmem = 0;
#endif

#if CLICK_LINUXMODULE || CLICK_BSDMODULE

# if CLICK_LINUXMODULE
struct task_struct *clickfs_task;
#  define CLICK_ALLOC(size)	kmalloc((size), (current == clickfs_task ? GFP_KERNEL : GFP_ATOMIC))
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
    uint32_t where;
    size_t size;
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
operator new(size_t sz) throw ()
{
  click_dmalloc_totalnew++;
# if CLICK_DMALLOC
  if (void *v = CLICK_ALLOC(sz + sizeof(Chunk))) {
    click_dmalloc_curnew++;
    Chunk *c = (Chunk *)v;
    c->magic = CHUNK_MAGIC;
    c->size = sz;
    c->where = click_dmalloc_where;
    c->prev = &chunks;
    c->next = chunks.next;
    c->next->prev = chunks.next = c;
    click_dmalloc_curmem += sz;
    if (click_dmalloc_curmem > click_dmalloc_maxmem)
      click_dmalloc_maxmem = click_dmalloc_curmem;
    return (void *)((unsigned char *)v + sizeof(Chunk));
  } else {
    click_dmalloc_failnew++;
    return 0;
  }
# else
  if (void *v = CLICK_ALLOC(sz)) {
    click_dmalloc_curnew++;
    return v;
  } else {
    click_dmalloc_failnew++;
    return 0;
  }
# endif
}

void *
operator new[](size_t sz) throw ()
{
  click_dmalloc_totalnew++;
# if CLICK_DMALLOC
  if (void *v = CLICK_ALLOC(sz + sizeof(Chunk))) {
    click_dmalloc_curnew++;
    Chunk *c = (Chunk *)v;
    c->magic = CHUNK_MAGIC;
    c->size = sz;
    c->where = click_dmalloc_where;
    c->prev = &chunks;
    c->next = chunks.next;
    c->next->prev = chunks.next = c;
    click_dmalloc_curmem += sz;
    if (click_dmalloc_curmem > click_dmalloc_maxmem)
      click_dmalloc_maxmem = click_dmalloc_curmem;
    return (void *)((unsigned char *)v + sizeof(Chunk));
  } else {
    click_dmalloc_failnew++;
    return 0;
  }
# else
  if (void *v = CLICK_ALLOC(sz)) {
    click_dmalloc_curnew++;
    return v;
  } else {
    click_dmalloc_failnew++;
    return 0;
  }
# endif
}

void
operator delete(void *addr)
{
  if (addr) {
    click_dmalloc_curnew--;
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
    click_dmalloc_curmem -= c->size;
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
    click_dmalloc_curnew--;
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
    click_dmalloc_curmem -= c->size;
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


// LALLOC

#if CLICK_LINUXMODULE
extern "C" {

# define CLICK_LALLOC_MAX_SMALL	131072

void *
click_lalloc(size_t size)
{
    void *v;
    click_dmalloc_totalnew++;
    if (size > CLICK_LALLOC_MAX_SMALL)
	v = vmalloc(size);
    else
	v = CLICK_ALLOC(size);
    if (v) {
	click_dmalloc_curnew++;
# if CLICK_DMALLOC
	click_dmalloc_curmem += size;
# endif
    } else
	click_dmalloc_failnew++;
    return v;
}

void
click_lfree(volatile void *p, size_t size)
{
    if (p) {
	if (size > CLICK_LALLOC_MAX_SMALL)
	    vfree((void *) p);
	else
	    kfree((void *) p);
	click_dmalloc_curnew--;
# if CLICK_DMALLOC
	click_dmalloc_curmem -= size;
# endif
    }
}

}
#endif


// RANDOMNESS

CLICK_DECLS

#if CLICK_LINUXMODULE || (!CLICK_BSDMODULE && CLICK_RAND_MAX != RAND_MAX)
uint32_t click_random_seed = 152;
#endif

void
click_random_srandom()
{
    static const int bufsiz = 64;
    union {
	char c[bufsiz];
	uint32_t u32[bufsiz / 4];
    } buf;
    int pos = 0;

    Timestamp ts = Timestamp::now();
    memcpy(buf.c + pos, &ts, sizeof(Timestamp));
    pos += sizeof(Timestamp);

#if CLICK_USERLEVEL
# ifdef O_NONBLOCK
    int fd = open("/dev/random", O_RDONLY | O_NONBLOCK);
# elif defined(O_NDELAY)
    int fd = open("/dev/random", O_RDONLY | O_NDELAY);
# else
    int fd = open("/dev/random", O_RDONLY);
# endif
    if (fd >= 0) {
	ssize_t amt = read(fd, buf.c + pos, bufsiz - pos);
	close(fd);
	if (amt > 0)
	    pos += amt;
    }

    struct {
	pid_t p;
	uid_t u;
    } pu;
    pu.p = getpid();
    pu.u = getuid();
    int pu_amt = (bufsiz - pos < (int) sizeof(pu) ? bufsiz - pos : (int) sizeof(pu));
    memcpy(buf.c + pos, &pu, pu_amt);
    pos += pu_amt;
#endif

    uint32_t result = 0;
    for (int i = 0; i < pos / 4; i++) {
	result ^= buf.u32[i];
	result = (result << 1) | (result >> 31);
    }

#if CLICK_LINUXMODULE
    uint32_t kernrand;
    get_random_bytes(&kernrand, sizeof(kernrand));
    result ^= kernrand;
#endif

    click_srandom(result);
}

uint32_t
click_random(uint32_t low, uint32_t high)
{
    if (unlikely(low > high))
	return low;
    uint32_t r;
    if (unlikely(high - low > CLICK_RAND_MAX)) {
	while ((r = (click_random() << 17) ^ (click_random() >> 14)) > high - low)
	    /* try again */;
	return r + low;
    } else if (high == low + 1) { // common case
	return low + ((click_random() >> 14) & 1);
    } else {
	uint32_t count = ((uint32_t) CLICK_RAND_MAX + 1) / (high - low + 1);
	uint32_t max = count * (high - low + 1);
	while ((r = click_random()) >= max)
	    /* try again */;
	return (r / count) + low;
    }
}

CLICK_ENDDECLS


// SORTING

// This reimplementation of click_qsort is a version of
// Engineering a Sort Function, Jon L. Bentley and M. Douglas McIlroy,
// Software---Practice & Experience, 23(11), 1249-1265, Nov. 1993

namespace {

typedef long cq_word_t;
#define CQ_WORD_SIZE		sizeof(cq_word_t)
#define cq_exch(a, b, t)	(t = a, a = b, b = t)
#define cq_swap(a, b)						\
    (swaptype ? cq_swapfunc(a, b, size, swaptype)		\
     : (void) cq_exch(*(cq_word_t *) (a), *(cq_word_t *) (b), swaptmp))

static char *
cq_med3(char *a, char *b, char *c,
	int (*compar)(const void *, const void *, void *), void *thunk)
{
    int ab = compar(a, b, thunk);
    int bc = compar(b, c, thunk);
    if (ab < 0)
	return (bc < 0 ? b : (compar(a, c, thunk) < 0 ? c : a));
    else
	return (bc > 0 ? b : (compar(a, c, thunk) > 0 ? c : a));
}

static void
cq_swapfunc(char *a, char *b, size_t n, int swaptype)
{
    if (swaptype <= 1) {
	cq_word_t t;
	for (; n; a += CQ_WORD_SIZE, b += CQ_WORD_SIZE, n -= CQ_WORD_SIZE)
	    cq_exch(*(cq_word_t *) a, *(cq_word_t *) b, t);
    } else {
	char t;
	for (; n; ++a, ++b, --n)
	    cq_exch(*a, *b, t);
    }
}

}


#define CQ_STACKSIZ	((8 * SIZEOF_SIZE_T + 1) * 2)

int
click_qsort(void *base, size_t n, size_t size,
	    int (*compar)(const void *, const void *, void *), void *thunk)
{
    int swaptype;
    if ((((char *) base - (char *) 0) | size) % CQ_WORD_SIZE)
	swaptype = 2;
    else
	swaptype = (size > CQ_WORD_SIZE ? 1 : 0);
    cq_word_t swaptmp;

    size_t stackbuf[CQ_STACKSIZ];
    size_t *stack = stackbuf;
    *stack++ = 0;
    *stack++ = n;

    while (stack != stackbuf) {
	stack -= 2;
	char *a = (char *) base + stack[0] * size;
	n = stack[1] - stack[0];

	// insertion sort for tiny arrays
	if (n < 7) {
	    for (char *pi = a + size; pi < a + n * size; pi += size)
		for (char *pj = pi;
		     pj > a && compar(pj - size, pj, thunk) > 0;
		     pj -= size)
		    cq_swap(pj, pj - size);
	    continue;
	}

	// find a pivot
	char *pm = a + (n / 2) * size;
	if (n > 7) {
	    char *p1 = a;
	    char *p2 = a + (n - 1) * size;
	    if (n > 40) {     /* Big arrays, pseudomedian of 9 */
		size_t offset = (n / 8) * size;
		p1 = cq_med3(p1, p1 + offset, p1 + 2*offset, compar, thunk);
		pm = cq_med3(pm - offset, pm, pm + offset, compar, thunk);
		p2 = cq_med3(p2 - 2*offset, p2 - offset, p2, compar, thunk);
	    }
	    pm = cq_med3(p1, pm, p2, compar, thunk);
	}

	// 2009.Jan.21: A tiny change makes the sort complete even with a
	// bogus comparator, such as "return 1;".  Guarantee that "a" holds
	// the pivot.  This means we don't need to compare "a" against the
	// pivot explicitly.  (See initialization of "pa = pb = a + size".)
	// Subdivisions will thus never include the pivot, even if "cmp(pivot,
	// pivot)" returns nonzero.  We will thus never run indefinitely.
	cq_word_t pivottmp;
	char *pivot;
	if (swaptype)
	    pivot = a;
	else
	    pivot = (char *) &pivottmp, pivottmp = *(cq_word_t *) pm;
	cq_swap(a, pm);

	// partition
	char *pa, *pb, *pc, *pd;
	pa = pb = a + size;
	pc = pd = a + (n - 1) * size;
	int r;
	while (1) {
	    while (pb <= pc && (r = compar(pb, pivot, thunk)) <= 0) {
		if (r == 0) {
		    cq_swap(pa, pb);
		    pa += size;
		}
		pb += size;
	    }
	    while (pc >= pb && (r = compar(pc, pivot, thunk)) >= 0) {
		if (r == 0) {
		    cq_swap(pc, pd);
		    pd -= size;
		}
		pc -= size;
	    }
	    if (pb > pc)
		break;
	    cq_swap(pb, pc);
	    pb += size;
	    pc -= size;
	}

	// swap the extreme ranges, which are equal to the pivot, into the
	// middle
	char *pn = a + n * size;
	size_t s = (pa - a < pb - pa ? pa - a : pb - pa);
	if (s)
	    cq_swapfunc(a, pb - s, s, swaptype);
	size_t pd_minus_pc = pd - pc;
	s = (pd_minus_pc < pn - pd - size ? pd_minus_pc : pn - pd - size);
	if (s)
	    cq_swapfunc(pb, pn - s, s, swaptype);

	// mark subranges to sort
	if (pb == pa && pd == pc)
	    continue;
	assert(stack + 4 < stackbuf + CQ_STACKSIZ);
	stack[2] = stack[1] - (pd - pc) / size;
	stack[3] = stack[1];
	stack[1] = stack[0] + (pb - pa) / size;
	// Push stack items biggest first.  This limits the stack size to
	// log2 MAX_SIZE_T!  Optimization in Hoare's original paper, suggested
	// by Sedgewick in his own qsort implementation paper.
	if (stack[3] - stack[2] > stack[1] - stack[0]) {
	    size_t tx;
	    tx = stack[0], stack[0] = stack[2], stack[2] = tx;
	    tx = stack[1], stack[1] = stack[3], stack[3] = tx;
	}
	stack += (stack[2] != stack[3] ? 4 : 2);
    }

    return 0;
}

int
click_qsort(void *base, size_t n, size_t size, int (*compar)(const void *, const void *))
{
    int (*compar2)(const void *, const void *, void *);
    compar2 = reinterpret_cast<int (*)(const void *, const void *, void *)>(compar);
    return click_qsort(base, n, size, compar2, 0);
}


// THREADS

#if CLICK_USERLEVEL && HAVE_MULTITHREAD && HAVE___THREAD_STORAGE_CLASS
__thread int click_current_thread_id;
#endif


// TIMEVALS AND JIFFIES

#if CLICK_USERLEVEL

# if CLICK_HZ != 1000
#  error "CLICK_HZ must be 1000"
# endif
CLICK_DECLS

void
click_gettimeofday(timeval *tvp)
{
    *tvp = Timestamp::now().timeval();
}

click_jiffies_t
click_jiffies()
{
    return Timestamp::now().msecval();
}

CLICK_ENDDECLS
#endif


// OTHER

#if CLICK_LINUXMODULE || CLICK_BSDMODULE

#if CLICK_BSDMODULE

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

# if __FreeBSD_version >= 700000 && __FreeBSD_version < 703000
/* memmove() appeared in the FreeBSD 7.3 kernel */
extern "C" void *
memmove(void *dest, const void *src, size_t n)
{
    bcopy(src, dest, n);
    return (dest);
}
# endif

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

#if __GNUC__ >= 3
void
__cxa_pure_virtual()
{
  click_chatter("pure virtual method called\n");
}
#else
void
__pure_virtual()
{
  click_chatter("pure virtual method called\n");
}
#endif

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

#if __GNUC__ == 2 && __GNUC_MINOR__ == 96
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
