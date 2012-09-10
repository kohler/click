// -*- mode: c++; c-basic-offset: 4 -*-
#ifndef ANYDEVICE_HH
#define ANYDEVICE_HH
#include <click/element.hh>
#include <click/task.hh>
class HandlerCall;

#include <click/cxxprotect.h>
CLICK_CXX_PROTECT
#include <linux/netdevice.h>
#if LINUX_VERSION_CODE >= KERNEL_VERSION(2, 6, 24)
# include <net/net_namespace.h>
# include <linux/percpu.h>
#endif
CLICK_CXX_UNPROTECT
#include <click/cxxunprotect.h>

// #define CLICK_DEVICE_CYCLES 1
// #define CLICK_DEVICE_PRFCTR 1
// #define CLICK_DEVICE_THESIS_STATS 1
// #define _DEV_OVRN_STATS_ 1
#define CLICK_CYCLE_COMPENSATION 0

#if HAVE_STRIDE_SCHED
# define CLICK_DEVICE_ADJUST_TICKETS 1
#endif

#if CLICK_DEVICE_PRFCTR && __i386__
# define CLICK_DEVICE_STATS 1
# define SET_STATS(p0mark, p1mark, time_mark) \
  { \
    unsigned high; \
    rdpmc(0, p0mark, high); \
    rdpmc(1, p1mark, high); \
    time_mark = click_get_cycles(); \
  }
# define GET_STATS_RESET(p0mark, p1mark, time_mark, pctr0, pctr1, tctr) \
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
# define CLICK_DEVICE_STATS 1
# define SET_STATS(p0mark, p1mark, time_mark) \
  { \
    time_mark = click_get_cycles(); \
  }
# define GET_STATS_RESET(p0mark, p1mark, time_mark, pctr0, pctr1, tctr) \
  { \
    uint64_t __now = click_get_cycles(); \
    tctr += __now - time_mark - CLICK_CYCLE_COMPENSATION; \
    time_mark = __now; \
  }
#else
# define GET_STATS_RESET(a,b,c,d,e,f)	/* nothing */
# define SET_STATS(a,b,c)		/* nothing */
#endif

#if (LINUX_VERSION_CODE >= KERNEL_VERSION(3, 1, 0) || defined(HAVE_NETIF_RECEIVE_SKB)) \
	&& !(HAVE___NETIF_RECEIVE_SKB || HAVE_NETIF_RECEIVE_SKB_EXTENDED)
# define CLICK_DEVICE_UNRECEIVABLE_SK_BUFF 1
extern "C" {
DECLARE_PER_CPU(sk_buff *, click_device_unreceivable_sk_buff);
}
#endif

#if !HAVE_CLICK_KERNEL && LINUX_VERSION_CODE >= KERNEL_VERSION(2, 6, 24) \
	&& LINUX_VERSION_CODE < KERNEL_VERSION(2, 6, 36) \
	&& (defined(CONFIG_BRIDGE) || defined(CONFIG_BRIDGE_MODULE)) \
	&& !HAVE_LINUX_NETDEV_RX_HANDLER_REGISTER
# define CLICK_FROMDEVICE_USE_BRIDGE 1
#endif

class AnyDeviceMap;

class AnyDevice : public Element { public:

    enum { CONFIGURE_PHASE_FROMHOST = CONFIGURE_PHASE_DEFAULT - 2,
	   CONFIGURE_PHASE_TODEVICE = CONFIGURE_PHASE_DEFAULT - 1,
	   CONFIGURE_PHASE_POLLDEVICE = CONFIGURE_PHASE_DEFAULT };

    AnyDevice() CLICK_COLD;
    ~AnyDevice() CLICK_COLD;

    const String &devname() const	{ return _devname; }
    net_device *device() const		{ return _dev; }
    int ifindex() const			{ return _dev ? _dev->ifindex : -1; }

    bool allow_nonexistent() const	{ return _allow_nonexistent; }
    bool promisc() const		{ return _promisc; }
    bool timestamp() const		{ return _timestamp; }

    int configure_keywords(Vector<String> &conf, ErrorHandler *errh,
			   bool is_reader);
    int initialize_keywords(ErrorHandler *errh);

    net_device *lookup_device(ErrorHandler *errh);
    enum {
	anydev_change = 1,
	anydev_from_device = 2
    };
    void set_device(net_device *dev, AnyDeviceMap *map, int flags);
    void clear_device(AnyDeviceMap *map, int flags);
    void alter_from_device(int delta);

    static inline net_device *get_by_name(const char *name) {
#if LINUX_VERSION_CODE >= KERNEL_VERSION(2, 6, 24)
	return dev_get_by_name(&init_net, name);
#else
	return dev_get_by_name(name);
#endif
    }

    static net_device *get_by_ether_address(const String &name, Element *context);

    // used for FromDevice
    struct fake_bridge {
	int magic;
	atomic_t refcount;
	enum {
	    click_magic = 0x817A10A7
	};
    };

  protected:

    String _devname;
    net_device *_dev;

    bool _promisc;
    bool _timestamp;
    bool _clear_anno;
    bool _in_map : 1;
    bool _quiet : 1;
    bool _allow_nonexistent : 1;
    bool _devname_exists : 1;
    bool _carrier_ok : 1;
    AnyDevice *_next;

    HandlerCall *_up_call;
    HandlerCall *_down_call;

    void alter_promiscuity(int delta);

    friend class AnyDeviceMap;

};

class AnyTaskDevice : public AnyDevice { public:

    AnyTaskDevice() CLICK_COLD;

    inline void adjust_tickets(int work);

  protected:

    Task _task;
    int _max_tickets;
    int _idles;

};


inline void
AnyTaskDevice::adjust_tickets(int work)
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

    void initialize() CLICK_COLD;
    inline void lock(bool write, unsigned long &flags);
    inline void unlock(bool write, unsigned long flags);
    inline AnyDevice *lookup(net_device *dev, AnyDevice *last) const;
    AnyDevice *lookup_unknown(net_device *dev, AnyDevice *last) const;
    int lookup_all(net_device *dev, bool known, AnyDevice **develt_store, int ndev) const;
    void insert(AnyDevice *develt, bool locked);
    void remove(AnyDevice *develt, bool locked);

  private:

    enum { MAP_SIZE = 64 };
    AnyDevice *_unknown_map;
    AnyDevice *_map[MAP_SIZE];
    rwlock_t _lock;

};

inline void
AnyDeviceMap::lock(bool write, unsigned long &flags)
{
    if (write)
	write_lock_irqsave(&_lock, flags);
    else
	read_lock_irqsave(&_lock, flags);
}

inline void
AnyDeviceMap::unlock(bool write, unsigned long flags)
{
    if (write)
	write_unlock_irqrestore(&_lock, flags);
    else
	read_unlock_irqrestore(&_lock, flags);
}

inline AnyDevice *
AnyDeviceMap::lookup(net_device *dev, AnyDevice *last) const
    // must be called between AnyDeviceMap::lock() ... unlock()
{
    if (!dev)
	return 0;
    AnyDevice *d = (last ? last->_next : _map[dev->ifindex % MAP_SIZE]);
    while (d && d->device() != dev)
	d = d->_next;
    return d;
}

#endif
