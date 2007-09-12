/*

Programmer: Roman Chertov

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
*/

#include <click/config.h>
#include <click/router.hh>
#include <click/confparse.hh>
#include <click/error.hh>
#include <click/standard/scheduleinfo.hh>
#include <linux/module.h>
#include <linux/poll.h>
#include <linux/version.h>
#include <linux/fs.h>
#include <unistd.h>
#include<linux/mm.h>

#include <click/cxxprotect.h>
CLICK_CXX_PROTECT
#include <asm/types.h>
#include <asm/uaccess.h>
#include <linux/ip.h>
#include <linux/inetdevice.h>
#include <net/route.h>


CLICK_CXX_UNPROTECT
#include <click/cxxunprotect.h>

#include "fromuserdevice.hh"
#include <click/hashmap.hh>

// need to accomodate a MTU packet as well as the size of the packet in each slot

static char DEV_NAME[] = "toclick";
static int  DEV_MAJOR = 240;
static int  DEV_MINOR = 0;
static int  DEV_NUM = 0;

struct file_operations *FromUserDevice::dev_fops;


static void fl_wakeup(Timer *, void *);

static FromUserDevice *elem[20] = {0};

FromUserDevice::FromUserDevice()
{
    _size = 0;
    _capacity = CAPACITY;
    _devname = DEV_NAME;
    _slot_size = SLOT_SIZE; 
    _buff = 0;
    _r_slot = 0;
    _w_slot = 0;
    _dev_major = DEV_MAJOR;
    _dev_minor = DEV_MINOR;
    _write_count = 0;
    _drop_count = 0;
    _pkt_count = 0;
    _block_count = 0;
    _failed_count = 0;
    _exit = false;
}

FromUserDevice::~FromUserDevice()
{

}

extern struct file_operations *click_new_file_operations();

void FromUserDevice::static_initialize()
{
    if ((dev_fops = click_new_file_operations())) {
	dev_fops->write   = dev_write;
	dev_fops->poll    = dev_poll;
	dev_fops->open    = dev_open;
	dev_fops->release = dev_release;
    }
}

void FromUserDevice::static_cleanup()
{
    dev_fops = 0;		// proclikefs will free eventually
}

// open function - called when the "file" /dev/toclick is opened in userspace
int FromUserDevice::dev_open(struct inode *inode, struct file *filp)
{
    int num = MINOR(inode->i_rdev); // low order number

    click_chatter("FromUserDevice_open %d\n", num);
    if (!elem[num])
    {
        click_chatter("No FromUserDevice element for this device: %d\n", num);
        return -EIO;
    }
    filp->private_data = elem[num];
    return 0;
}

// close function - called when the "file" /dev/toclikc is closed in userspace  
int FromUserDevice::dev_release(struct inode *inode, struct file *filp)
{
    FromUserDevice *elem = (FromUserDevice*)filp->private_data;

    if (!elem)
    {
        click_chatter("Empty private struct!\n");
        return -EIO;
    }

    click_chatter("FromUserDevice_release WriteCounter: %lu\n", elem->_write_count);
    return 0;
}

ssize_t FromUserDevice::dev_write (struct file *filp, const char *buf,
                             size_t count, loff_t *ppos)
{
    FromUserDevice *elem = (FromUserDevice*)filp->private_data;
    int          err;
    struct slot *slot;
    // there is a private field and that doesn't compile in C++ so we can't use DEFINE_WAIT macro
    wait_queue_t wq;
#include <click/cxxprotect.h>
    init_wait(&wq);
#include <click/cxxunprotect.h>
    u_char      *temp_buff; 
    ulong        flags;

    //click_chatter("FromUserDevice_write %d\n", count);
    if (!elem)
    {
        click_chatter("Empty private struct!\n");
        return -EIO;
    }
    // we should make a copy_from_user here and not while we hold the spinlock
    temp_buff = (u_char*)click_lalloc(SLOT_SIZE);
    err = copy_from_user(temp_buff, buf, count);
    if (err != 0)
    {
        click_lfree(temp_buff, SLOT_SIZE);
        elem->_failed_count++;
        click_chatter("Write Fault");
        return -EFAULT;
    }

    spin_lock_irqsave(&elem->_lock, flags);
    // the incoming buffer is too big
    if (count > elem->_slot_size)
    {
        elem->_drop_count++;

        spin_unlock_irqrestore(&elem->_lock, flags);
        click_lfree(temp_buff, SLOT_SIZE);
        click_chatter("Incoming buffer is bigger than current slot size\n");
        return -EFAULT;
    }
    while(1)
    {
        if (elem->_exit)
        {
            spin_unlock_irqrestore(&elem->_lock, flags);
            click_lfree(temp_buff, SLOT_SIZE);
            return -EIO;
        }
        if (elem->_size >= elem->_capacity)
        {
            elem->_sleep_proc++;
            elem->_block_count++;
            spin_unlock_irqrestore(&elem->_lock, flags);
            // need to put the process to sleep
            //interruptible_sleep_on(&elem->_proc_queue);
            prepare_to_wait(&elem->_proc_queue, &wq, TASK_INTERRUPTIBLE);
            schedule();
            finish_wait(&elem->_proc_queue, &wq);
	    if (elem->_exit)
		return -EIO;
            spin_lock_irqsave(&elem->_lock, flags);
            elem->_sleep_proc--;
        }
        else
            break; // we are sure that size is not zero so continue now
    }
    slot = elem->_buff + elem->_w_slot;
    slot->size = count;
    // copy from our temp buff into the ring buffer
    memcpy(slot->buff, temp_buff, count);
    elem->_w_slot = (elem->_w_slot + 1) % elem->_capacity;
    elem->_size++;
    elem->_write_count++;

    //click_ip  *ip = (click_ip*)slot->buff;
    //click_chatter("%x %x %d %d", ip->ip_src.s_addr, ip->ip_dst.s_addr,
    //              ip->ip_v, ip->ip_hl);
    spin_unlock_irqrestore(&elem->_lock, flags);
    click_lfree(temp_buff, SLOT_SIZE);
    return count;
}


int FromUserDevice::configure(Vector<String> &conf, ErrorHandler *errh)
{
    int          res;
    dev_t        dev;

    if (!dev_fops)
	return errh->error("file operations missing");
    
    click_chatter("CONFIGURE\n");
    if (cp_va_kparse(conf, this, errh,
		     "DEV_MINOR", cpkP+cpkM, cpUnsigned, &_dev_minor,
		     "CAPACITY", 0, cpUnsigned, &_capacity,
		     cpEnd) < 0)
        return -1;

    spin_lock_init(&_lock);
    if (!(_buff = (struct slot*)click_lalloc(_capacity * sizeof(struct slot))))
    {
        click_chatter("FromUserDevice Failed to alloc %lu slots\n", _capacity);
        return -1;
    }

    _dev_major = DEV_MAJOR;
    DEV_NUM++;
    if (DEV_NUM == 1)
    {
        // time to associate the devname with this class
        // dynamically allocate the major number with the device
    
        //register the device now. this will register 255 minor numbers 
        res = register_chrdev(_dev_major, DEV_NAME, dev_fops);
        if (res < 0) 
        {
            click_chatter("Failed to Register Dev:%s Major:%d Minor:%d\n", 
                DEV_NAME, _dev_major, _dev_minor);
            click_lfree((char*)_buff, _capacity * sizeof(struct slot));
            _buff = 0;
            return - EIO;
        }
    }
    elem[_dev_minor] = this;
    init_waitqueue_head (&_proc_queue);
    click_chatter("Dev:%s Major: %d Minor: %d Size:%d\n", _devname.c_str(), _dev_major, _dev_minor, _size);
    return 0;
}

uint FromUserDevice::dev_poll(struct file *filp, struct poll_table_struct *pt)
{
    FromUserDevice *elem = (FromUserDevice*)filp->private_data;
    uint      mask = 0;
    ulong     flags;

    spin_lock_irqsave(&elem->_lock, flags);
    //click_chatter("FromUserDevice_poll\n");
    if (elem->_size == elem->_capacity)
        mask |= POLLOUT | POLLWRNORM; /* readable */
    spin_unlock_irqrestore(&elem->_lock, flags);
    return mask;
}

int FromUserDevice::initialize(ErrorHandler *errh)
{
    click_chatter("FromUserDevice init\n");

/*
    // don't schedule until we get configured
    ScheduleInfo::initialize_task(this, &_task, _buff != 0, errh);
#ifdef HAVE_STRIDE_SCHED
    // user specifies max number of tickets; we start with default
    _max_tickets = _task.tickets();
    _task.set_tickets(Task::DEFAULT_TICKETS);
#endif*/
    return 0;
}

Packet* FromUserDevice::pull(int)
{
    WritablePacket *p = 0;
    struct slot    *slot;
    ulong           flags;

    spin_lock_irqsave(&_lock, flags);
    click_ip       *ip;
    struct sk_buff *skb;

    if (_size)
    {
        slot = _buff + _r_slot;
        _r_slot = (_r_slot + 1) % _capacity;
        _size--;

        p = Packet::make(slot->size);

        memcpy(p->data(), slot->buff, slot->size);
        /*ip = (click_ip*)(p->data());
        p->timestamp_anno().set_now();
        p->set_dst_ip_anno(ip->ip_dst);
        p->set_ip_header(ip, sizeof(click_ip));*/
        // output the packet

        //click_chatter("%x %x %d %d", ip->ip_src.s_addr, ip->ip_dst.s_addr,
        //              ip->ip_v, ip->ip_hl);
        _pkt_count++;
        //click_chatter("Pkt Push: %d\n", slot->size);
    }
    spin_unlock_irqrestore(&_lock, flags);
    // wake up procs if any are sleeping
    wake_up_interruptible(&_proc_queue); 
    return p;
}

//cleanup
void FromUserDevice::cleanup(CleanupStage)
{
    ulong flags;
    
    click_chatter("cleanup\n");

    spin_lock_irqsave(&_lock, flags);
    DEV_NUM--;
    _exit = true;
    spin_unlock_irqrestore(&_lock, flags);

    if (_buff)
    {
        wake_up_interruptible(&_proc_queue);
	while (waitqueue_active(&_proc_queue))
	    schedule();
        // I guess after this, god knows what will happen to procs that are
        // blocked
        if (!DEV_NUM)
            unregister_chrdev(_dev_major, DEV_NAME);
        // now clear out the memory
        click_lfree((char*)_buff, _capacity * sizeof(struct slot));
    }  
}

enum { H_COUNT, H_DROPS, H_WRITE_CALLS, H_CAPACITY,
       H_SLOT_SIZE, H_SIZE, H_BLOCK, H_FAILED };

String FromUserDevice::read_handler(Element *e, void *thunk)
{
    FromUserDevice *c = (FromUserDevice *)e;

    switch ((intptr_t)thunk) 
    {
        case H_COUNT:       return String(c->_pkt_count);
        case H_FAILED:      return String(c->_failed_count);
        case H_SIZE:        return String(c->_size);
        case H_BLOCK:       return String(c->_block_count);
        case H_DROPS:       return String(c->_drop_count);
        case H_WRITE_CALLS: return String(c->_write_count);
        case H_CAPACITY:    return String(c->_capacity);
        case H_SLOT_SIZE:   return String(c->_slot_size);
        default:            return "<error>";
    }
}

void FromUserDevice::add_handlers()
{
    add_read_handler("count", read_handler, (void *)H_COUNT);
    add_read_handler("failed", read_handler, (void *)H_FAILED);
    add_read_handler("blocks", read_handler, (void *)H_BLOCK);
    add_read_handler("size", read_handler, (void *)H_SIZE);
    add_read_handler("drops", read_handler, (void *)H_DROPS);
    add_read_handler("writes", read_handler, (void *)H_WRITE_CALLS);
    add_read_handler("capacity", read_handler, (void *)H_CAPACITY);
    add_read_handler("slot_size", read_handler, (void *)H_SLOT_SIZE);
}



ELEMENT_REQUIRES(linuxmodule experimental)
EXPORT_ELEMENT(FromUserDevice)
ELEMENT_MT_SAFE(FromUserDevice)
