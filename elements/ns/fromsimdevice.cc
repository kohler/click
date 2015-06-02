/*
 * fromsimevice.{cc,hh} -- element reads packets from a simulated network
 * interface.
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
#include <sys/time.h>
#include <unistd.h>
#include <click/simclick.h>
#include "fromsimdevice.hh"
#include "tosimdevice.hh"
#include <click/error.hh>
#include <click/args.hh>
#include <click/glue.hh>
#include <unistd.h>
#include <fcntl.h>
#include <click/router.hh>

#include <sys/ioctl.h>
CLICK_DECLS

FromSimDevice::FromSimDevice()
  : _packetbuf_size(0),_packetbuf(0)
{

}

FromSimDevice::~FromSimDevice()
{
  uninitialize();
}

int
FromSimDevice::configure(Vector<String> &conf, ErrorHandler *errh)
{
  _packetbuf_size = 2048;
  _promisc = false;
  _headroom = Packet::default_headroom;
  _headroom += (4 - (_headroom + 2) % 4) % 4; // default 4/2 alignment
  if (Args(conf, this, errh)
      .read_mp("DEVNAME", _ifname)
      .read_p("PROMISC", _promisc)
      .read_p("SNAPLEN", _packetbuf_size)
      .read("HEADROOM", _headroom)
      .complete() < 0)
    return -1;
  if (_packetbuf_size > 8192 || _packetbuf_size < 128)
    return errh->error("maximum packet length out of range");
  return 0;
}

int
FromSimDevice::initialize(ErrorHandler *errh)
{
  if (!_ifname)
    return errh->error("interface not set");

  // Get the simulator ifid
  Router* myrouter = router();
  _fd = myrouter->sim_get_ifid(_ifname.c_str());
  if (_fd < 0) return errh->error("unable to open netowrk interface");
  // create packet buffer
  _packetbuf = new unsigned char[_packetbuf_size];
  if (!_packetbuf) {
    close(_fd);
    return errh->error("out of memory");
  }

  // Request that we get packets sent to us from the simulator
  myrouter->sim_listen(_fd,eindex());

  // Set the promisc mode on the interface
  if (_promisc) {
    myrouter->sim_if_promisc(_fd);
  }
  return 0;
}

void
FromSimDevice::uninitialize()
{
  if (_fd >= 0) {
    //close(_fd);
    //remove_select(_fd, SELECT_READ);
    _fd = -1;
  }
  if (_packetbuf) {
    delete[] _packetbuf;
    _packetbuf = 0;
  }
}

void
FromSimDevice::set_annotations(Packet *p,int ptype)
{
  static uint8_t bcast_addr[] = { 0xff, 0xff, 0xff, 0xff, 0xff, 0xff };

  if (SIMCLICK_PTYPE_ETHER == ptype) {
    // check if multicast
    // ! mcast => ! bcast
    if (!(p->data()[0] & 1))
      return;

    // check for bcast
    if (memcmp(bcast_addr, p->data(), 6) == 0)
      p->set_packet_type_anno(Packet::BROADCAST);
    else
      p->set_packet_type_anno(Packet::MULTICAST);
  }

  return;
}

int
FromSimDevice::incoming_packet(int ifid,int ptype,const unsigned char* data,
			       int len,simclick_simpacketinfo* pinfo){
  int result = 0;
  (void) ifid;

  Packet *p = Packet::make(_headroom, data, len, 0);
  set_annotations(p,ptype);
  p->set_sim_packetinfo(pinfo);
  p->timestamp_anno().assign_now();
  output(0).push(p);

  return result;
}

CLICK_ENDDECLS
ELEMENT_REQUIRES(ns)
EXPORT_ELEMENT(FromSimDevice)
