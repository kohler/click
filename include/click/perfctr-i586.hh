
#ifndef PERFCTR_HH
#define PERFCTR_HH

#ifdef __KERNEL__
#include <click/glue.hh>
#include <asm/msr.h>
#endif

// do not use with PerfInfo unless PerfInfo also registers the same metrics

#define DCU_MISS_OUTSTANDING 	0x48
#define INST_RETIRED 		0xC0
#define IFU_FETCH		0x80
#define IFU_FETCH_MISS		0x81
#define IFU_MEM_STALL		0x86
#define L2_LINES_IN		0x24
#define L2_LINES_OUT		0x26
#define L2_IFETCH		0x28 | (0xF<<8)
#define L2_LD			0x29 | (0xF<<8)
#define L2_LINES_OUTM		0x27
#define L2_RQSTS		0x2E | (0xF<<8)
#define BUS_TRAN_MEM		0x6F
#define BUS_TRAN_INVAL		0x69

#define MSR_OS (1<<17)
#define MSR_OCCURRENCE (1<<18)
#define MSR_ENABLE (1<<22)
#define MSR_FLAGS0 (MSR_OS|MSR_OCCURRENCE|MSR_ENABLE)
#define MSR_FLAGS1 (MSR_OS|MSR_OCCURRENCE)

#define MSR_EVNTSEL0 0x186
#define MSR_EVNTSEL1 0x187

class PerfCtr {
  unsigned _low00, _low10, _t;
 public:
  PerfCtr() : _low00(0), _low10(0), _t(0) {}
#ifdef __KERNEL__
  void set_metrics(unsigned m0, unsigned m1) {
    wrmsr(MSR_EVNTSEL0, m0|MSR_FLAGS0, 0);
    wrmsr(MSR_EVNTSEL1, m1|MSR_FLAGS1, 0);
  }
  void reset() { 
    unsigned high; 
    rdpmc(0, _low00, high);
    rdpmc(1, _low10, high);
    _t = get_cycles();
  }
  void get_stats(unsigned *pctr0, unsigned *pctr1, unsigned *t) {
    unsigned high;
    unsigned low01, low11;
    *t += get_cycles() - _t;
    rdpmc(0, low01, high);
    rdpmc(1, low11, high);
    *pctr0 += (low01 >= _low00) ? low01-_low00 : (UINT_MAX-_low00+low01);
    *pctr1 += (low11 >= _low10) ? low11-_low10 : (UINT_MAX-_low10+low11);
    reset();
  }
#else
  void set_metrics(unsigned, unsigned) {}
  void reset() {} 
  void get_stats(unsigned *, unsigned *, unsigned *) {}
#endif
};


#endif

