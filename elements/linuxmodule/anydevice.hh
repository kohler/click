#ifndef ANYDEVICE_HH
#define ANYDEVICE_HH
#include <click/element.hh>
#include <click/task.hh>

#include <click/cxxprotect.h>
CLICK_CXX_PROTECT
#include <linux/netdevice.h>
CLICK_CXX_UNPROTECT
#include <click/cxxunprotect.h>

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

class AnyDevice : public Element { public:

    AnyDevice();
    ~AnyDevice();

    const String &devname() const	{ return _devname; }
    net_device *device() const		{ return _dev; }
    int ifindex() const			{ return _dev ? _dev->ifindex : -1; }
    AnyDevice *next() const		{ return _next; }
    void set_next(AnyDevice *d)		{ _next = d; }

    int find_device(bool, ErrorHandler *);
    void adjust_tickets(int work);

  protected:

    String _devname;
    net_device *_dev;

    Task _task;
    int _max_tickets;
    int _idles;

    AnyDevice *_next;

};


inline void
AnyDevice::adjust_tickets(int work)
{
#if CLICK_DEVICE_ADJUST_TICKETS
  int tix = _task.tickets();
  
  // simple additive increase damped multiplicative decrease scheme
  if (work > 2) {
    tix += work;
    if (tix > _max_tickets)
      tix = _max_tickets;
    _idles = 0;
  } else if (work == 0) {
    _idles++;
    if (_idles >= 64) {
      if (tix > 64)
	tix -= (tix >> 5);
      else
	tix -= 2;
      if (tix < 1)
	tix = 1;
      _idles = 0;
    }
  }

  _task.set_tickets(tix);
#endif
}

class AnyDeviceMap { public:

    void initialize();
    AnyDevice *lookup(net_device *);
    AnyDevice *lookup_unknown(net_device *);
    void insert(AnyDevice *);
    void remove(AnyDevice *);

  private:

    static const int MAP_SIZE = 64;
    AnyDevice *_unknown_map;
    AnyDevice *_map[MAP_SIZE];

};

inline AnyDevice *
AnyDeviceMap::lookup(net_device *dev)
{
    AnyDevice *d = _map[dev->ifindex % MAP_SIZE];
    while (d && d->device() != dev)
	d = d->next();
    return d;
}

net_device *find_device_by_ether_address(const String &, Element *);

#endif
