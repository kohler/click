// -*- mode: c++; c-basic-offset: 4 -*-
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

#ifdef HAVE_STRIDE_SCHED
# define CLICK_DEVICE_ADJUST_TICKETS 1
#endif

#if CLICK_DEVICE_PRFCTR && __i386__

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
    uint64_t __now = click_get_cycles(); \
    tctr += __now - time_mark - CLICK_CYCLE_COMPENSATION; \
    time_mark = __now; \
  }

#else

#define GET_STATS_RESET(a,b,c,d,e,f)	/* nothing */
#define SET_STATS(a,b,c)		/* nothing */

#endif

#if LINUX_VERSION_CODE < KERNEL_VERSION(2, 4, 0)
typedef struct enet_statistics net_device_stats;
#define dev_hold(dev)		/* nada */
#define dev_put(dev)		/* nada */
#endif

class AnyDeviceMap;

class AnyDevice : public Element { public:

    AnyDevice();
    ~AnyDevice();

    const String &devname() const	{ return _devname; }
    net_device *device() const		{ return _dev; }
    int ifindex() const			{ return _dev ? _dev->ifindex : -1; }

    bool promisc() const		{ return _promisc; }
    void set_promisc()			{ _promisc = true; }

    int find_device(bool, AnyDeviceMap *, ErrorHandler *);
    void set_device(net_device *, AnyDeviceMap *);
    void clear_device(AnyDeviceMap *);
    void adjust_tickets(int work);

  protected:

    String _devname;
    net_device *_dev;

    Task _task;
    int _max_tickets;
    int _idles;

    bool _promisc : 1;
    bool _in_map : 1;
    AnyDevice *_next;

    friend class AnyDeviceMap;

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
    AnyDevice *lookup(net_device *, AnyDevice *);
    AnyDevice *lookup_unknown(net_device *, AnyDevice *);
    void lookup_all(net_device *, bool known, Vector<AnyDevice *> &);
    void insert(AnyDevice *);
    void remove(AnyDevice *);
    void move_to_front(AnyDevice *);

  private:

    enum { MAP_SIZE = 64 };
    AnyDevice *_unknown_map;
    AnyDevice *_map[MAP_SIZE];

};

inline AnyDevice *
AnyDeviceMap::lookup(net_device *dev, AnyDevice *last)
{
    if (!dev)
	return 0;
    AnyDevice *d = (last ? last->_next : _map[dev->ifindex % MAP_SIZE]);
    while (d && d->device() != dev)
	d = d->_next;
    return d;
}

net_device *dev_get_by_ether_address(const String &, Element *);

#endif
