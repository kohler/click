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

#define TODEV_IDLE_LIMIT 32
#define TODEV_MAX_PKTS_PER_RUN 32


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
  int configure(const String &, ErrorHandler *);
  int initialize(ErrorHandler *);
  void uninitialize();
  void add_handlers(HandlerRegistry *);
  
  bool wants_packet_upstream() const;
  void run_scheduled();
  
  void push(int port, Packet *);
  
  bool tx_intr();

  // Statistics.
  int _idle_calls;   // # of times called because driver was idle.
  int _busy_returns; // # of times returned because dev->tbusy.
  int _rejected;     // # of packets rejected by hard_start_xmit().
  int _pkts_sent;    // # number of packet sent
  int _activations;  // # number of activations
  
 private:

  String _devname;
  struct device *_dev;
  int _registered;
  int _idle; 	     // # of times pull didn't get a packet
};

#endif
