
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

