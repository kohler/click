
/* we found that a 2 to 1 ratio prevents polldev from pulling too much stuff
 * for the transmit device. these two numbers must also be less than the rx
 * and tx dma ring size (64 and 16, respectively), so that cleaning the device
 * dma queues only need to happen once per run */

#define POLLDEV_MAX_PKTS_PER_RUN 8
#define TODEV_MAX_PKTS_PER_RUN   16

// #define DEV_RXFIFO_STATS 1
// #define DEV_KEEP_STATS 1
#define ADJ_TICKETS 1
#define BATCH_PKT_PROC 1

#if DEV_KEEP_STATS

#define SET_STATS(p0mark, p1mark, time_mark) \
  { \
    unsigned high; \
    rdpmc(0, p0mark, high); \
    rdpmc(1, p1mark, high); \
    time_mark = get_cycles(); \
  }

#define GET_STATS_RESET(p0mark, p1mark, time_mark, pctr0, pctr1, tctr) \
  { \
    unsigned high; \
    unsigned low01, low11; \
    tctr += get_cycles() - time_mark; \
    rdpmc(0, low01, high); \
    rdpmc(1, low11, high); \
    pctr0 += (low01 >= p0mark) ? low01-p0mark : (UINT_MAX-p0mark+low01); \
    pctr1 += (low11 >= p1mark) ? low11-p1mark : (UINT_MAX-p1mark+low11); \
    rdpmc(0, p0mark, high); \
    rdpmc(1, p1mark, high); \
    time_mark = get_cycles(); \
  }

#else

#define GET_STATS_RESET(a,b,c,d,e,f) ;
#define SET_STATS(a,b,c) ;

#endif

