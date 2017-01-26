#ifndef CLICK_FROMSIMDEVICE_HH
#define CLICK_FROMSIMDEVICE_HH
#include <click/element.hh>
#include <click/simclick.h>
CLICK_DECLS

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

/*
 * =title FromSimDevice.u
 * =c
 * FromSimDevice(DEVNAME [, PROMISC? [, SNAPLEN]])
 * =s netdevices
 * reads packets from a simulator device
 * =d
 *
 * This manual page describes the user-level version of the FromSimDevice
 * element.
 *
 * =e
 *   FromSimDevice(eth0, 2048) -> ...
 *
 * =a ToSimDevice.u, FromDump, ToDump, FromDevice(n) */

class FromSimDevice : public Element {

  String _ifname;
  int _packetbuf_size;
  int _headroom;
  int _fd;
  unsigned char *_packetbuf;
  bool _promisc;

  static void set_annotations(Packet *,int ptype);
  // set appropriate annotations, i.e. MAC packet type.
  // modifies the packet.

 public:

  FromSimDevice() CLICK_COLD;
  ~FromSimDevice() CLICK_COLD;

  const char *class_name() const	{ return "FromSimDevice"; }
  const char *port_count() const	{ return PORTS_0_1; }
  const char *processing() const	{ return PUSH; }

  int configure_phase() const		{ return CONFIGURE_PHASE_DEFAULT; }
  int configure(Vector<String> &, ErrorHandler *) CLICK_COLD;
  int initialize(ErrorHandler *) CLICK_COLD;
  void uninitialize();

  String ifname() const			{ return _ifname; }
  int fd() const			{ return _fd; }
  int incoming_packet(int ifid,int ptype,const unsigned char* data,int len,
		      simclick_simpacketinfo* pinfo);
};

CLICK_ENDDECLS
#endif
