#ifndef CLICK_TOSIMDEVICE_HH
#define CLICK_TOSIMDEVICE_HH
#include <click/element.hh>
#include <click/string.hh>
#include <click/task.hh>
#include <click/notifier.hh>
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
 * =title ToSimDevice.u
 * =c
 * ToSimDevice(DEVNAME [, ENCAP])
 * =s netdevices
 * sends packets to simulated network device
 * =d
 *
 * This manual page describes the ToSimDevice element.
 *
 * This element will only work with the click simulator interface.
 *
 * Keyword arguments are:
 *
 * =over 8
 *
 * =item ENCAP
 *
 * Word.  The interface's encapsulation type.  Options are ETHER, IP, and
 * UNKNOWN; default is ETHER.
 *
 * =back
 *
 * =a
 * FromSimDevice.u */


/*
 * Write packets to a simulated network interface.
 * Expects packets that already have an ether header.
 * Can push or pull.
 */

class ToSimDevice : public Element { public:

  ToSimDevice() CLICK_COLD;
  ~ToSimDevice() CLICK_COLD;

  const char *class_name() const		{ return "ToSimDevice"; }
  const char *port_count() const		{ return PORTS_1_0; }
  //const char *flags() const			{ return "S2"; }

  int configure_phase() const { return CONFIGURE_PHASE_DEFAULT; }
  int configure(Vector<String> &, ErrorHandler *) CLICK_COLD;
  int initialize(ErrorHandler *) CLICK_COLD;
  void uninitialize();
  void add_handlers() CLICK_COLD;

  String ifname() const				{ return _ifname; }
  int fd() const				{ return _fd; }

  void push(int port, Packet *);
  bool run_task(Task *);

  private:

    String _ifname;
    int _fd;
    bool _my_fd;
    Task _task;
    int _encap_type;
    NotifierSignal _signal;

    void send_packet(Packet *);

};

CLICK_ENDDECLS
#endif
