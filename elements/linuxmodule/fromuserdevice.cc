/*

Programmer: Roman Chertov, Cliff Frey

All files in this distribution of are Copyright 2005 by the Purdue
Research Foundation of Purdue University. All rights reserved.
Redistribution and use in source and binary forms are permitted
provided that this entire copyright notice is duplicated in all such
copies, and that any documentation, announcements, and other materials
related to such distribution and use acknowledge that the software was
developed at Purdue University, West Lafayette, IN.  No charge may be
made for copies, derivations, or distributions of this material
without the express written consent of the copyright holder. Neither
the name of the University nor the name of the author may be used to
endorse or promote products derived from this material without
specific prior written permission. THIS SOFTWARE IS PROVIDED ``AS IS''
AND WITHOUT ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, WITHOUT
LIMITATION, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
ANY PARTICULAR PURPOSE.

Copyright (c) 2011 Meraki, Inc.

*/

#include <click/config.h>
#include <click/glue.hh>
#include <click/error.hh>
#include <click/args.hh>
#include <click/router.hh>

#include <click/cxxprotect.h>
CLICK_CXX_PROTECT
#include <linux/poll.h>
#include <linux/wait.h>
#include <linux/sched.h>
#include <asm/uaccess.h>
CLICK_CXX_UNPROTECT
#include <click/cxxunprotect.h>

#include "fromuserdevice.hh"
#include "touserdevice.hh"


FromUserDevice::FromUserDevice()
{
    spin_lock_init(&_lock);
    _size = 0;
    _buff = 0;
    _r_slot = 0;
    _w_slot = 0;

    _write_count = 0;
    _drop_count = 0;
    _pkt_count = 0;
    _block_count = 0;
    _failed_count = 0;
    _exit = false;
    _max = 0;

    _sleep_proc = 0;
    init_waitqueue_head(&_proc_queue);
}

FromUserDevice::~FromUserDevice()
{
}

ssize_t
FromUserDevice::dev_write(const char *buf, size_t count, loff_t *ppos)
{
    ulong flags;

    //click_chatter("FromUserDevice_write %d\n", count);

    if (count > _max_pkt_size)
    {
	spin_lock_irqsave(&_lock, flags);
	_drop_count++;
	spin_unlock_irqrestore(&_lock, flags);
	return -EMSGSIZE;
    }

    // we do copy_from_user before we hold the spinlock
    WritablePacket *p = WritablePacket::make(_headroom, 0, count, _tailroom);
    if (!p)
	return -ENOMEM;
    int err = copy_from_user((char*)p->data(), buf, count);
    if (err != 0) {
	p->kill();
	spin_lock_irqsave(&_lock, flags);
	_failed_count++;
	spin_unlock_irqrestore(&_lock, flags);
	return -EFAULT;
    }

    // there is a private field and that doesn't compile in C++ so we can't use DEFINE_WAIT macro
    wait_queue_t wq;
#include <click/cxxprotect.h>
    init_wait(&wq);
#include <click/cxxunprotect.h>

    spin_lock_irqsave(&_lock, flags);
    while (1) {
	if (_exit) {
	    spin_unlock_irqrestore(&_lock, flags);
	    p->kill();
	    return -EIO;
	}
	if (_size < _capacity) {
	    break;
	} else {
	    // need to put the process to sleep
	    _sleep_proc++;
	    _block_count++;
	    prepare_to_wait(&_proc_queue, &wq, TASK_INTERRUPTIBLE);
	    spin_unlock_irqrestore(&_lock, flags);
	    if (signal_pending(current)) {
		finish_wait(&_proc_queue, &wq);
		p->kill();
		return -ERESTARTSYS;
	    }

	    //interruptible_sleep_on(&_proc_queue);
	    schedule();
	    finish_wait(&_proc_queue, &wq);
	    spin_lock_irqsave(&_lock, flags);
	    _sleep_proc--;
	    if (_exit) {
		spin_unlock_irqrestore(&_lock, flags);
		p->kill();
		return -EIO;
	    }
	}
    }
    // put the packet pointer into the buffer
    _buff[_w_slot] = p;

    _w_slot++;
    if (_w_slot == _capacity)
	_w_slot = 0;
    _size++;
    _write_count++;
    if (_max < _size)
	_max = _size;

    spin_unlock_irqrestore(&_lock, flags);

    _empty_note.wake();
    return count;
}

void *
FromUserDevice::cast(const char *n)
{
    if (strcmp(n, Notifier::EMPTY_NOTIFIER) == 0)
	return static_cast<Notifier *>(&_empty_note);
    else
	return Element::cast(n);
}

int
FromUserDevice::configure(Vector<String> &conf, ErrorHandler *errh)
{
    ToUserDevice *tud;
    _empty_note.initialize(Notifier::EMPTY_NOTIFIER, router());
    _headroom = Packet::default_headroom;
    _tailroom = 0;
    _max_pkt_size = 1536;
    _capacity = 64;
    if (Args(conf, this, errh)
	.read_mp("TO_USER_DEVICE", ElementCastArg("ToUserDevice"), tud)
	.read("CAPACITY", _capacity)
	.read("HEADROOM", _headroom)
	.read("TAILROOM", _tailroom)
	.read("MAX_PACKET_SIZE", _max_pkt_size)
	.complete() < 0)
	return -1;

    if (tud->_from_user_device)
	return errh->error("%p{element} already has a registered FromUserDevice", tud);
    if (tud->_type != ToUserDevice::type_packet)
	return errh->error("FromUserDevice only supports 'TYPE packet' ToUserDevice elements");
    tud->_from_user_device = this;

    if (_capacity == 0)
	return errh->error("CAPACITY must be greater than zero");
    if (!(_buff = (Packet**)click_lalloc(_capacity * sizeof(Packet*))))
	return errh->error("failed to allocate memory");
    return 0;
}

uint
FromUserDevice::dev_poll()
{
    uint mask = 0;
    ulong flags;

    spin_lock_irqsave(&_lock, flags);
    if (_exit || _size < _capacity)
	mask |= POLLOUT | POLLWRNORM;
    spin_unlock_irqrestore(&_lock, flags);
    return mask;
}

Packet *
FromUserDevice::pull(int)
{
    Packet *p = 0;
    ulong flags;

    spin_lock_irqsave(&_lock, flags);
    if (_size) {
	p = _buff[_r_slot];
	_r_slot++;
	if (_r_slot == _capacity)
	    _r_slot = 0;
	_size--;
	_pkt_count++;
    }
    if (!p)
	_empty_note.sleep();
    spin_unlock_irqrestore(&_lock, flags);
    // wake up procs if any are sleeping
    wake_up_interruptible(&_proc_queue);
    return p;
}

void
FromUserDevice::cleanup(CleanupStage stage)
{
    ulong flags;

    if (stage < CLEANUP_CONFIGURED)
	return; // have to quit, as configure was never called

    spin_lock_irqsave(&_lock, flags);
    _exit = true;
    spin_unlock_irqrestore(&_lock, flags);

    if (_buff) {
	wake_up_interruptible(&_proc_queue);
	while (waitqueue_active(&_proc_queue))
	    schedule();
	// now clear out the memory
	while (_size) {
	    Packet *p = _buff[_r_slot];
	    _r_slot = (_r_slot + 1) % _capacity;
	    p->kill();
	    _size--;
	}
	click_lfree((char*)_buff, _capacity * sizeof(Packet*));
    }
}

void
FromUserDevice::add_handlers()
{
    add_data_handlers("count", Handler::h_read, &_pkt_count);
    add_data_handlers("failed", Handler::h_read, &_failed_count);
    add_data_handlers("blocks", Handler::h_read, &_block_count);
    add_data_handlers("size", Handler::h_read, &_size);
    add_data_handlers("drops", Handler::h_read, &_drop_count);
    add_data_handlers("writes", Handler::h_read, &_write_count);
    add_data_handlers("capacity", Handler::h_read, &_capacity);
    add_data_handlers("max", Handler::h_read, &_max);
}

ELEMENT_REQUIRES(linuxmodule ToUserDevice)
EXPORT_ELEMENT(FromUserDevice)
ELEMENT_MT_SAFE(FromUserDevice)
