/*
 * tosimdevice.{cc,hh} -- writes packets to simulated network devices
 *
 */

/*****************************************************************************
 *  Copyright 2002, Univerity of Colorado at Boulder.                        *
 *                                                                           *
 *                        All Rights Reserved                                *
 *                                                                           *
 *  Permission to use, copy, modify, and distribute this software and its    *
 *  documentation for any purpose other than its incorporation into a        *
 *  commercial product is hereby granted without fee, provided that the      *
 *  above copyright notice appear in all copies and that both that           *
 *  copyright notice and this permission notice appear in supporting         *
 *  documentation, and that the name of the University not be used in        *
 *  advertising or publicity pertaining to distribution of the software      *
 *  without specific, written prior permission.                              *
 *                                                                           *
 *  UNIVERSITY OF COLORADO DISCLAIMS ALL WARRANTIES WITH REGARD TO THIS      *
 *  SOFTWARE, INCLUDING ALL IMPLIED WARRANTIES OF MERCHANTABILITY AND        *
 *  FITNESS FOR ANY PARTICULAR PURPOSE.  IN NO EVENT SHALL THE UNIVERSITY    *
 *  OF COLORADO BE LIABLE FOR ANY SPECIAL, INDIRECT OR CONSEQUENTIAL         *
 *  DAMAGES OR ANY DAMAGES WHATSOEVER RESULTING FROM LOSS OF USE, DATA       *
 *  OR PROFITS, WHETHER IN AN ACTION OF CONTRACT, NEGLIGENCE OR OTHER        *
 *  TORTIOUS ACTION, ARISING OUT OF OR IN CONNECTION WITH THE USE OR         *
 *  PERFORMANCE OF THIS SOFTWARE.                                            *
 *                                                                           *
 ****************************************************************************/


#include <click/config.h>
#include <click/package.hh>
#include "fromsimdevice.hh"
#include "tosimdevice.hh"
#include <click/error.hh>
#include <click/etheraddress.hh>
#include <click/confparse.hh>
#include <click/router.hh>
#include <click/standard/scheduleinfo.hh>

#include <cstdio>
#include <cassert>
#include <unistd.h>
CLICK_DECLS

ToSimDevice::ToSimDevice()
  : Element(1, 0), _fd(-1), _my_fd(false), _task(this), _encap_type(SIMCLICK_PTYPE_ETHER)
{
  MOD_INC_USE_COUNT;
}

ToSimDevice::~ToSimDevice()
{
  MOD_DEC_USE_COUNT;
  uninitialize();
}

ToSimDevice *
ToSimDevice::clone() const
{
  return new ToSimDevice;
}

int
ToSimDevice::configure(Vector<String> &conf, ErrorHandler *errh)
{
  String encap_type;
  if (cp_va_parse(conf, this, errh,
		  cpString, "interface name", &_ifname,
		  cpOptional,
		  cpWord, "encapsulation type",&encap_type,
		  0) < 0)
    return -1;
  if (!_ifname)
    return errh->error("interface not set");
  if (!encap_type || encap_type == "ETHER")
    _encap_type = SIMCLICK_PTYPE_ETHER;
  else if (encap_type == "IP")
    _encap_type = SIMCLICK_PTYPE_IP;
  else
    return errh->error("bad encapsulation type, expected `ETHER' or `IP'");

  return 0;
}

int
ToSimDevice::initialize(ErrorHandler *errh)
{
  _fd = -1;
  if (!_ifname)
    return errh->error("interface not set");

  // Get the simulator ifid
  Router* myrouter = router();
  _fd = myrouter->sim_get_ifid(_ifname.cc());
  if (_fd < 0) return -1;

  _my_fd = true;
  if (input_is_pull(0))
    ScheduleInfo::join_scheduler(this, &_task, errh);
  return 0;
}

void
ToSimDevice::uninitialize()
{
  _task.unschedule();
}

void
ToSimDevice::send_packet(Packet *p)
{
  Router* myrouter = router();
  int retval;
  // We send out either ethernet or IP
  retval = myrouter->sim_write(_fd,_encap_type,p->data(),p->length(),
				 p->get_sim_packetinfo());
  p->kill();
}

void
ToSimDevice::push(int, Packet *p)
{
  assert(p->length() >= 14);
  //fprintf(stderr,"Hey!!! Pushing!!!\n");
  send_packet(p);
}

bool
ToSimDevice::run_task()
{
  // XXX reduce tickets when idle
  bool active = false;
  if (router()->sim_if_ready(_fd)) {
    //fprintf(stderr,"Hey!!! Pulling ready!!!\n");
    if (Packet *p = input(0).pull()) {
      //fprintf(stderr,"Hey!!! Sending a packet!!!\n");
      send_packet(p);
      active = true;
    }
  }

  _task.fast_reschedule();
  return active;
}

void
ToSimDevice::add_handlers()
{
  if (input_is_pull(0))
    add_task_handlers(&_task);
}

CLICK_ENDDECLS
ELEMENT_REQUIRES(FromSimDevice ns)
EXPORT_ELEMENT(ToSimDevice)
