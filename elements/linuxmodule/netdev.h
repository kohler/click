
/* we found that a 2 to 1 ratio prevents polldev from pulling too much stuff
 * for the transmit device. these two numbers must also be less than the rx
 * and tx dma ring size (32 and 16, respectively) */

#define POLLDEV_MAX_PKTS_PER_RUN 8
#define TODEV_MAX_PKTS_PER_RUN   16

