/*
 * tulipstats.{cc,hh} -- element sends packets to Linux devices.
 * Eddie Kohler
 *
 * Copyright (c) 1999-2000 Massachusetts Institute of Technology
 * Copyright (c) 2000 Mazu Networks, Inc.
 *
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files (the "Software"),
 * to deal in the Software without restriction, subject to the following
 * conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * Further elaboration of this license, including a DISCLAIMER OF ANY
 * WARRANTY, EXPRESS OR IMPLIED, is provided in the LICENSE file, which is
 * also accessible at http://www.pdos.lcs.mit.edu/click/license.html
 */

#include <click/config.h>
#include <click/package.hh>
#include <click/glue.hh>
#include "tulipstats.hh"
#include <click/error.hh>
#include <click/etheraddress.hh>
#include <click/straccum.hh>
#include <click/confparse.hh>
#include "elements/standard/scheduleinfo.hh"
extern "C" {
#define new xxx_new
#define class xxx_class
#define delete xxx_delete
#include <linux/pci.h>
#include <linux/netdevice.h>
#include <asm/processor.h>
#include <asm/bitops.h>
#include <asm/io.h>
#include <asm/unaligned.h>
#undef new
#undef class
#undef delete
}

/* for hot-swapping */
static AnyDeviceMap tulip_stats_map;
static int tulip_stats_count;
static int tulip_stats_active;

#if HAVE_TULIP_INTERRUPT_HOOK
extern "C" void (*tulip_interrupt_hook)(struct device *, unsigned);
#endif

/* from tulip.c */
enum tulip_offsets {
	CSR0=0,    CSR1=0x08, CSR2=0x10, CSR3=0x18, CSR4=0x20, CSR5=0x28,
	CSR6=0x30, CSR7=0x38, CSR8=0x40, CSR9=0x48, CSR10=0x50, CSR11=0x58,
	CSR12=0x60, CSR13=0x68, CSR14=0x70, CSR15=0x78 };

static const char *eb_names[] = {
  "EB0 (parity error)",
  "EB1 (master abort)",
  "EB2 (target abort)",
  "EB3 (reserved)",
  "EB4 (reserved)",
  "EB5 (reserved)",
  "EB6 (reserved)",
  "EB7 (reserved)"
};
static const char *ts_names[] = {
  "TS0 (stopped)",
  "TS1 (run, fetching xmit descrip)",
  "TS2 (run, waiting for eot)",
  "TS3 (run, reading memory buffer)",
  "TS4 (reserved)",
  "TS5 (run, setup packet)",
  "TS6 (susp, xmit fifo underflow)",
  "TS7 (run, closing xmit descrip)"
};
static const char *rs_names[] = {
  "RS0 (stopped)",
  "RS1 (run, fetch recv descrip)",
  "RS2 (run, checking for eor)",
  "RS3 (run, waiting for recv packet)",
  "RS4 (susp, unavail recv buffer)",
  "RS5 (run, closing recv descrip)",
  "RS6 (run, flushing current frame)",
  "RS7 (run, queue recv frame)"
};
static const char *intr_names[] = {
  "TI (transmit intr)",
  "TPS (transmit process stopped)",
  "TU (transmit buffer unavailable)",
  "TJT (transmit jabber timeout)",
  "IX4 (reserved)",
  "UNF (transmit underflow)",
  "RI (receive interrupt)",
  "RU (receive buffer unavailable)",
  "RPS (receive process stopped)",
  "RWT (receive watchdog timeout)",
  "ETI (early transmit intr)",
  "GTE (general-purpose timer expired)",
  "IX12 (reserved)",
  "FBE (fatal bus error)",
  "ERI (early receive intr)",
  "AIS (abnormal intr summary)",
  "NIS (normal intr summary)"
};

static void
tulipstats_static_initialize()
{
  tulip_stats_count++;
  if (tulip_stats_count > 1) return;
  tulip_stats_map.initialize();
}

static void
tulipstats_static_cleanup()
{
  tulip_stats_count--;
}

TulipStats::TulipStats()
  : _task(this)
{
  // no MOD_INC_USE_COUNT; rely on AnyDevice
  tulipstats_static_initialize();
}

TulipStats::~TulipStats()
{
  // no MOD_DEC_USE_COUNT; rely on AnyDevice
  tulipstats_static_cleanup();
}


int
TulipStats::configure(const Vector<String> &conf, ErrorHandler *errh)
{
  if (cp_va_parse(conf, this, errh,
		  cpString, "interface name", &_devname,
		  cpEnd) < 0)
    return -1;
  _dev = dev_get(_devname.cc());
  if (!_dev)
    _dev = find_device_by_ether_address(_devname, this);
  if (!_dev)
    return errh->error("no device `%s'", _devname.cc());
  return 0;
}

int
TulipStats::initialize(ErrorHandler *errh)
{
#ifndef HAVE_CLICK_KERNEL
  errh->warning("not compiled for a Click kernel");
#endif

  if (tulip_stats_map.insert(this) < 0)
    return errh->error("cannot use TulipStats for device `%s'", _devname.cc());
  
  ScheduleInfo::join_scheduler(this, &_task, errh);

  tulip_stats_active++;
#if HAVE_TULIP_INTERRUPT_HOOK
  if (tulip_stats_active == 1)
    tulip_interrupt_hook = interrupt_notifier;
#endif

  _dev_stats = _dev->get_stats(_dev);
  
  reset_counts();
  
  return 0;
}

void
TulipStats::uninitialize()
{
  _task.unschedule();
  tulip_stats_map.remove(this);

  tulip_stats_active--;
#if HAVE_TULIP_INTERRUPT_HOOK
  if (tulip_stats_active == 0)
    tulip_interrupt_hook = 0;
#endif
}

void
TulipStats::stats_poll()
{
  _nstats_polls++;
  
  long ioaddr = _dev->base_addr;
  unsigned csr5 = inl(ioaddr + CSR5);
  
  unsigned eb = (csr5 >> 23) & 7;
  unsigned ts = (csr5 >> 20) & 7;
  unsigned rs = (csr5 >> 17) & 7;
  _ts[ts]++;
  _rs[rs]++;
  _eb[eb]++;

  unsigned csr8 = inl(ioaddr+CSR8);
  _dev_stats->rx_dropped += (csr8 & 0x10000 ? 0x10000 : csr8);
  _dev_stats->rx_missed_errors += csr8 & 0xffff;
  _dev_stats->rx_fifo_errors += (csr8 >> 17) & 0x7ff;
  if (csr8 & (1 << 28))
    _oco++;
  if (csr8 & (1 << 16))
    _mfo++;

  if (_dev->tbusy)
    _tbusy++;
}

void
TulipStats::run_scheduled()
{
  stats_poll();
  _task.fast_reschedule();
}

void
TulipStats::reset_counts()
{
  for (int i = 0; i < 8; i++)
    _ts[i] = _rs[i] = _eb[i] = 0;
  for (int i = 0; i < 17; i++)
    _intr[i] = 0;
  _nstats_polls = _nintr = 0;
  _mfo = _oco = 0;
  _base_rx_missed_errors = _dev_stats->rx_missed_errors;
  _base_rx_fifo_errors = _dev_stats->rx_fifo_errors;
  _base_tx_errors = _dev_stats->tx_errors;
  _base_tx_aborted_errors = _dev_stats->tx_aborted_errors;
  _base_tx_carrier_errors = _dev_stats->tx_carrier_errors;
  _base_tx_window_errors = _dev_stats->tx_window_errors;
  _base_tx_fifo_errors = _dev_stats->tx_fifo_errors;
  _base_tx_heartbeat_errors = _dev_stats->tx_heartbeat_errors;
  _base_tx_packets = _dev_stats->tx_packets;
  _tbusy = 0;
}

void
TulipStats::interrupt_notifier(struct device *dev, unsigned csr5)
{
  AnyDevice *anydev = tulip_stats_map.lookup(dev->ifindex);
  if (!anydev)
    return;
  TulipStats *tulips = static_cast<TulipStats *>(anydev);

  tulips->_nintr++;
  for (unsigned bit = 0; bit < 17; bit++)
    if (csr5 & (1 << bit))
      tulips->_intr[bit]++;

  tulips->stats_poll();
}

static void
append_line(StringAccum &sa, const char *desc, unsigned long num)
{
  int l = strlen(desc) + 1;
  sa.push(desc, l - 1);
  sa.push(':');
  while (l < 40) {
    sa.push('\t');
    l = ((l + 8) >> 3) << 3;
  }
  sa << num << "\n";
}

String
TulipStats::read_counts(Element *e, void *)
{
  TulipStats *ts = static_cast<TulipStats *>(e);
  struct net_device_stats *dev_stats = ts->_dev_stats;
  StringAccum sa;
  append_line(sa, "PC (polling stats count)", ts->_nstats_polls);
  sa << "\n";
  for (int i = 0; i < 8; i++)
    append_line(sa, eb_names[i], ts->_eb[i]);
  sa << "\n";
  for (int i = 0; i < 8; i++)
    append_line(sa, ts_names[i], ts->_ts[i]);
  sa << "\n";
  for (int i = 0; i < 8; i++)
    append_line(sa, rs_names[i], ts->_rs[i]);
  sa << "\n";
  append_line(sa, "IC (intr count)", ts->_nintr);
  sa << "\n";
  for (int i = 0; i < 17; i++)
    append_line(sa, intr_names[i], ts->_intr[i]);
  sa << "\n";
  append_line(sa, "OCO (overflow ctr overflow)", ts->_oco);
  append_line(sa, "FOC (fifo overflow ctr)", dev_stats->rx_fifo_errors - ts->_base_rx_fifo_errors);
  append_line(sa, "MFO (missed frame overflow)", ts->_mfo);
  append_line(sa, "MFC (missed frame ctr)", dev_stats->rx_missed_errors - ts->_base_rx_missed_errors);
  sa << "\n";
  append_line(sa, "TBZ (device xmit busy)", ts->_tbusy);
  append_line(sa, "TXE (xmit errors)", dev_stats->tx_errors - ts->_base_tx_errors);
  append_line(sa, "TXA (xmit aborted)", dev_stats->tx_aborted_errors - ts->_base_tx_aborted_errors);
  append_line(sa, "TXC (xmit carrier)", dev_stats->tx_carrier_errors - ts->_base_tx_carrier_errors);
  append_line(sa, "TXW (xmit window)", dev_stats->tx_window_errors - ts->_base_tx_window_errors);
  append_line(sa, "TXF (xmit fifo)", dev_stats->tx_fifo_errors - ts->_base_tx_fifo_errors);
  append_line(sa, "TXH (xmit heartbeat)", dev_stats->tx_heartbeat_errors - ts->_base_tx_heartbeat_errors);
  append_line(sa, "TPO (tulip reported tx packets)", dev_stats->tx_packets - ts->_base_tx_packets);
  return sa.take_string();
}

static int
TulipStats_reset(const String &, Element *e, void *, ErrorHandler *)
{
  TulipStats *ts = static_cast<TulipStats *>(e);
  ts->reset_counts();
  return 0;
}

void
TulipStats::add_handlers()
{
  add_read_handler("counts", read_counts, 0);
  add_write_handler("reset_counts", TulipStats_reset, 0);
  add_task_handlers(&_task);
}

/* If you want to include TulipStats in your Click kernel driver, remove
 * `false' from the line below. */
ELEMENT_REQUIRES(AnyDevice linuxmodule false)
EXPORT_ELEMENT(TulipStats)
