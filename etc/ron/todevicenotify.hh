#ifndef TODEVICENOTIFY_HH
#define TODEVICENOTIFY_HH
#include <click/element.hh>
#include <click/string.hh>
#include <click/task.hh>
#include "queuenotify.hh"
#include "../../elements/userlevel/fromdevice.hh"
#include "../../elements/userlevel/todevice.hh"
/*
 * =title ToDeviceNotify.u
 * =c
 * ToDeviceNotify(DEVNAME)
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
 * Packets sent via ToDevice should already have a link-level
 * header prepended. This means that ARP processing,
 * for example, must already have been done.
 *
 * Under Linux, a L<FromDevice(n)> element will not receive packets sent by a
 * ToDevice element for the same device. Under other operating systems, your
 * mileage may vary.
 *
 * This element is only available at user level.
 *
 * =a
 * ToDevice, FromDevice.u, FromDump, ToDump, ToDevice(n) */

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
# endif
#endif

/*
 * Write packets to the ethernet via the bpf.
 * Expects packets that already have an ether header.
 * Can push or pull.
 */

class ToDeviceNotify : public ToDevice, public NotifiedElement { public:
  ToDeviceNotify();
  ~ToDeviceNotify();

  const char *class_name() const		{ return "ToDeviceNotify"; }
  int initialize(ErrorHandler *);
  bool run_task(Task *);
  void notify(int signal);

protected:
  bool _data_ready;

};
#endif
