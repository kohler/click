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
  void add_handlers();
  
  void run_scheduled();
  
  bool tx_intr();

#if DEV_KEEP_STATS
  // Statistics.
  unsigned long long _idle_calls;
  unsigned long long _idle_pulls;
  unsigned long long _busy_returns;
  unsigned long long _pkts_sent;    
  unsigned long long _time_pull;
  unsigned long long _time_clean;
  unsigned long long _time_queue;
  unsigned long long _perfcnt1_pull;
  unsigned long long _perfcnt1_clean;
  unsigned long long _perfcnt1_queue;
  unsigned long long _perfcnt2_pull;
  unsigned long long _perfcnt2_clean;
  unsigned long long _perfcnt2_queue;
#endif
  unsigned long _rejected;
  unsigned long _hard_start;
  unsigned long _activations; 
  
 private:

  int queue_packet(Packet *p);

  String _devname;
  struct device *_dev;
  int _registered;
  int _dev_idle;
  int _last_dma_length;
  int _last_tx;
  int _last_busy;
};

#endif
