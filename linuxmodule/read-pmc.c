/*
 * Read and clear the PPro performance counters.
 * Can't be compiled in C++?
 */

  /*
   * Event number.
   * 0x48 Number of cycles while a DCU mis is outstanding.
   * 0x80 instruction fetches.
   * 0x81 instruction fetch misses.
   * 0x85 ITLB misses.
   * 0x86 cycles of instruction fetch stall.
   * 0xC8 interrupts.
   * 0x13 floating point divides (only counter 1).
   * 0x79 cycles in which the cpu is NOT halted.
   */

#include <linux/types.h>
#include <linux/config.h>
#include <linux/kernel.h>
#include <asm/system.h>


#define P6MSR_CTRSEL0 0x186   /* MSR for programming CTR0 on P6 */
#define P6MSR_CTRSEL1 0x187   /* MSR for programming CTR0 on P6 */
#define P6MSR_CTR0 0xc1       /* Ctr0 on P6 */
#define P6MSR_CTR1 0xc2       /* Ctr1 on P6 */

typedef unsigned long long u_quad_t;
typedef u_quad_t pctrval;

#define rdmsr(msr)						\
({								\
  pctrval v;							\
  __asm __volatile (".byte 0xf, 0x32" : "=A" (v) : "c" (msr));	\
  v;								\
})

#define wrmsr(msr, v) \
     __asm __volatile (".byte 0xf, 0x30" :: "A" ((u_quad_t) (v)), "c" (msr));

/* Read the performance counters (Pentium Pro only) */
#define rdpmc(ctr)				\
({						\
  pctrval v;					\
  __asm __volatile (".byte 0xf, 0x33\n"		\
		    "\tandl $0xff, %%edx"	\
		    : "=A" (v) : "c" (ctr));	\
  v;						\
})

void
click_cycle_counter(int which, unsigned int *fnp, unsigned long long *valp)
{
  *fnp = rdmsr(P6MSR_CTRSEL0 + which);
  *valp = rdpmc(which);
  wrmsr(P6MSR_CTR0+which, 0);
}
