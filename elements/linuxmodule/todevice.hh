#ifndef TODEVICE_HH
#define TODEVICE_HH

/*
 * =c
 * ToDevice(DEVNAME)
 * =s
 * sends packets to network device (kernel)
 * V<devices>
 * =d
 *
 * This manual page describes the Linux kernel module version of the ToDevice
 * element. For the user-level element, read the ToDevice.u manual page.
 *
 * Sends packets out the Linux network interface named DEVNAME.
 *
 * Packets must have a link header. For ethernet, ToDevice
 * makes sure every packet is at least 60 bytes long.
 *
 * =n
 * The Linux networking code may also send packets out the device. Click won't
 * see those packets. Worse, Linux may cause the device to be busy when a
 * ToDevice wants to send a packet. Click is not clever enough to re-queue
 * such packets, and discards them. 
 *
 * ToDevice interacts with Linux in two ways: when Click is running in polling
 * mode, or when Click is running in interrupt mode. In both of these cases,
 * we depend on the net driver's send operation for synchronization (e.g.
 * tulip send operation uses a bit lock).
 *
 * =a FromDevice, PollDevice, FromLinux, ToLinux, ToDevice.u */

#include "anydevice.hh"
#include "fromlinux.hh"

class ToDevice : public AnyDevice {
  
 public:
  
  ToDevice();
  ~ToDevice();
  
  const char *class_name() const	{ return "ToDevice"; }
  const char *processing() const	{ return PULL; }
  ToDevice *clone() const		{ return new ToDevice; }
  
  int configure_phase() const	{ return FromLinux::TODEVICE_CONFIGURE_PHASE; }
  int configure(const Vector<String> &, ErrorHandler *);
  int initialize(ErrorHandler *);
  void uninitialize();
  void add_handlers();
  
  void run_scheduled();
  
  bool tx_intr();

#if CLICK_DEVICE_STATS
  // Statistics.
  unsigned long long _idle_calls;
  unsigned long long _idle_pulls;
  unsigned long long _linux_pkts_sent;
  unsigned long long _time_pull;
  unsigned long long _time_clean;
  unsigned long long _time_queue;
  unsigned long long _perfcnt1_pull;
  unsigned long long _perfcnt1_clean;
  unsigned long long _perfcnt1_queue;
  unsigned long long _perfcnt2_pull;
  unsigned long long _perfcnt2_clean;
  unsigned long long _perfcnt2_queue;
  unsigned long _activations; 
#endif
  unsigned _npackets;
#if CLICK_DEVICE_THESIS_STATS
  unsigned long long _pull_cycles;
#endif
  unsigned long _rejected;
  unsigned long _hard_start;
  unsigned long _busy_returns;

  bool polling() const			{ return _polling; }
  
 private:

  bool _registered;
  bool _polling;
  int _dev_idle;
  int _last_tx;
  int _last_busy;
  
  int queue_packet(Packet *p);
  
};

#endif
