// -*- c-basic-offset: 4 -*-
#ifndef CLICK_ANYDEVICE_HH
#define CLICK_ANYDEVICE_HH
#include <click/element.hh>
#include <click/task.hh>

#include <click/cxxprotect.h>
CLICK_CXX_PROTECT
#include <sys/socket.h>
#include <net/if.h>
#include <net/if_var.h>
#include <machine/limits.h>
CLICK_CXX_UNPROTECT
#include <click/cxxunprotect.h>

#define CLICK_CYCLE_COMPENSATION 0

#ifndef RR_SCHED
# define CLICK_DEVICE_ADJUST_TICKETS 1
#endif

#define GET_STATS_RESET(a,b,c,d,e,f)	/* nothing */
#define SET_STATS(a,b,c)		/* nothing */

class AnyDevice : public Element { public:

    enum { CONFIGURE_PHASE_FROMOS = CONFIGURE_PHASE_DEFAULT,
	   CONFIGURE_PHASE_TODEVICE = CONFIGURE_PHASE_FROMOS + 1 };

    AnyDevice();
    ~AnyDevice();

    const String &devname() const	{ return _devname; }
    struct ifnet *device() const	{ return _dev; }
    int ifindex() const			{ return _dev ? _dev->if_index : -1; }
    AnyDevice *next() const		{ return _next; }
    void set_next(AnyDevice *d)		{ _next = d; }
    void set_max_tickets(int t)		{ _max_tickets = t; }
    int wakeup();
    void set_need_wakeup()		{ _need_wakeup = true; }
    void clear_need_wakeup()		{ _need_wakeup = false; }

    int find_device(bool, ErrorHandler *);
    void adjust_tickets(int work);

  protected:

    String _devname;
    Task _task;

  private:

    bool _need_wakeup;
    int _max_tickets;
    int _idles;
    AnyDevice *_next;
    struct ifnet *_dev;

};

inline int
AnyDevice::wakeup()
{
    if (_need_wakeup) {
	_need_wakeup = false;
	_task.wakeup();
	return 1;
    } else
	return 0;
}

inline void
AnyDevice::adjust_tickets(int work)
{
#if CLICK_DEVICE_ADJUST_TICKETS
    int tix = _task.tickets();
    int old_tix = tix;

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

    void initialize();
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

#endif
