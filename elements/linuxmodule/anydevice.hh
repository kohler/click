#ifndef ANYDEVICE_HH
#define ANYDEVICE_HH
#include "element.hh"
extern "C" {
#include <linux/netdevice.h>
}

#define MAX_DEVICES	1024

/* we found that a 2 to 1 ratio prevents polldev from pulling too much stuff
 * for the transmit device. these two numbers must also be less than the rx
 * and tx dma ring size (64 and 16, respectively), so that cleaning the device
 * dma queues only need to happen once per run */

#define INPUT_BATCH     8
#define OUTPUT_BATCH    16

// #define CLICK_DEVICE_STATS 1
// #define _DEV_OVRN_STATS_ 1

#ifndef RR_SCHED
# define CLICK_DEVICE_ADJUST_TICKETS 1
#endif

#if CLICK_DEVICE_STATS

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

class AnyDevice : public Element { protected:
  
  String _devname;
  struct device *_dev;
  AnyDevice *_next;

 public:

  AnyDevice()				: _dev(0), _next(0) { }

  const String &devname() const		{ return _devname; }
  int ifindex() const			{ return _dev ? _dev->ifindex : -1; }
  AnyDevice *next() const		{ return _next; }
  void set_next(AnyDevice *d)		{ _next = d; }
  
};

class AnyDeviceMap {

  AnyDevice *_map[MAX_DEVICES];

 public:

  void initialize();
  AnyDevice *lookup(unsigned);
  int insert(AnyDevice *);
  void remove(AnyDevice *);
  
};

inline AnyDevice *
AnyDeviceMap::lookup(unsigned ifi)
{
  if (ifi >= MAX_DEVICES)
    return 0;
  else
    return _map[ifi];
}

struct device *find_device_by_ether_address(const String &, Element *);

#endif
