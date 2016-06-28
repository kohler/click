/*
 * codel.{cc,hh} -- element implements CoDel dropping policy
 * Apurv Bhartia
 *
 * Copyright (c) 2014 Cisco Systems, Inc.
 */

#include <click/config.h>
#include "codel.hh"
#include <click/standard/storage.hh>
#include <click/routervisitor.hh>
#include <click/error.hh>
#include <click/router.hh>
#include <click/args.hh>
#include <click/straccum.hh>
#include <click/integers.hh>
#include <click/packet_anno.hh>
CLICK_DECLS

#define CODEL_DEBUG 0

CoDel::CoDel()
{
}

CoDel::~CoDel()
{
}

int
CoDel::finish_configure(const String &queues_string, ErrorHandler *errh)
{
    // check queues_string, but only if queues have not been configured already
    if (queues_string && !_queue_elements.size()) {
	Vector<String> eids;
	cp_spacevec(queues_string, eids);
	_queue_elements.clear();
	for (int i = 0; i < eids.size(); i++)
	    if (Element *e = router()->find(eids[i], this, errh))
		_queue_elements.push_back(e);
	if (eids.size() != _queue_elements.size())
	    return -1;
    }

    return 0;
}

int
CoDel::configure(Vector<String> &conf, ErrorHandler *errh)
{
    // initialize the target and interval with CoDel-recommended values (5 and 100ms respectively)
    _codel_target_ts = Timestamp::make_msec(0, 5);
    _codel_interval_ts = Timestamp::make_msec(0, 100);

    String queues_string = String();
    if (Args(conf, this, errh)
    .read_p("TARGET", _codel_target_ts)
    .read_p("INTERVAL", _codel_interval_ts)
	.read("QUEUES", AnyArg(), queues_string)
	.complete() < 0)
        return -1;

    return finish_configure(queues_string, errh);
}

int
CoDel::initialize(ErrorHandler *errh)
{
    // Find the next queues upstream
    _queues.clear();
    _queue1 = 0;

    if (_queue_elements.empty()) {
	ElementCastTracker filter(router(), "Storage");
	int ok = router()->visit_upstream(this, 0, &filter);
	if (ok < 0)
	    return errh->error("flow-based router context failure");
	_queue_elements = filter.elements();
    }

    if (_queue_elements.empty())
	return errh->error("no nearby Queues");
    for (int i = 0; i < _queue_elements.size(); i++)
	if (Storage *s = (Storage *)_queue_elements[i]->cast("Storage"))
	    _queues.push_back(s);
	else
	    errh->error("%<%s%> is not a Storage element", _queue_elements[i]->name().c_str());
    if (_queues.size() != _queue_elements.size())
	return -1;
    else if (_queues.size() == 1)
	_queue1 = _queues[0];

    _total_drops = 0;
    _state_drops = 0;
    _dropping = false;

    _first_above_time = Timestamp();
    _drop_next = Timestamp();

    return 0;
}

int
CoDel::queue_size() const
{
    if (_queue1)
	return _queue1->size();
    else {
	int s = 0;
	for (int i = 0; i < _queues.size(); i++)
	    s += _queues[i]->size();
	return s;
    }
}

inline void
CoDel::handle_drop(Packet *p)
{
    if (noutputs() == 1)
        p->kill();
    _total_drops++;
}

// delegate to the codel handler on a pull-request which can then generate (or not) the packet //
Packet *
CoDel::pull(int)
{
    return delegate_codel();
}

// helper: pull a packet, and tracks if the sojourn time of the packet is above the target //
Packet *
CoDel::dequeue_and_track_sojourn_time(Timestamp now, bool &retVal)
{
    _ok_to_drop = 0;
    Packet *p = input(0).pull();

    if (p == NULL) {
        // if no packet then reset _first_above_time
        _first_above_time.assign(0, 0);
        retVal = false;
        return NULL;
    } else if (!FIRST_TIMESTAMP_ANNO(p).sec()) {
        // if FIRST_TIMESTAMP_ANNO not set, then do nothing; imp else CoDel would misbehave!
        retVal = false;
        return p;
    } else {
        Timestamp sojourn_time = now - FIRST_TIMESTAMP_ANNO(p);

#if CODEL_DEBUG
        click_chatter("[%d] [%s] sojourn_time: %s pkt_ts: %s target: %s", EXTRA_PACKETS_ANNO(p), now.unparse().c_str(), sojourn_time.unparse().c_str(), FIRST_TIMESTAMP_ANNO(p).unparse().c_str(), _codel_target_ts.unparse().c_str());
#endif

        if (sojourn_time < _codel_target_ts) {
            // sojourn_time not high enough, reset again
            _first_above_time.assign(0, 0);
        } else {
            // check if the packet needs to be dropped
            if (_first_above_time == Timestamp::make_msec(0, 0)) {
                // first time above sojourn time, then check again later
                _first_above_time = now + _codel_interval_ts;
            } else if (now >= _first_above_time) {
                // mark to drop it
                _ok_to_drop = 1;
            }
        }
    }
    retVal = true;
    return p;
}

// heavy-lifter - calls dequeue_and_track_sojourn_time and drops packets, if required //
Packet*
CoDel::delegate_codel()
{
    Packet *p = NULL;
    bool ret_val = false;
    Timestamp now = Timestamp::now();

    p = dequeue_and_track_sojourn_time(now, ret_val);

    // ret_val can be false in two cases:
    //  1. no packet in the queue to pull from
    //  2. FIRST_TIMESTAMP_ANNO not set in the packet - so 'sojourn_time' cannot be calculated'
    //  In either case, just return and do nothing

    if (!ret_val) {
        _dropping = false;
        return p;
    }

    // already in the dropping state
    if (_dropping) {
        // is it time to leave the dropping state?
        if (!_ok_to_drop) {
            _dropping = false;
        } else if (now >= _drop_next) {
            // drop the current packet and dequeue the next
            while ((now >= _drop_next) && _dropping) {
                assert(now >= _drop_next);
                handle_drop(p);
#if CODEL_DEBUG
                click_chatter("total_drops: %d, now: %s, drop_next: %s\n", _total_drops, now.unparse().c_str(), _drop_next.unparse().c_str());
#endif
                ++_state_drops;
                p = dequeue_and_track_sojourn_time(now, ret_val);

                if (!_ok_to_drop) {
                    _dropping = false;
                } else {
                    _drop_next = control_law(_drop_next);
                }
            }
        }
    } else if (_ok_to_drop && ((now - _drop_next < _codel_interval_ts) || (now - _first_above_time >= _codel_interval_ts))) {

        // not in the dropping state - want to enter? then check:
        // 1. been in the 'dropping' state recently, or
        // 2. first_above_time been above 'interval'

        // drop the packet, dequeue next packet and enter dropping state
        handle_drop(p);
        p = dequeue_and_track_sojourn_time(now, ret_val);
        _dropping = true;

        if (now - _drop_next < _codel_interval_ts) {
            _state_drops = (_state_drops > 2) ? (_state_drops - 2) : 1;
        } else {
            _state_drops = 1;
        }
        _drop_next = control_law(now);
    }
    return p;
}

// determines the next drop time of the packet - scaling done to allow usage of int_sqrt to minimize floating point arithmetic, etc. //
Timestamp
CoDel::control_law(Timestamp t)
{
    uint32_t scale_factor = 1 << 4;
    uint32_t scale_factor_squared = scale_factor * scale_factor;
    uint32_t scaled_codel_interval = _codel_interval_ts.msecval() * scale_factor;
    uint32_t scaled_state_drops = _state_drops * scale_factor_squared;

    uint32_t val_click_ns = int_divide(scaled_codel_interval * Timestamp::nsec_per_msec, int_sqrt(scaled_state_drops));

    uint32_t val_click_sec;
    uint32_t rem_ns = int_divide(val_click_ns, Timestamp::nsec_per_sec, val_click_sec);   

    Timestamp val_click_ts = Timestamp::make_nsec(val_click_sec, rem_ns);
    return (t + val_click_ts);
}

// HANDLERS

String
CoDel::read_handler(Element *f, void *vparam)
{
    CoDel *codel = (CoDel *)f;
    StringAccum sa;
    switch ((intptr_t)vparam) {
        case 2:     // stats //
            sa << codel->queue_size() << " current queue\n"
               << codel->drops() << " drops\n";
            return sa.take_string();

        case 3:	    // queues //
            for (int i = 0; i < codel->_queue_elements.size(); i++)
                sa << codel->_queue_elements[i]->name() << "\n";
            return sa.take_string();

        default:	// config //
            sa << codel->_codel_target_ts.unparse().c_str() << "s, " << codel->_codel_interval_ts.unparse().c_str() << " s, ";
            return sa.take_string();
    }
}

void
CoDel::add_handlers()
{
    add_data_handlers("drops", Handler::OP_READ, &_total_drops);
    add_read_handler("codel_interval", read_keyword_handler, "1 INTERVAL");
    add_write_handler("codel_interval", reconfigure_keyword_handler, "1 INTERVAL");
    add_read_handler("codel_target", read_keyword_handler, "0 TARGET");
    add_write_handler("codel_target", reconfigure_keyword_handler, "0 TARGET");
    add_read_handler("stats", read_handler, 2);
    add_read_handler("queues", read_handler, 3);
    add_read_handler("config", read_handler, 4);
}

CLICK_ENDDECLS
ELEMENT_REQUIRES(int64)
EXPORT_ELEMENT(CoDel)
