#ifndef TULIPSTATS_HH
#define TULIPSTATS_HH

/*
 * =c
 * TulipStats(DEVNAME)
 * =s profiling
 * reports statistics gathered from Tulip device
 * =io
 * None
 *
 * =d
 *
 * TulipStats gathers statistics from the Tulip network device named DEVNAME
 * and reports them via the C<counts> handler. It schedules itself on the task
 * queue; use ScheduleInfo to set its scheduling tickets (it doesn't need very
 * many). Each time it is scheduled, TulipStats will poll the Tulip's receive,
 * transmit, and error states. It will be called whenever there is an
 * interrupt, and maintains counts of how many interrupts happened and why. It
 * also maintains detailed counts of missed frame count errors, FIFO
 * overflows, and many other kinds of errors. Some of these values are already
 * maintained by the device driver, but their values can be unreliable under
 * heavy load unless TulipStats is used.
 *
 * =e
 *
 * Here is a sample use of TulipStats:
 *
 *    ts :: TulipStats(eth0);
 *    ScheduleInfo(ts 0.01);
 *
 * =h counts read-only
 *
 * Returns all statistics kept by TulipStats, one per line. Each line has a
 * human-readable description and a number.
 *
 * =h reset_counts write-only
 *
 * Reset all counts to zero.
 *
 * =n
 *
 * The C<counts> handler returns a lot of information. Here is a sample dump:
 *
 *   PC (polling stats count):               15031503
 *   
 *   EB0 (parity error):                     15031503
 *   EB1 (master abort):                     0
 *   EB2 (target abort):                     0
 *   EB3 (reserved):                         0
 *   EB4 (reserved):                         0
 *   EB5 (reserved):                         0
 *   EB6 (reserved):                         0
 *   EB7 (reserved):                         0
 *   
 *   TS0 (stopped):                          15031503
 *   TS1 (run, fetching xmit descrip):       0
 *   TS2 (run, waiting for eot):             0
 *   TS3 (run, reading memory buffer):       0
 *   TS4 (reserved):                         0
 *   TS5 (run, setup packet):                0
 *   TS6 (susp, xmit fifo underflow):        0
 *   TS7 (run, closing xmit descrip):        0
 *   
 *   RS0 (stopped):                          15031503
 *   RS1 (run, fetch recv descrip):          0
 *   RS2 (run, checking for eor):            0
 *   RS3 (run, waiting for recv packet):     0
 *   RS4 (susp, unavail recv buffer):        0
 *   RS5 (run, closing recv descrip):        0
 *   RS6 (run, flushing current frame):      0
 *   RS7 (run, queue recv frame):            0
 *   
 *   IC (intr count):                        0
 *   
 *   TI (transmit intr):                     0
 *   TPS (transmit process stopped):         0
 *   TU (transmit buffer unavailable):       0
 *   TJT (transmit jabber timeout):          0
 *   IX4 (reserved):                         0
 *   UNF (transmit underflow):               0
 *   RI (receive interrupt):                 0
 *   RU (receive buffer unavailable):        0
 *   RPS (receive process stopped):          0
 *   RWT (receive watchdog timeout):         0
 *   ETI (early transmit intr):              0
 *   GTE (general-purpose timer expired):    0
 *   IX12 (reserved):                        0
 *   FBE (fatal bus error):                  0
 *   ERI (early receive intr):               0
 *   AIS (abnormal intr summary):            0
 *   NIS (normal intr summary):              0
 *   
 *   OCO (overflow ctr overflow):            0
 *   FOC (fifo overflow ctr):                0
 *   MFO (missed frame overflow):            0
 *   MFC (missed frame ctr):                 0
 *   
 *   TBZ (device xmit busy):                 0
 *   TXE (xmit errors):                      0
 *   TXA (xmit aborted):                     0
 *   TXC (xmit carrier):                     0
 *   TXW (xmit window):                      0
 *   TXF (xmit fifo):                        0
 *   TXH (xmit heartbeat):                   0
 *   TPO (tulip reported tx packets):        0
 *
 * C<PC (polling stats count)> is the number of times the Tulip was polled.
 * C<IC (intr count)> is the number of interrupts it generated, more or less.
 * (The device driver can sometimes count more interrupts than actually
 * occurred.)
 *
 * Every time the Tulip is polled, TulipStats takes note of its error state,
 * transmit state, and receive state. C<EB0> through C<EB7> are error state
 * counts, C<TS0> through C<TS7> transmit state counts, and C<RS0> through
 * C<RS7> receive state counts. The sum of the eight values C<EB0> through
 * C<EB7> will equal C<PC>, and similary for C<TSx> and C<RSx>.
 *
 * The group of stats starting with C<TI (transmit intr)> and ending with
 * C<NIS (normal intr summary)> correspond to the reasons the Tulip gave for
 * interrupting.
 *
 * The rest of the stats, starting with C<OCO (overflow ctr overflow)>, report
 * the values for counters on the Tulip, such as missed frame counters.
 * TulipStats reports the accumulated difference in these counters since it
 * was installed. Thus, C<MFC> does not report the total number of missed
 * frames since last boot, but only the number of missed frames since
 * TulipStats was installed, or since the last C<reset_counts>.
 *
 * =a ScheduleInfo, FromDevice, PollDevice */

#include "elements/linuxmodule/anydevice.hh"
#include "elements/linuxmodule/fromlinux.hh"

class TulipStats : public AnyDevice {
  
 public:
  
  TulipStats();
  ~TulipStats();
  
  const char *class_name() const	{ return "TulipStats"; }
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

  Task _task;
  
  void stats_poll();
  static void interrupt_notifier(struct device *, unsigned);
  static String read_counts(Element *, void *);
  
};

#endif
