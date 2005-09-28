/*
 * todevice.{cc,hh} -- element writes packets to network via pcap library
 * Douglas S. J. De Couto, Eddie Kohler, John Jannotti
 *
 * Copyright (c) 1999-2000 Massachusetts Institute of Technology
 * Copyright (c) 2005 Regents of the University of California
 *
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files (the "Software"),
 * to deal in the Software without restriction, subject to the conditions
 * listed in the Click LICENSE file. These conditions include: you must
 * preserve this copyright notice, and you cannot mention the copyright
 * holders in advertising related to the Software without their permission.
 * The Software is provided WITHOUT ANY WARRANTY, EXPRESS OR IMPLIED. This
 * notice is a summary of the Click LICENSE file; the license in that file is
 * legally binding.
 */

#include <click/config.h>
#if HAVE_NET_BPF_H
# include <sys/types.h>
# include <sys/time.h>
# include <net/bpf.h>
# define PCAP_DONT_INCLUDE_PCAP_BPF_H 1
#endif
#include "todevice.hh"
#include <click/error.hh>
#include <click/etheraddress.hh>
#include <click/confparse.hh>
#include <click/router.hh>
#include <click/standard/scheduleinfo.hh>
#include <click/packet_anno.hh>
#include <click/straccum.hh>
#include <stdio.h>
#include <unistd.h>

#if TODEVICE_BSD_DEV_BPF
# include <fcntl.h>
# include <sys/types.h>
# include <sys/socket.h>
# include <sys/ioctl.h>
# include <net/if.h>
#elif TODEVICE_LINUX
# include <sys/socket.h>
# include <sys/ioctl.h>
# include <net/if.h>
# include <net/if_packet.h>
# include <features.h>
# if __GLIBC__ >= 2 && __GLIBC_MINOR__ >= 1
#  include <netpacket/packet.h>
# else
#  include <linux/if_packet.h>
# endif
#endif

CLICK_DECLS

ToDevice::ToDevice()
  : _task(this), _timer(this), _fd(-1), _my_fd(false),
    _q(0),
    _pulls(0)
{
}

ToDevice::~ToDevice()
{
}

int
ToDevice::configure(Vector<String> &conf, ErrorHandler *errh)
{
  
  if (cp_va_parse(conf, this, errh,
		  cpString, "interface name", &_ifname,
		  cpKeywords,
		  "DEBUG", cpBool, "debug", &_debug,
		  cpEnd) < 0)
    return -1;
  if (!_ifname)
    return errh->error("interface not set");
  return 0;
}

int
ToDevice::initialize(ErrorHandler *errh)
{
  _timer.initialize(this);
  _fd = -1;

#if TODEVICE_BSD_DEV_BPF
  
  /* pcap_open_live() doesn't open for writing. */
  for (int i = 0; i < 16 && _fd < 0; i++) {
    char tmp[64];
    sprintf(tmp, "/dev/bpf%d", i);
    _fd = open(tmp, 1);
  }
  if (_fd < 0)
    return(errh->error("open /dev/bpf* for write: %s", strerror(errno)));

  struct ifreq ifr;
  strncpy(ifr.ifr_name, _ifname.c_str(), sizeof(ifr.ifr_name));
  ifr.ifr_name[sizeof(ifr.ifr_name) - 1] = 0;
  if (ioctl(_fd, BIOCSETIF, (caddr_t)&ifr) < 0)
    return errh->error("BIOCSETIF %s failed", ifr.ifr_name);
# ifdef BIOCSHDRCMPLT
  int yes = 1;
  if (ioctl(_fd, BIOCSHDRCMPLT, (caddr_t)&yes) < 0)
      errh->warning("BIOCSHDRCMPLT %s failed", ifr.ifr_name);
# endif
  _my_fd = true;

#elif TODEVICE_LINUX || TODEVICE_PCAP
  
  // find a FromDevice and reuse its socket if possible
  for (int ei = 0; ei < router()->nelements() && _fd < 0; ei++) {
    Element *e = router()->element(ei);
    FromDevice *fdev = (FromDevice *)e->cast("FromDevice");
    if (fdev && fdev->ifname() == _ifname && fdev->fd() >= 0) {
      _fd = fdev->fd();
      _my_fd = false;
    }
  }
  if (_fd < 0) {
# if TODEVICE_LINUX
    _fd = FromDevice::open_packet_socket(_ifname, errh);
    _my_fd = true;
# else
    return errh->error("ToDevice requires an initialized FromDevice on this platform") ;
# endif
  }
  if (_fd < 0)
    return -1;
  
#else
  
  return errh->error("ToDevice is not supported on this platform");
  
#endif

  // check for duplicate writers
  void *&used = router()->force_attachment("device_writer_" + _ifname);
  if (used)
    return errh->error("duplicate writer for device `%s'", _ifname.c_str());
  used = this;

  ScheduleInfo::join_scheduler(this, &_task, errh);
  _signal = Notifier::upstream_empty_signal(this, 0, &_task);
  return 0;
}

void
ToDevice::cleanup(CleanupStage)
{
  if (_fd >= 0 && _my_fd)
    close(_fd);
  _fd = -1;
}



/*
 * Linux select marks datagram fd's as writeable when the socket
 * buffer has enough space to do a send (sock_writeable() in
 * sock.h). BSD select always marks datagram fd's as writeable
 * (bpf_poll() in sys/net/bpf.c) This function should behave
 * appropriately under both.  It makes use of select if it correctly
 * tells us when buffers are available, and it schedules a backoff
 * timer if buffers are not available.  
 * --jbicket
 */
void
ToDevice::selected(int) 
{
  Packet *p;
  if (_q) {
    p = _q;
    _q = 0;
  } else {
    p = input(0).pull();
    _pulls++;
  }
  if (p) {
    int retval;
    const char *syscall;
    
#if TODEVICE_WRITE
    retval = ((uint32_t) write(_fd, p->data(), p->length()) == p->length() ? 0 : -1);
    syscall = "write";
#elif TODEVICE_SEND
    retval = send(_fd, p->data(), p->length(), 0);
    syscall = "send";
#else
    retval = 0;
#endif
    
    if (retval < 0) {
      if (errno == ENOBUFS || errno == EAGAIN) {
	assert(!_q);
	_q = p;
	/* we should backoff */
	remove_select(_fd, SELECT_WRITE);

	_backoff = (!_backoff) ? 1 : _backoff*2;
	_timer.schedule_after(Timestamp::make_usec(_backoff));

	if (_debug) {
	    Timestamp now = Timestamp::now();
	    click_chatter("%{element} backing off for %d at %{timestamp}\n",
			  this, _backoff, &now);
	}
	return;
      } else {
	click_chatter("ToDevice(%s) %s: %s", _ifname.c_str(), syscall, strerror(errno));
	checked_output_push(1, p);
      }
    } else {
      _backoff = 0;
      checked_output_push(0, p);
    }
  }
  if (!_q && !p && !_signal) {
    if (remove_select(_fd, SELECT_WRITE) < 0) {
      click_chatter("%s %{element} remove_select failed %d\n", 
		    Timestamp::now().unparse().c_str(), this, _fd);
    }
  }
}

void
ToDevice::run_timer(Timer *)
{
  if (_debug) {
    click_chatter("%s %{element}::%s\n",
		  Timestamp::now().unparse().c_str(), this, __func__);

  }

  if (_q || _signal) {
    if (add_select(_fd, SELECT_WRITE) < 0) {
      click_chatter("%s %{element}::%s add_select failed %d\n", 
		    Timestamp::now().unparse().c_str(), this, __func__, _fd);
    }
    selected(_fd);
  }
}

bool
ToDevice::run_task()
{
  if (_q || _signal) {
    if (add_select(_fd, SELECT_WRITE) < 0) {
      click_chatter("%s %{element}::%s add_select failed %d\n", 
		    Timestamp::now().unparse().c_str(), this, __func__, _fd);
    }
    selected(_fd);
    return true;
  }
  return false;
}


enum {H_DEBUG, H_SIGNAL, H_PULLS, H_Q};

String
ToDevice::read_param(Element *e, void *thunk)
{
  ToDevice *td = (ToDevice *)e;
  switch((uintptr_t) thunk) {
  case H_DEBUG:
    return String(td->_debug) + "\n";
  case H_SIGNAL:
    return String(td->_signal) + "\n";
  case H_PULLS:
    return String(td->_pulls) + "\n";
  case H_Q:
    return String((bool) td->_q) + "\n";
  default:
    return String();
  }
}

int 
ToDevice::write_param(const String &in_s, Element *e, void *vparam,
		     ErrorHandler *errh)
{
  ToDevice *td = (ToDevice *)e;
  String s = cp_uncomment(in_s);
  switch((int)vparam) {
  case H_DEBUG: {
    bool debug;
    if (!cp_bool(s, &debug)) 
      return errh->error("debug parameter must be boolean");
    td->_debug = debug;
    break;
  }
  }
  return 0;
}
void
ToDevice::add_handlers()
{
  add_task_handlers(&_task);

  add_read_handler("debug", read_param, (void *) H_DEBUG);
  add_read_handler("pulls", read_param, (void *) H_PULLS);
  add_read_handler("signal", read_param, (void *) H_SIGNAL);
  add_read_handler("q", read_param, (void *) H_Q);

  add_write_handler("debug", write_param, (void *) H_DEBUG);

}

CLICK_ENDDECLS
ELEMENT_REQUIRES(FromDevice userlevel)
EXPORT_ELEMENT(ToDevice)
