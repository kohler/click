#ifndef ANYDEVICE_HH
#define ANYDEVICE_HH
#include <click/element.hh>
extern "C" {
#include <linux/netdevice.h>
}

#define MAX_DEVICES	1024

// #define CLICK_DEVICE_CYCLES 1
// #define CLICK_DEVICE_PRFCTR 1
// #define CLICK_DEVICE_THESIS_STATS 1
// #define _DEV_OVRN_STATS_ 1
#define CLICK_CYCLE_COMPENSATION 0

#ifndef RR_SCHED
# define CLICK_DEVICE_ADJUST_TICKETS 1
#endif

#if CLICK_DEVICE_PRFCTR

#define CLICK_DEVICE_STATS 1
#define SET_STATS(p0mark, p1mark, time_mark) \
  { \
    unsigned high; \
    rdpmc(0, p0mark, high); \
    rdpmc(1, p1mark, high); \
    time_mark = click_get_cycles(); \
  }
#define GET_STATS_RESET(p0mark, p1mark, time_mark, pctr0, pctr1, tctr) \
  { \
    unsigned high; \
    unsigned low01, low11; \
    tctr += click_get_cycles() - time_mark - CLICK_CYCLE_COMPENSATION; \
    rdpmc(0, low01, high); \
    rdpmc(1, low11, high); \
    pctr0 += (low01 >= p0mark) ? low01-p0mark : (UINT_MAX-p0mark+low01); \
    pctr1 += (low11 >= p1mark) ? low11-p1mark : (UINT_MAX-p1mark+low11); \
    rdpmc(0, p0mark, high); \
    rdpmc(1, p1mark, high); \
    time_mark = click_get_cycles(); \
  }

#elif CLICK_DEVICE_CYCLES

#define CLICK_DEVICE_STATS 1
#define SET_STATS(p0mark, p1mark, time_mark) \
  { \
    time_mark = click_get_cycles(); \
  }
#define GET_STATS_RESET(p0mark, p1mark, time_mark, pctr0, pctr1, tctr) \
  { \
    unsigned long long __now = click_get_cycles(); \
    tctr += __now - time_mark - CLICK_CYCLE_COMPENSATION; \
    time_mark = __now; \
  }

#else

#define GET_STATS_RESET(a,b,c,d,e,f)	/* nothing */
#define SET_STATS(a,b,c)		/* nothing */

#endif

class AnyDevice : public Element { protected:
  
  String _devname;
  struct device *_dev;
  AnyDevice *_next;

  int _idles;
  void adjust_tickets(int work);

 public:

  AnyDevice();
  ~AnyDevice();

  const String &devname() const		{ return _devname; }
  int ifindex() const			{ return _dev ? _dev->ifindex : -1; }
  AnyDevice *next() const		{ return _next; }
  void set_next(AnyDevice *d)		{ _next = d; }
  
};

  
inline void
AnyDevice::adjust_tickets(int work)
{
#if CLICK_DEVICE_ADJUST_TICKETS  
  int adj = 0;
  // simple additive increase damped multiplicative decrease scheme
  if (work > 2) {
    adj = work;
    _idles = 0;
  }
  else if (work == 0) {
    _idles ++;
    if (_idles >= 64) {
      adj = 0-(tickets()>>5);
      if (adj == 0) 
	adj = -2;
      _idles = 0;
    }
  }
  adj_tickets(adj);
#endif
}

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
