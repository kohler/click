#ifndef TODEVICE_HH
#define TODEVICE_HH

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

#include "element.hh"
#include "string.hh"
#include "netdev.h"

class ToDevice : public Element {
  
 public:
  
  ToDevice();
  ToDevice(const String &);
  ~ToDevice();
  static void static_initialize();
  static void static_cleanup();
  
  const char *class_name() const		{ return "ToDevice"; }
  Processing default_processing() const	{ return PULL; }
  
  ToDevice *clone() const;
  int configure(const String &, ErrorHandler *);
  int initialize(ErrorHandler *);
  void uninitialize();
  void add_handlers(HandlerRegistry *);
  
  void run_scheduled();
  
  bool tx_intr();

  // Statistics.
  int _idle_calls;   // # of times called because driver was idle.
  int _busy_returns; // # of times returned because dev->tbusy.
  int _hard_start;   // # of hard xmit starts
  unsigned long long _activations;  // # number of activations
  unsigned long long _pkts_on_dma;  // # number of packet left on tx ring
  unsigned long long _pkts_sent;    // # number of packet sent
  unsigned long long _tks_allocated;

  unsigned long long _dma_full_resched;
  unsigned long long _q_burst_resched;
  unsigned long long _q_full_resched;
  unsigned long long _q_empty_resched;
  
 private:

  void queue_packet(Packet *p);

  String _devname;
  struct device *_dev;
  int _registered;
  int _dev_idle;
  int _last_dma_length;
  int _last_tx;
  int _last_busy;
};

#endif
