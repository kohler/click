#ifndef TULIPSTATS_HH
#define TULIPSTATS_HH

/*
 * =c
 * TulipStats(DEVNAME)
 * =s
 * =a FromDevice, PollDevice, FromLinux, ToLinux, ToDevice.u */

#include "elements/linuxmodule/anydevice.hh"
#include "elements/linuxmodule/fromlinux.hh"

class TulipStats : public AnyDevice {
  
 public:
  
  TulipStats();
  ~TulipStats();
  
  const char *class_name() const	{ return "TulipStats"; }
  const char *processing() const	{ return PULL; }
  TulipStats *clone() const		{ return new TulipStats; }
  
  int configure_phase() const	{ return FromLinux::TODEVICE_CONFIGURE_PHASE; }
  int configure(const Vector<String> &, ErrorHandler *);
  int initialize(ErrorHandler *);
  void uninitialize();
  void add_handlers();

  void reset_counts();
  
  void run_scheduled();
  
 private:

  unsigned long _nstats_polls;
  unsigned long _ts[8];
  unsigned long _rs[8];
  unsigned long _eb[8];

  unsigned long _nintr;
  unsigned long _intr[17];

  struct net_device_stats *_dev_stats;
  unsigned long _mfo;
  unsigned long _oco;
  unsigned long _base_rx_missed_errors;
  unsigned long _base_rx_fifo_errors;
  unsigned long _base_tx_errors;
  unsigned long _base_tx_aborted_errors;
  unsigned long _base_tx_carrier_errors;
  unsigned long _base_tx_window_errors;
  unsigned long _base_tx_fifo_errors;
  unsigned long _base_tx_heartbeat_errors;
  unsigned long _base_tx_packets;
  
  unsigned long _tbusy;

  void stats_poll();
  static void interrupt_notifier(struct device *, unsigned);
  static String read_counts(Element *, void *);
  
};

#endif
