#ifndef NET_DEV_HH
#define NET_DEV_HH

#include "error.hh"

#define MAX_DEVICES	1024
#define DEV_ELEM_TYPES	3

#define TODEV_OBJ 	0
#define FROMDEV_OBJ 	1
#define POLLDEV_OBJ	2

/* we found that a 2 to 1 ratio prevents polldev from pulling too much stuff
 * for the transmit device. these two numbers must also be less than the rx
 * and tx dma ring size (64 and 16, respectively), so that cleaning the device
 * dma queues only need to happen once per run */

#define INPUT_MAX_PKTS_PER_RUN 		8
#define OUTPUT_MAX_PKTS_PER_RUN   	16

#define _CLICK_STATS_ 1
#define ADJ_TICKETS 1

#if _CLICK_STATS_

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

extern unsigned first_time;
extern void* ifindex_map[MAX_DEVICES][DEV_ELEM_TYPES];

/* Updates the interface map, which contains, for each device number, a list
 * of three possible objects: a FromDevice object, a PollDevice object, or a
 * ToDevice object. Returns 0 if the device number (ifindex argument) or type
 * of object (which argument) is bad. If the map already has an object of the
 * given type for the given device, returns that object ptr. Otherwise, stores
 * the given pointer (value argument) in the map.
 */
inline void *
update_ifindex_map(unsigned ifindex, ErrorHandler *errh, 
    		   unsigned which, void *value)
{
  if (ifindex >= MAX_DEVICES) {
    errh->error("Too many devices");
    return 0L;
  }
  if (which > DEV_ELEM_TYPES) {
    errh->error("Bad device element type");
    return 0L;
  }

  if (first_time) {
    for(int i=0; i<MAX_DEVICES; i++) 
      for(int j=0; j<DEV_ELEM_TYPES; j++)
	ifindex_map[i][j] = 0L;
    first_time = 0;
  }

  if (ifindex_map[ifindex][which] == 0L)
    ifindex_map[ifindex][which] = value;
  return ifindex_map[ifindex][which];
}

/* returns the entry for the given device (ifindex argument) and object type
 * (which argument). */
inline void *
lookup_ifindex_map(unsigned ifindex, unsigned which)
{
  if (ifindex >= MAX_DEVICES)
    return 0L;
  if (which > DEV_ELEM_TYPES)
    return 0L;
  return ifindex_map[ifindex][which];
}

/* reset the entry for the given device (ifindex argument) and object type
 * (which argument) to 0 */
inline void
remove_ifindex_map(unsigned ifindex, unsigned which)
{
  if (ifindex >= MAX_DEVICES)
    return 0L;
  if (which > DEV_ELEM_TYPES)
    return 0L;
  ifindex_map[ifindex][which] = 0;
}

#endif

