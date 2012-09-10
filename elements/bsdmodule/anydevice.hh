// -*- c-basic-offset: 4 -*-
#ifndef CLICK_ANYDEVICE_HH
#define CLICK_ANYDEVICE_HH
#include <click/element.hh>
#include <click/task.hh>
#include <click/cxxprotect.h>
CLICK_CXX_PROTECT
#define DEVICE_POLLING
#include <sys/socket.h>
#include <net/if.h>
#include <net/if_var.h>
#include <net/netisr.h>
#include <sys/limits.h>
CLICK_CXX_UNPROTECT
#include <click/cxxunprotect.h>
#define CLICK_CYCLE_COMPENSATION 0
#ifdef BSD_NETISRSCHED
# if __FreeBSD_version >= 800000
#  define NETISR_CLICK 13        // must match empty slots in net/netisr.h !!!
# else
#  define NETISR_CLICK 1         // must match empty slots in net/netisr.h !!!
# endif
#endif
#if HAVE_STRIDE_SCHED
# define CLICK_DEVICE_ADJUST_TICKETS 1
#endif
#define GET_STATS_RESET(a,b,c,d,e,f)	/* nothing */
#define SET_STATS(a,b,c)		/* nothing */
CLICK_DECLS

//extern int *polling;            // 1 = BSD poller; 2 = Click poller

extern struct ifnet _dev_click;

class AnyDeviceMap;

class AnyDevice : public Element { public:

    enum { CONFIGURE_PHASE_FROMHOST = CONFIGURE_PHASE_DEFAULT - 2,
           CONFIGURE_PHASE_TODEVICE = CONFIGURE_PHASE_DEFAULT - 1,
           CONFIGURE_PHASE_POLLDEVICE = CONFIGURE_PHASE_DEFAULT };

    AnyDevice() CLICK_COLD;
    ~AnyDevice() CLICK_COLD;

    const String &devname() const	{ return _devname; }
    struct ifnet *device() const	{ return _dev; }
    int ifindex() const			{ return _dev ? _dev->if_index : -1; }

    bool promisc() const                { return _promisc; }
    void set_promisc()                  { _promisc = true; }

    AnyDevice *next() const		{ return _next; }
    void set_next(AnyDevice *d)		{ _next = d; }
    void set_max_tickets(int t)		{ _max_tickets = t; }

    int find_device(bool, AnyDeviceMap *, ErrorHandler *);
    void set_device(net_device *, AnyDeviceMap *);
    void clear_device(AnyDeviceMap *);
    void adjust_tickets(int work);
    void intr_reschedule();

  protected:

    String _devname;
    struct ifnet *_dev;
    Task _task;

    bool _promisc : 1;
    AnyDevice *_next;

  private:

    int _max_tickets;
    int _idles;

};


class AnyTaskDevice : public AnyDevice { public:

    AnyTaskDevice() CLICK_COLD;

    void adjust_tickets(int work);

  protected:

    Task _task;
    int _max_tickets;
    int _idles;

};


inline void
AnyDevice::intr_reschedule(void)
{
#ifdef BSD_NETISRSCHED
    if (!_task.scheduled())
	_task.reschedule();
    //if (!polling || (polling && *polling != 2))
# if __FreeBSD_version >= 800000
        /* XXX: FreeBSD 8 does not have schednetisr() */
# else
	schednetisr(NETISR_CLICK);
# endif
#else
    _task.reschedule();
#endif
}


inline void
AnyDevice::adjust_tickets(int work)
{
#if CLICK_DEVICE_ADJUST_TICKETS
    int tix = _task.tickets();
    // int old_tix = tix;

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
    // click_chatter(" tickets from %d to %d", old_tix, tix);

  _task.set_tickets(tix);
#endif
}

class AnyDeviceMap { public:

    void initialize() CLICK_COLD;
    AnyDevice *lookup(struct ifnet *);
    AnyDevice *lookup_unknown(struct ifnet *);
    void insert(AnyDevice *);
    void remove(AnyDevice *);

  private:

    static const int MAP_SIZE = 64;
    AnyDevice *_unknown_map;
    AnyDevice *_map[MAP_SIZE];

};

inline AnyDevice *
AnyDeviceMap::lookup(struct ifnet *dev)
{
    if (dev == NULL)
	return NULL;

    AnyDevice *d = _map[dev->if_index % MAP_SIZE];
    while (d && d->device() != dev)
	d = d->next();
    return d;
}

struct ifnet *find_device_by_ether_address(const String &, Element *);

CLICK_ENDDECLS
#endif
