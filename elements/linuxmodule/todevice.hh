#ifndef TODEVICE_HH
#define TODEVICE_HH
#include "element.hh"
#include "string.hh"

/*
 * =c
 * ToDevice(devname)
 * =d
 * Sends packets out a Linux network interface device.
 *
 * Packets must have a link header. For ethernet, ToDevice
 * makes sure every packet is at least 60 bytes long.
 *
 * =n
 * The Linux networking code may also send packets out
 * the device. Click won't see those packets. Worse,
 * Linux may cause the device to be busy when
 * a ToDevice wants to send a packet.
 * Click is not clever enough to re-queue such packets,
 * and discards them.
 *
 * This element is only available inside the kernel module.
 *
 * =a FromDevice
 * =a ToLinux
 */

class ToDevice : public Element {
  
 public:
  
  ToDevice();
  ToDevice(const String &);
  ~ToDevice();
  static void static_initialize();
  static void static_cleanup();
  
  const char *class_name() const		{ return "ToDevice"; }
  Processing default_processing() const	{ return AGNOSTIC; }
  
  ToDevice *clone() const;
  int configure(const String &, Router *, ErrorHandler *);
  int initialize(Router *, ErrorHandler *);
  void uninitialize(Router *);
  void add_handlers(HandlerRegistry *);
  
  bool wants_packet_upstream() const;
  void run_scheduled();
  
  void push(int port, Packet *);
  
  int tx_intr();

  // Statistics.
  int _pull_calls;   // # of times please_pull() called.
  int _idle_calls;   // # of times called because driver was idle.
  int _drain_returns;// # of times returned because queue was empty.
  int _busy_returns; // # of times returned because dev->tbusy.
  int _rejected;     // # of packets rejected by hard_start_xmit().
  
 private:
  
  String _devname;
  struct device *_dev;
  int _registered;
  
};

#endif
