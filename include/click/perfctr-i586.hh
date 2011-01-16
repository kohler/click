#ifndef CLICK_PERFCTR_HH
#define CLICK_PERFCTR_HH
#ifdef __KERNEL__
# include <click/glue.hh>
# include <click/cxxprotect.h>
CLICK_CXX_PROTECT
# include <asm/msr.h>
CLICK_CXX_UNPROTECT
# include <click/cxxunprotect.h>
#endif

#define DCU_MISS_OUTSTANDING	0x48
#define INST_RETIRED		0xC0
#define IFU_FETCH		0x80
#define IFU_FETCH_MISS		0x81
#define IFU_MEM_STALL		0x86
#define L2_LINES_IN		0x24
#define L2_LINES_OUT		0x26
#define L2_IFETCH		0x28 | (0xF<<8)
#define L2_LD			0x29 | (0xF<<8)
#define L2_LINES_OUTM		0x27
#define L2_RQSTS		0x2E | (0xF<<8)
#define BUS_LOCK_CLOCKS         0x63
#define BUS_TRAN_RFO            0x66
#define BUS_TRAN_INVAL		0x69
#define BUS_TRAN_MEM		0x6F

#define MSR_OS (1<<17)
#define MSR_OCCURRENCE (1<<18)
#define MSR_ENABLE (1<<22)
#define MSR_FLAGS0 (MSR_OS|MSR_OCCURRENCE|MSR_ENABLE)
#define MSR_FLAGS1 (MSR_OS|MSR_OCCURRENCE)

#define MSR_EVNTSEL0 0x186
#define MSR_EVNTSEL1 0x187

#endif
