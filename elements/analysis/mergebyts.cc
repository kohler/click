// -*- mode: c++; c-basic-offset: 4 -*-
#include <click/config.h>
#include <click/error.hh>
#include "mergebyts.hh"
#include <click/standard/scheduleinfo.hh>
#include <click/confparse.hh>
#include <click/router.hh>

MergeByTimestamp::MergeByTimestamp()
    : Element(1, 1), _vec(0)
{
    MOD_INC_USE_COUNT;
}

MergeByTimestamp::~MergeByTimestamp()
{
    MOD_DEC_USE_COUNT;
    delete[] _vec;
}

void
MergeByTimestamp::notify_ninputs(int n)
{
    set_ninputs(n);
}

int
MergeByTimestamp::configure(const Vector<String> &conf, ErrorHandler *errh)
{
    _stop = false;
    _dead_null = false;
    return cp_va_parse(conf, this, errh,
		       cpKeywords,
		       "STOP", cpBool, "stop when queue empty?", &_stop,
		       "NULL_IS_DEAD", cpBool, "inputs are dead once they return null?", &_dead_null,
		       0);
}

int 
MergeByTimestamp::initialize(ErrorHandler *errh)
{
    _vec = new Packet *[ninputs() + 1];
    if (!_vec)
	return errh->error("out of memory!");
    for (int i = 0; i < ninputs(); i++)
	_vec[i] = 0;
    _new = true;
    return 0;
}

void
MergeByTimestamp::uninitialize()
{
    for (int i = 0; i < ninputs(); i++)
	if (_vec[i])
	    _vec[i]->kill();
}

Packet *
MergeByTimestamp::pull(int)
{
    int which = -1;
    struct timeval *tv = 0;
    bool live_null = (!_dead_null || _new);
    for (int i = 0; i < ninputs(); i++) {
	if (!_vec[i] && live_null)
	    _vec[i] = input(i).pull();
	if (_vec[i]) {
	    struct timeval *this_tv = &_vec[i]->timestamp_anno();
	    if (!tv || timercmp(this_tv, tv, <)) {
		which = i;
		tv = this_tv;
	    }
	}
    }

    _new = false;
    
    if (which >= 0) {
	Packet *p = _vec[which];
	_vec[which] = 0;
	return p;
    } else {
	if (_stop)
	    router()->please_stop_driver();
	return 0;
    }
}

EXPORT_ELEMENT(MergeByTimestamp)
