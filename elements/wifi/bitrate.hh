#ifndef _BITRATE_H
#define _BITRATE_H

#include <click/config.h>
extern unsigned calc_usecs_wifi_packet_tries(int length,
					      int rate,
					      int try0, int tryN);

extern unsigned calc_backoff(int rate, int t);

extern unsigned calc_usecs_wifi_packet(int length,
				       int rate, int retries);


extern unsigned calc_transmit_time(int rate, int length);

#endif /* _BITRATE_H_ */
