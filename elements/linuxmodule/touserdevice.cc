/*

Programmer: Roman Chertov, Cliff Frey

All files in this distribution of are Copyright 2006 by the Purdue
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
#include <click/standard/scheduleinfo.hh>

#include <click/cxxprotect.h>
CLICK_CXX_PROTECT
#include <linux/poll.h>
#include <linux/wait.h>
#include <linux/sched.h>
#include <linux/fs.h>
#include <asm/uaccess.h>
CLICK_CXX_UNPROTECT
#include <click/cxxunprotect.h>

#include "touserdevice.hh"
#include "fromuserdevice.hh"


static const char DEV_NAME[] = "click_user";
static int DEV_MAJOR = 241;
static int DEV_NUM = 0;

struct file_operations *ToUserDevice::dev_fops;

ToUserDevice * volatile ToUserDevice::elem_map[256] = {0};

struct pcap_hdr {
    uint32_t magic_number;   /* magic number */
    uint16_t version_major;  /* major version number */
    uint16_t version_minor;  /* minor version number */
    int32_t  thiszone;	     /* GMT to local correction */
    uint32_t sigfigs;	     /* accuracy of timestamps */
    uint32_t snaplen;	     /* max length of captured packets, in octets */
    uint32_t network;	     /* data link type */
};

struct pcaprec_hdr {
    uint32_t ts_sec;	     /* timestamp seconds */
    uint32_t ts_usec;	     /* timestamp microseconds */
    uint32_t incl_len;	     /* number of octets of packet saved in file */
    uint32_t orig_len;	     /* actual length of packet */
};

ToUserDevice::ToUserDevice() : _task(this)
{
    _exit = false;
    _size = 0;
    _q = 0;
    _r_slot = 0;
    _w_slot = 0;
    _dev_major = DEV_MAJOR;
    _read_count = 0;
    _drop_count = 0;
    _sleep_proc = 0;
    _pkt_count = 0;
    _block_count = 0;
    _pkt_read_count = 0;
    _failed_count = 0;
    _from_user_device = 0;
}

ToUserDevice::~ToUserDevice()
{
}

extern struct file_operations *click_new_file_operations();

void
ToUserDevice::static_initialize()
{
    if ((dev_fops = click_new_file_operations())) {
	dev_fops->read	  = dev_read;
	dev_fops->write	  = dev_write;
	dev_fops->poll	  = dev_poll;
	dev_fops->open	  = dev_open;
	dev_fops->release = dev_release;
    }
}

void
ToUserDevice::static_cleanup()
{
    dev_fops = 0;		// proclikefs will free eventually
}

#define GETELEM(filp)		((ToUserDevice *) (((struct file_priv *)filp->private_data)->dev));

// open function - called when the "file" /dev/toclick is opened in userspace
int
ToUserDevice::dev_open(struct inode *inode, struct file *filp)
{
    int num = MINOR(inode->i_rdev); // low order number
    if (!elem_map[num]) {
	click_chatter("No ToUserDevice element for this device: %d\n", num);
	return -EIO;
    }
    //struct file_priv *f = (struct file_priv *)kmalloc(sizeof(struct file_priv), GFP_KERNEL);
    file_priv *f = (file_priv *) kmalloc(sizeof(struct file_priv), GFP_ATOMIC);
    f->dev = (ToUserDevice*)elem_map[num];
    f->read_once = 0;
    f->p = 0;
    filp->private_data = (void *)f;
    return 0;
}

// close function - called when the "file" /dev/toclick is closed in userspace
int
ToUserDevice::dev_release(struct inode *inode, struct file *filp)
{
    ToUserDevice *elem = GETELEM(filp);
    if (!elem) {
	click_chatter("Empty private struct!\n");
	return -EIO;
    }
    kfree(filp->private_data);
    return 0;
}

ssize_t
ToUserDevice::dev_write(struct file *filp, const char *buff, size_t len, loff_t *ppos)
{
    ToUserDevice *elem = GETELEM(filp);
    FromUserDevice *fud = elem->_from_user_device;
    if (!fud)
	return -EPERM;

    return fud->dev_write(buff, len, ppos);
}

ssize_t
ToUserDevice::dev_read(struct file *filp, char *buff, size_t len, loff_t *ppos)
{
    ToUserDevice *elem = GETELEM(filp);
    file_priv *f = (file_priv *)filp->private_data;
    unsigned nfetched = 0;
    ssize_t nread = 0;

    elem->_read_count++;

    if (elem->_debug)
	printk("read %d pkt %p\n", f->read_once, f->p);

    if (f->read_once == 0 && elem->_encap_type == type_encap_pcap) {
	/* on initial read, write out the pcap header */
	Packet *p = Packet::make(sizeof(struct pcap_hdr));
	if (!p)
	    return -ENOMEM;
	nfetched++;
	struct pcap_hdr *hdr = (struct pcap_hdr *)p->data();
	memset(hdr, 0, sizeof(struct pcap_hdr));
	hdr->magic_number = htonl(0xa1b2c3d4);
	hdr->version_major = htons(2);
	hdr->version_minor = htons(4);
	hdr->thiszone = htonl(0);
	hdr->sigfigs = htonl(0);
	hdr->snaplen = htonl(elem->_pcap_snaplen);
	hdr->network = htonl(elem->_pcap_network);
	f->p = p;
	f->read_once = 1;
	if (elem->_debug)
	    printk("%s:%d copied header\n", __func__, __LINE__);
    }

    if (f->p) {
	Packet *p = f->p;
	ssize_t to_copy = p->length();
	if (to_copy > len)
	    to_copy = len;
	if (copy_to_user(buff, p->data(), to_copy)) {
	    elem->_failed_count++;
	    click_chatter("%p{element}: Read Fault", elem);
	    p->kill();
	    return -EFAULT;
	}
	nread += to_copy;
	if (to_copy < p->length()) {
	    p->pull(nread);
	} else if (to_copy == p->length()) {
	    p->kill();
	    f->p = 0;
	}
	if (nread == len || elem->_type == type_packet)
	    return nread;
    }

    spin_lock(&elem->_lock); // LOCK
    if (!nread && elem->_blocking) {
	if ((filp->f_flags & O_NONBLOCK) && !elem->_size) {
	    spin_unlock(&elem->_lock); // UNLOCK
	    return -EAGAIN;
	}

	// there is a private field and that doesn't compile in C++ so we can't use DEFINE_WAIT macro
	wait_queue_t wq;
#include <click/cxxprotect.h>
	init_wait(&wq);
#include <click/cxxunprotect.h>
	while (!elem->_size && !elem->_exit) {
	    // need to put the process to sleep
	    elem->_block_count++;
	    elem->_sleep_proc++;
	    prepare_to_wait(&elem->_proc_queue, &wq, TASK_INTERRUPTIBLE);
	    if (elem->_size || elem->_exit) {
		finish_wait(&elem->_proc_queue, &wq);
		break;
	    }
	    spin_unlock(&elem->_lock); // UNLOCK
	    if (signal_pending(current)) {
		finish_wait(&elem->_proc_queue, &wq);
		return -ERESTARTSYS;
	    }
	    schedule();
	    spin_lock(&elem->_lock); // LOCK
	    elem->_sleep_proc--;
	}
	if (elem->_exit) {
	    spin_unlock(&elem->_lock); // UNLOCK
	    return -EIO;
	}
    }

    while (elem->_size
	   && (nfetched < elem->_max_burst || elem->_max_burst == 0)
	   && nread < len
	   && !elem->_exit) {
	nfetched++;
	Packet *p = elem->pop_packet();
	spin_unlock(&elem->_lock); // UNLOCK
	// unlock as don't need the lock, and can't have a lock and copy_to_user
	ssize_t to_copy = p->length();
	if (to_copy > len - nread)
	    to_copy = len - nread;
	if (copy_to_user(buff + nread, p->data(), to_copy)) {
	    elem->_failed_count++;
	    click_chatter("%p{element}: Read Fault", elem);
	    p->kill();
	    return -EFAULT;
	}
	nread += to_copy;
	if (elem->_type == type_stream && to_copy < p->length()) {
	    p->pull(to_copy);
	    f->p = p;
	} else {
	    p->kill();
	}
	spin_lock(&elem->_lock); // LOCK
	elem->_pkt_read_count++;
    }

    if (nread == 0 && elem->_exit)
	nread = -EIO;

    spin_unlock(&elem->_lock); // UNLOCK
    return nread;
}

uint
ToUserDevice::dev_poll(struct file *filp, struct poll_table_struct *pt)
{
    ToUserDevice *elem = GETELEM(filp);
    uint mask = 0;

    if (!elem)
	return 0;
    poll_wait(filp, &elem->_proc_queue, pt);

    FromUserDevice *fud = elem->_from_user_device;
    if (fud)
	mask |= fud->dev_poll();

    spin_lock(&elem->_lock); // LOCK
    if (elem->_exit || !elem->_blocking || elem->_size)
    {
	mask |= POLLIN | POLLRDNORM; /* readable */
	//click_chatter("ToUserDevice_poll Readable\n");
    }
    spin_unlock(&elem->_lock); // UNLOCK
    return mask;
}

int
ToUserDevice::configure(Vector<String> &conf, ErrorHandler *errh)
{
    int		 res;
    dev_t	 dev;

    if (!dev_fops)
	return errh->error("file operations missing");

    _pcap_network = 1;
    _pcap_snaplen = 65535;
    _debug = false;
    _drop_tail = true;
    _blocking = true;
    _max_burst = 1;
    _capacity = default_capacity;
    String encap = String::make_stable("none");
    String type;
    if (Args(conf, this, errh)
	.read_mp("DEV_MINOR", _dev_minor)
	.read("CAPACITY", _capacity)
	.read("BURST", _max_burst)
	.read("TYPE", WordArg(), type)
	.read("ENCAP", WordArg(), encap)
	.read("PCAP_NETWORK", _pcap_network)
	.read("PCAP_SNAPLEN", _pcap_snaplen)
	.read("DEBUG", _debug)
	.read("BLOCKING", _blocking)
	.read("DROP_TAIL", _drop_tail)
	.complete() < 0)
	return -1;

    if (_dev_minor >= 256)
	return errh->error("DEV_MINOR number too high");

    if (encap == "none")
	_encap_type = type_encap_none;
    else if (encap == "pcap")
	_encap_type = type_encap_pcap;
    else if (encap == "len32")
	_encap_type = type_encap_len32;
    else if (encap == "len_net16")
	_encap_type = type_encap_len_net16;
    else
	return errh->error("bad ENCAP type");

    if (type == "packet")
	_type = type_packet;
    else if (type == "stream")
	_type = type_stream;
    else if (!type)
	_type = (_encap_type == type_encap_none) ? type_packet : type_stream;
    else
	return errh->error("bad TYPE (must be packet or stream)");

    spin_lock_init(&_lock); // LOCK INIT
    if (!(_q = (Packet **)click_lalloc(_capacity * sizeof(Packet *))))
    {
	click_chatter("ToUserDevice Failed to alloc %lu slots\n", _capacity);
	return -1;
    }
    _dev_major = DEV_MAJOR;
    DEV_NUM++;
    if (DEV_NUM == 1)
    {
	// time to associate the devname with this class

	//register the device now. this will register 255 minor numbers
	res = register_chrdev(_dev_major, DEV_NAME, dev_fops);
	if (res < 0)
	{
	    click_chatter("Failed to Register Dev:%s Major:%d Minor:%d\n",
		DEV_NAME, _dev_major, _dev_minor);
	    click_lfree((char*)_q, _capacity * sizeof(Packet *));
	    _q = 0;
	    return - EIO;
	}
    }
    elem_map[_dev_minor] = this;
    init_waitqueue_head(&_proc_queue);
    return 0;
}

int
ToUserDevice::initialize(ErrorHandler *errh)
{
    if (input_is_pull(0))
    {
	ScheduleInfo::initialize_task(this, &_task, errh);
	_signal = Notifier::upstream_empty_signal(this, 0, &_task);
    }
    return 0;
}

void
ToUserDevice::cleanup(CleanupStage stage)
{
    if (stage < CLEANUP_CONFIGURED)
	return; // have to quit, as configure was never called
    spin_lock(&_lock); // LOCK
    DEV_NUM--;
    _exit = true; // signal for exit
    spin_unlock(&_lock); // UNLOCK

    if (_q) {
	wake_up_interruptible(&_proc_queue);
	while (waitqueue_active(&_proc_queue))
	    schedule();

	if (!DEV_NUM)
	    unregister_chrdev(_dev_major, DEV_NAME);

	reset();
	click_lfree((char*)_q, _capacity * sizeof(Packet *));
    }
}

void
ToUserDevice::reset()
{
    spin_lock(&_lock); // LOCK
    // now clear out the memory
    while (_size > 0) {
	_q[_r_slot]->kill();
	_r_slot++;
	if (_r_slot == _capacity)
	    _r_slot = 0;
	_size--;
    }
    spin_unlock(&_lock); // UNLOCK
}

bool
ToUserDevice::process(Packet *p)
{
    if (_encap_type != type_encap_none) {
	WritablePacket *wp = p->uniqueify();
	if (unlikely(!wp))
	    return false;
	if (_encap_type == type_encap_pcap) {
	    int orig_len = wp->length();
	    int this_len = wp->length();
	    if (this_len > _pcap_snaplen) {
		this_len = _pcap_snaplen;
		wp->take(wp->length() - _pcap_snaplen);
	    }
	    wp = wp->push(sizeof(struct pcaprec_hdr));
	    if (unlikely(!wp))
		return false;
	    struct pcaprec_hdr *h = (struct pcaprec_hdr *)wp->data();
	    memset(h, 0, sizeof(struct pcaprec_hdr));
	    Timestamp t = wp->timestamp_anno();
	    h->ts_sec = htonl(t.sec());
	    h->ts_usec = htonl(t.usec());
	    h->incl_len = htonl(this_len);
	    h->orig_len = htonl(orig_len);
	} else if (_encap_type == type_encap_len32) {
	    wp = wp->push(sizeof(uint32_t));
	    if (unlikely(!wp))
		return false;
	    uint32_t len = wp->length();
	    memcpy(wp->data(), &len, sizeof(uint32_t));
	} else if (_encap_type == type_encap_len_net16) {
	    wp = wp->push(sizeof(uint16_t));
	    if (unlikely(!wp))
		return false;
	    uint16_t len = htons(wp->length());
	    memcpy(wp->data(), &len, sizeof(uint16_t));
	}
	p = wp;
    }

    spin_lock(&_lock);
    _pkt_count++;

    if (_size >= _capacity) {
	_drop_count++;
	if (_drop_tail)
	    pop_packet()->kill();
	else {
	    p->kill();
	    spin_unlock(&_lock);
	    return false;
	}
    }

    _q[_w_slot] = p;
    _w_slot++;
    if (_w_slot == _capacity)
	_w_slot = 0;
    _size++;
    spin_unlock(&_lock); // UNLOCK
    // wake up procs if any are sleeping
    wake_up_interruptible(&_proc_queue);
    return true;
}

void
ToUserDevice::push(int, Packet *p)
{
    process(p);
}

// something is upstream so we can run
bool
ToUserDevice::run_task(Task *)
{
    Packet *p = input(0).pull();

    if (p)
	process(p);
    else if (!_signal)
	return false;
    _task.fast_reschedule();
    return p != 0;
}

Packet *
ToUserDevice::pop_packet()
{
    Packet *p = 0;
    if (_size > 0) {
	p = _q[_r_slot];
	_r_slot++;
	if (_r_slot == _capacity)
	    _r_slot = 0;
	_size--;
    }
    return p;
}

int
ToUserDevice::write_handler(const String &s_in, Element *e, void *thunk, ErrorHandler *errh)
{
    ToUserDevice *td = (ToUserDevice *)e;
    String s = cp_uncomment(s_in);
    switch ((intptr_t)thunk) {
    case h_reset: {
	td->reset();
	break;
    }
    }
    return 0;
}

void
ToUserDevice::add_handlers()
{
    add_data_handlers("count", Handler::OP_READ, &_pkt_count);
    add_data_handlers("size", Handler::OP_READ, &_size);
    add_data_handlers("drops", Handler::OP_READ, &_drop_count);
    add_data_handlers("capacity", Handler::OP_READ, &_capacity);
    add_data_handlers("read_count", Handler::OP_READ, &_read_count);
    add_data_handlers("read_fail_count", Handler::OP_READ, &_failed_count);
    add_data_handlers("pkt_read_count", Handler::OP_READ, &_pkt_read_count);
    add_data_handlers("drop_count", Handler::OP_READ, &_drop_count);
    add_data_handlers("block_count", Handler::OP_READ, &_block_count);
    add_data_handlers("sleeping_proc", Handler::OP_READ, &_sleep_proc);

    add_write_handler("reset", write_handler, h_reset);
    add_data_handlers("debug", Handler::OP_READ | Handler::OP_WRITE, &_debug);
    add_data_handlers("drop_tail", Handler::OP_READ | Handler::OP_WRITE, &_drop_tail);
    add_data_handlers("burst", Handler::OP_READ | Handler::OP_WRITE, &_max_burst);

    if (_encap_type == type_encap_pcap) {
	add_data_handlers("pcap_snaplen", Handler::OP_READ | Handler::OP_WRITE, &_pcap_snaplen);
	add_data_handlers("pcap_network", Handler::OP_READ | Handler::OP_WRITE, &_pcap_network);
    }

    if (input_is_pull(0))
	add_task_handlers(&_task);
}


ELEMENT_REQUIRES(linuxmodule FromUserDevice)
EXPORT_ELEMENT(ToUserDevice)
ELEMENT_MT_SAFE(ToUserDevice)
