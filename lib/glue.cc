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

#include <limits.h>
#ifndef CLICK_LINUXMODULE
# include <stdarg.h>
#endif
#ifdef CLICK_USERLEVEL
# include <unistd.h>
#endif

void
click_chatter(const char *fmt, ...)
{
  va_list val;
  va_start(val, fmt);

  if (ErrorHandler::has_default_handler()) {
    ErrorHandler *errh = ErrorHandler::default_handler();
    errh->verror(ErrorHandler::ERR_MESSAGE, "", fmt, val);
  } else {
#ifdef CLICK_LINUXMODULE
#if __MTCLICK__
    static char buf[NR_CPUS][512];	// XXX
    int i = vsprintf(buf[current->processor], fmt, val);
    printk("<1>%s\n", buf[current->processor]);
#else
    static char buf[512];		// XXX
    int i = vsprintf(buf, fmt, val);
    printk("<1>%s\n", buf);
#endif
#elif defined(CLICK_BSDMODULE)
    vprintf(fmt, val);
#else /* User-space */
    vfprintf(stderr, fmt, val);
    fprintf(stderr, "\n");
#endif
  }
  
  va_end(val);
}

// Just for statistics.
unsigned int click_new_count = 0;
unsigned int click_outstanding_news = 0;

#if defined(_KERNEL) || defined(__KERNEL__)

/*
 * Kernel module glue.
 */

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

#ifdef CLICK_LINUXMODULE
#define	CLICK_ALLOC(size)	kmalloc((size), GFP_ATOMIC)
#define	CLICK_FREE(ptr)		kfree((ptr))
#endif

#ifdef CLICK_BSDMODULE
#define CLICK_ALLOC(size)	malloc((size), M_TEMP, M_WAITOK)
#define	CLICK_FREE(ptr)		free(ptr, M_TEMP)
#endif

#define CHUNK_MAGIC		0xffff3f7f	/* -49281 */
#define CHUNK_MAGIC_FREED	0xc66b04f5
struct Chunk {
  int magic;
  int size;
  int where;
  Chunk *prev;
  Chunk *next;
};

static Chunk *chunks = 0;
int click_dmalloc_where = 0x3F3F3F3F;

void *
operator new(unsigned int sz)
{
  click_new_count++;
  click_outstanding_news++;
#if CLICK_DMALLOC
  void *v = CLICK_ALLOC(sz + sizeof(Chunk));
  Chunk *c = (Chunk *)v;
  c->magic = CHUNK_MAGIC;
  c->size = sz;
  c->next = chunks;
  c->prev = 0;
  c->where = click_dmalloc_where;
  if (chunks) chunks->prev = c;
  chunks = c;
  return (void *)((unsigned char *)v + sizeof(Chunk));
#else
  return CLICK_ALLOC(sz);
#endif
}

void *
operator new [] (unsigned int sz)
{
  click_new_count++;
  click_outstanding_news++;
#if CLICK_DMALLOC
  void *v = CLICK_ALLOC(sz + sizeof(Chunk));
  Chunk *c = (Chunk *)v;
  c->magic = CHUNK_MAGIC;
  c->size = sz;
  c->next = chunks;
  c->prev = 0;
  c->where = click_dmalloc_where;
  if (chunks) chunks->prev = c;
  chunks = c;
  return (void *)((unsigned char *)v + sizeof(Chunk));
#else
  return CLICK_ALLOC(sz);
#endif
}

void
operator delete(void *addr)
{
  if (addr) {
    click_outstanding_news--;
#if CLICK_DMALLOC
    Chunk *c = (Chunk *)((unsigned char *)addr - sizeof(Chunk));
    if (c->magic == CHUNK_MAGIC_FREED) {
      click_chatter("click error: double-free of memory at %p (%d @ %p)\n",
		    addr, c->size, c->where);
      return;
    }
    if (c->magic != CHUNK_MAGIC) {
      click_chatter("click error: memory corruption on delete %p\n", addr);
      return;
    }
    c->magic = CHUNK_MAGIC_FREED;
    if (c->prev) c->prev->next = c->next;
    else chunks = c->next;
    if (c->next) c->next->prev = c->prev;
    CLICK_FREE((void *)c);
#else
    CLICK_FREE(addr);
#endif
  }
}

void
operator delete [] (void *addr)
{
  if (addr) {
    click_outstanding_news--;
#if CLICK_DMALLOC
    Chunk *c = (Chunk *)((unsigned char *)addr - sizeof(Chunk));
    if (c->magic == CHUNK_MAGIC_FREED) {
      click_chatter("click error: double-free of memory at %p (%d @ %p)\n",
		    addr, c->size, c->where);
      return;
    }
    if (c->magic != CHUNK_MAGIC) {
      click_chatter("click error: memory corruption on delete[] %p\n", addr);
      return;
    }
    c->magic = CHUNK_MAGIC_FREED;
    if (c->prev) c->prev->next = c->next;
    else chunks = c->next;
    if (c->next) c->next->prev = c->prev;
    CLICK_FREE((void *)c);
#else
    CLICK_FREE(addr);
#endif
  }
}

void
print_and_free_chunks()
{
#if CLICK_DMALLOC
  const char *hexstr = "0123456789ABCDEF";
  for (Chunk *c = chunks; c; ) {
    Chunk *n = c->next;

    // set where buffer
    char buf[13], *s = buf;
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
    
    click_chatter("  chunk at %p size %d alloc[%s] data ",
		  (void *)(c + 1), c->size, buf);
    unsigned char *d = (unsigned char *)(c + 1);
    for (int i = 0; i < 20 && i < c->size; i++)
      click_chatter("%02x", d[i]);
    click_chatter("\n");
    CLICK_FREE((void *)c);
    
    c = n;
  }
#endif
}

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

// Random number routines.

uint32_t click_random_seed = 152;

void
srandom(uint32_t seed)
{
  click_random_seed = seed;
}


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

};

#endif /* !__KERNEL__ */

#if defined(CLICK_USERLEVEL)

unsigned
click_jiffies()
{
  struct timeval tv;
  click_gettimeofday(&tv);
  return (tv.tv_sec * 100) + (tv.tv_usec / 10000);
}

#endif /* CLICK_USERLEVEL */

void
click_random_srandom()
{
    static const int bufsiz = 16;
    uint32_t buf[bufsiz];
    int pos = 0;
    click_gettimeofday((struct timeval *)(buf + pos));
    pos += sizeof(struct timeval) / sizeof(uint32_t);
#ifdef CLICK_USERLEVEL
    FILE *f = fopen("/dev/random", "rb");
    if (f) {
	fread(buf + pos, sizeof(uint32_t), bufsiz - pos, f);
	fclose(f);
	pos = bufsiz;
    } else {
	buf[pos++] = getpid();
	buf[pos++] = getuid();
    }
#endif

    uint32_t result = 0;
    for (int i = 0; i < pos; i++) {
	result ^= buf[i];
	result = (result << 1) | (result >> 31);
    }
    srandom(result);
}
