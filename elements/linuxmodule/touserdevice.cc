/*

Programmer: Roman Chertov

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
*/

#include <click/config.h>
#include <click/glue.hh>
#include <click/error.hh>
#include <click/confparse.hh>
#include <click/router.hh>
#include <click/standard/scheduleinfo.hh>

#include <click/cxxprotect.h>
CLICK_CXX_PROTECT
#include <linux/mm.h>
#include <linux/module.h>
#include <linux/version.h>
#include <linux/poll.h>
#include <linux/fs.h>
#include <asm/types.h>
#include <asm/uaccess.h>
#include <linux/ip.h>
#include <linux/inetdevice.h>
#include <net/route.h>
CLICK_CXX_UNPROTECT
#include <click/cxxunprotect.h>

#include "touserdevice.hh"
#include <click/hashmap.hh>
#include <click/llrpc.h>


static char DEV_NAME[] = "touser";
static int  DEV_MAJOR = 241;
static int  DEV_MINOR = 0;
static int  DEV_NUM = 0;

struct file_operations *ToUserDevice::dev_fops;
static struct mutex ToUserDevice::_ioctl_mutex;


static volatile ToUserDevice *elem[20] = {0};


ToUserDevice::ToUserDevice() : _task(this)
{
    _exit = false;
    _size = 0;
    _capacity = default_capacity;
    _devname = DEV_NAME;
    _q = 0;
    _r_slot = 0;
    _w_slot = 0;
    _dev_major = DEV_MAJOR;
    _dev_minor = DEV_MINOR;
    _read_count = 0;
    _drop_count = 0;
    _sleep_proc = 0;
    _pkt_count = 0;
    _block_count = 0;
    _pkt_read_count = 0;
    _failed_count = 0;
}

ToUserDevice::~ToUserDevice()
{

}

extern struct file_operations *click_new_file_operations();

void ToUserDevice::static_initialize()
{
    if ((dev_fops = click_new_file_operations())) {
	dev_fops->read    = dev_read;
	dev_fops->poll    = dev_poll;
	dev_fops->open    = dev_open;
	dev_fops->release = dev_release;
#if HAVE_UNLOCKED_IOCTL
	dev_fops->unlocked_ioctl = dev_unlocked_ioctl;
#else
	dev_fops->ioctl	  = dev_ioctl;
#endif
    }

#if LINUX_VERSION_CODE >= KERNEL_VERSION(2, 6, 36)
    mutex_init(&_ioctl_mutex);
#endif
}

void ToUserDevice::static_cleanup()
{
    dev_fops = 0;		// proclikefs will free eventually
}

#define GETELEM(filp)		((ToUserDevice *) ((uintptr_t) filp->private_data & ~(uintptr_t) 1))

// open function - called when the "file" /dev/toclick is opened in userspace
int ToUserDevice::dev_open(struct inode *inode, struct file *filp)
{
    int num = MINOR(inode->i_rdev); // low order number
    click_chatter("**ToUserDevice_open %d\n", num);
    if (!elem[num])
    {
        click_chatter("No ToUserDevice element for this device: %d\n", num);
        return -EIO;
    }
    filp->private_data = (void *) elem[num];
    return 0;
}

// close function - called when the "file" /dev/toclick is closed in userspace
int ToUserDevice::dev_release(struct inode *inode, struct file *filp)
{
    ToUserDevice *elem = GETELEM(filp);
    if (!elem) {
        click_chatter("Empty private struct!\n");
        return -EIO;
    }
    click_chatter("ToUserDevice_release ReadCounter: %lu\n", elem->_read_count);
    return 0;
}

#if HAVE_UNLOCKED_IOCTL
long ToUserDevice::dev_unlocked_ioctl(struct file *filp, unsigned int command,
				      unsigned long address)
{
	mutex_lock(&_ioctl_mutex);
	long ret = dev_ioctl(NULL, filp, command, address);
	mutex_unlock(&_ioctl_mutex);
	return ret;
}
#endif

int ToUserDevice::dev_ioctl(struct inode *inode, struct file *filp,
			    unsigned command, unsigned long address)
{
    ToUserDevice *elem = GETELEM(filp);
    if (elem->_exit)
	return -EIO;
    else if (command == CLICK_IOC_TOUSERDEVICE_GET_MULTI)
	return ((uintptr_t) filp->private_data) & 1;
    else if (command == CLICK_IOC_TOUSERDEVICE_SET_MULTI) {
	if ((int) address != 0 && (int) address != 1)
            return -EINVAL;
	filp->private_data = (void *) ((uintptr_t) elem | (int) address);
	return 0;
    } else
	return -EINVAL;
}

ssize_t ToUserDevice::dev_read(struct file *filp, char *buff, size_t len, loff_t *ppos)
{
    ToUserDevice *elem = GETELEM(filp);
    int multi = ((uintptr_t) filp->private_data) & 1;
    int err;
    // there is a private field and that doesn't compile in C++ so we can't use DEFINE_WAIT macro
    wait_queue_t wq;
#include <click/cxxprotect.h>
    init_wait(&wq);
#include <click/cxxunprotect.h>

    spin_lock(&elem->_lock); // LOCK
    elem->_read_count++;
    while (!elem->_size && !elem->_exit) {
	// need to put the process to sleep
	elem->_block_count++;
	elem->_sleep_proc++;
	prepare_to_wait(&elem->_proc_queue, &wq, TASK_INTERRUPTIBLE);
	spin_unlock(&elem->_lock); // UNLOCK
	schedule();
	finish_wait(&elem->_proc_queue, &wq);
	if (elem->_exit)
	    return -EIO;
	spin_lock(&elem->_lock); // LOCK
	elem->_sleep_proc--;
    }

    ssize_t nread = 0;
    unsigned nfetched = 0;
    while (elem->_size
	   && (nfetched == 0 || multi)
	   && (nfetched < elem->_max_burst || elem->_max_burst == 0)
	   && nread < len
	   && !elem->_exit) {
	Packet *p = elem->_q[elem->_r_slot];
	// user buffer is full
	if (nfetched > 0 && nread + sizeof(int) + p->length() > len)
	    break;
	elem->_r_slot++;
	if (elem->_r_slot == elem->_capacity)
	    elem->_r_slot = 0;
	elem->_size--;
	spin_unlock(&elem->_lock);
	// unlock as don't need the lock, and can't have a lock and copy_to_user

	// copy packet to user level
	if (multi) {
	    int len = p->length();
	    if (copy_to_user(buff + nread, &len, sizeof(int))) {
		elem->_failed_count++;
		click_chatter("Read Fault");
		p->kill();
		return -EFAULT;
	    }
	    nread += sizeof(int);
	}

	ssize_t to_copy = p->length();
	if (to_copy > len - nread)
	    to_copy = len - nread;
	if (copy_to_user(buff + nread, p->data(), to_copy)) {
	    elem->_failed_count++;
	    click_chatter("Read Fault");
	    p->kill();
	    return -EFAULT;
	}
	nread += to_copy;

	// to get int alignment
	if (multi && nread % sizeof(int))
	    nread += sizeof(int) - nread % sizeof(int);

	p->kill();
	spin_lock(&elem->_lock);
	nfetched++;
        elem->_pkt_read_count++;
    }

    if (nread == 0 && elem->_exit)
	nread = -EIO;
    spin_unlock(&elem->_lock);
    return nread;
}

uint ToUserDevice::dev_poll(struct file *filp, struct poll_table_struct *pt)
{
    ToUserDevice *elem = GETELEM(filp);
    uint    mask = 0;

    //click_chatter("ToUserDevice_poll\n");
    if (!elem || elem->_exit)
	return mask;

    poll_wait(filp, &elem->_proc_queue, pt);
    spin_lock(&elem->_lock); // LOCK
    if (elem->_size)
    {
        mask |= POLLIN | POLLRDNORM; /* readable */
        //click_chatter("ToUserDevice_poll Readable\n");
    }
    spin_unlock(&elem->_lock); // UNLOCK
    return mask;
}

int ToUserDevice::configure(Vector<String> &conf, ErrorHandler *errh)
{
    int          res;
    dev_t        dev;

    //click_chatter("CONFIGURE\n");
    if (!dev_fops)
	return errh->error("file operations missing");

    _max_burst = 0;
    if (cp_va_kparse(conf, this, errh,
		     "DEV_MINOR", cpkP+cpkM, cpUnsigned, &_dev_minor,
		     "CAPACITY", 0, cpUnsigned, &_capacity,
		     "BURST", 0, cpUnsigned, &_max_burst,
		     cpEnd) < 0)
        return -1;


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
    elem[_dev_minor] = this;
    init_waitqueue_head(&_proc_queue);
    click_chatter("Dev:%s Major: %d Minor: %d Size:%d\n", _devname.c_str(), _dev_major, _dev_minor, _size);
    return 0;
}

int ToUserDevice::initialize(ErrorHandler *errh)
{
    click_chatter("**ToUserDevice init\n");
    if (input_is_pull(0))
    {
        ScheduleInfo::initialize_task(this, &_task, errh);
        _signal = Notifier::upstream_empty_signal(this, 0, &_task);
    }
    return 0;
}

//cleanup
void ToUserDevice::cleanup(CleanupStage stage)
{
    //click_chatter("cleanup...");
    if (stage < CLEANUP_CONFIGURED)
        return; // have to quit, as configure was never called
    spin_lock(&_lock); // LOCK
    DEV_NUM--;
    _exit = true; // signal for exit
    spin_unlock(&_lock); // UNLOCK
    if (_q) {
        //click_chatter(" Start ");
        wake_up_interruptible(&_proc_queue);
	while (waitqueue_active(&_proc_queue))
	    schedule();

        // I guess after this, god knows what will happen to procs that are
        // blocked
        if (!DEV_NUM)
            unregister_chrdev(_dev_major, DEV_NAME);
        // now clear out the memory
	while (_size > 0) {
	    _q[_r_slot]->kill();
	    _r_slot++;
	    if (_r_slot == _capacity)
		_r_slot = 0;
	    _size--;
	}
        click_lfree((char*)_q, _capacity * sizeof(Packet *));
    }
    //click_chatter("cleanup Done. ");
}

bool ToUserDevice::process(Packet *p)
{
    spin_lock(&_lock);
    _pkt_count++;
    if (_size >= _capacity) {
	_drop_count++;
	p->kill();
	spin_unlock(&_lock);
	return false;
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

void ToUserDevice::push(int, Packet *p)
{
    if (p)
	process(p);
}

// something is upstream so we can run
bool ToUserDevice::run_task(Task *)
{
    Packet *p = input(0).pull();
    int     res;

    if (p)
        process(p);
    else if (!_signal)
        return false;
    _task.fast_reschedule();
    return p != 0;
}

enum { H_COUNT, H_DROPS, H_READ_CALLS, H_CAPACITY,
       H_READ_COUNT, H_SIZE, H_BLOCKS, H_FAILED};

String ToUserDevice::read_handler(Element *e, void *thunk)
{
    ToUserDevice *c = (ToUserDevice *)e;

    switch ((intptr_t)thunk)
    {
        case H_COUNT:      return String(c->_pkt_count);
        case H_FAILED:      return String(c->_failed_count);
        case H_BLOCKS:      return String(c->_block_count);
        case H_SIZE:       return String(c->_size);
        case H_DROPS:      return String(c->_drop_count);
        case H_READ_CALLS: return String(c->_read_count);
        case H_READ_COUNT: return String(c->_pkt_read_count);
        case H_CAPACITY:   return String(c->_capacity);
        default:           return "<error>";
    }
}

void ToUserDevice::add_handlers()
{
    add_read_handler("count", read_handler, (void *)H_COUNT);
    add_read_handler("failed", read_handler, (void *)H_FAILED);
    add_read_handler("blocks", read_handler, (void *)H_BLOCKS);
    add_read_handler("size", read_handler, (void *)H_SIZE);
    add_read_handler("drops", read_handler, (void *)H_DROPS);
    add_read_handler("reads", read_handler, (void *)H_READ_CALLS);
    add_read_handler("pkt_reads", read_handler, (void *)H_READ_COUNT);
    add_read_handler("capacity", read_handler, (void *)H_CAPACITY);
    if (input_is_pull(0))
        add_task_handlers(&_task);
}


ELEMENT_REQUIRES(linuxmodule experimental)
EXPORT_ELEMENT(ToUserDevice)
ELEMENT_MT_SAFE(ToUserDevice)
