#ifndef CLICK_TODEVICE_HH
#define CLICK_TODEVICE_HH
#include <click/element.hh>
#include <click/string.hh>
#include <click/task.hh>
#include <click/notifier.hh>
#include "elements/userlevel/fromdevice.hh"
CLICK_DECLS

/*
 * =title ToDevice.u
 * =c
 * ToDevice(DEVNAME [, I<KEYWORDS>])
 * =s devices
 * sends packets to network device (user-level)
 * =d
 *
 * This manual page describes the user-level version of the ToDevice element.
 * For the Linux kernel module element, read the ToDevice(n) manual page.
 *
 * Pulls packets and sends them out the named device using
 * Berkeley Packet Filters (or Linux equivalent).
 *
 * Keyword arguments are:
 *
 * =over 8
 *
 * =item SET_ERROR_ANNO
 * 
 * Boolean.  If true, then set the SEND_ERR annotation on error output
 * packets.  Default is false.
 *
 * =item IGNORE_QUEUE_OVERFLOWS
 *
 * Boolean.  If true, don't print more than one error message when
 * there are queue overflows error when sending packets to the device
 * (e.g. send() or write() produced an ENOBUFS or EAGAIN error).
 * Default is false.
 *
 * =back
 *
 * Packets sent via ToDevice should already have a link-level
 * header prepended. This means that ARP processing,
 * for example, must already have been done.
 *
 * Under Linux, a L<FromDevice(n)> element will not receive packets sent by a
 * ToDevice element for the same device. Under other operating systems, your
 * mileage may vary.
 *
 * If there is an error write()ing or send()ing a packet to the
 * device, the packet will be pushed out the (optional) output.  If
 * the SET_ERROR_ANNO keyword is true, the SEND_ERR annotation of the
 * pushed packet is set to the system error code.
 *
 * This element is only available at user level.
 * 
 * =a
 * FromDevice.u, FromDump, ToDump, ToDevice(n) */

#if defined(__linux__)
# define TODEVICE_LINUX 1
# define TODEVICE_SEND 1
#elif defined(HAVE_PCAP)
extern "C" {
# include <pcap.h>
}
# if defined(__FreeBSD__) || defined(__OpenBSD__)
#  define TODEVICE_BSD_DEV_BPF 1
#  define TODEVICE_WRITE 1
# elif defined(__sun)
#  define TODEVICE_PCAP 1
#  define TODEVICE_WRITE 1
# endif
#endif

/*
 * Write packets to the ethernet via the bpf.
 * Expects packets that already have an ether header.
 * Can push or pull.
 */

class ToDevice : public Element { public:
  
  ToDevice();
  ~ToDevice();
  
  const char *class_name() const		{ return "ToDevice"; }
  const char *processing() const		{ return AGNOSTIC; }
  const char *flags() const			{ return "S2"; }
  
  ToDevice *clone() const;
  int configure_phase() const { return FromDevice::CONFIGURE_PHASE_TODEVICE; }
  int configure(Vector<String> &, ErrorHandler *);
  int initialize(ErrorHandler *);
  void cleanup(CleanupStage);
  void add_handlers();

  void notify_noutputs(int);

  String ifname() const				{ return _ifname; }
  int fd() const				{ return _fd; }

  void push(int port, Packet *);
  void run_scheduled();

protected:
  Task _task;
  void send_packet(Packet *);

private:

  String _ifname;
  int _fd;
  bool _my_fd;
  NotifierSignal _signal;
  
  bool _set_error_anno;
  bool _ignore_q_errs;
  bool _printed_err;
};

CLICK_ENDDECLS
#endif
