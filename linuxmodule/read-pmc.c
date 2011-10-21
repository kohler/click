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

#include <click/config.h>
#include <linux/types.h>
#include <linux/kernel.h>
#include <asm/system.h>
#if __i386__ || __x86_64__
# include <asm/msr.h>
#endif


#define P6MSR_CTRSEL0 0x186   /* MSR for programming CTR0 on P6 */
#define P6MSR_CTRSEL1 0x187   /* MSR for programming CTR0 on P6 */
#define P6MSR_CTR0 0xc1       /* Ctr0 on P6 */
#define P6MSR_CTR1 0xc2       /* Ctr1 on P6 */

void
click_cycle_counter(int which, unsigned int *fnp, unsigned long long *valp)
{
#ifdef __i386__
  unsigned low, high;
  rdmsr(P6MSR_CTRSEL0 + which, low, high);
  *fnp = low;
  rdpmcl(which, *valp);
  wrmsrl(P6MSR_CTR0 + which, 0);
#else
  printk("<1>click_cycle_counter: not i386\n");
#endif
}
