/*
 * module.cc -- Linux kernel module main program
 * Eddie Kohler, Robert Morris
 *
 * Copyright (c) 1999 Massachusetts Institute of Technology.
 *
 * This software is being provided by the copyright holders under the GNU
 * General Public License, either version 2 or, at your discretion, any later
 * version. For more information, see the `COPYRIGHT' file in the source
 * distribution.
 */

#ifdef HAVE_CONFIG_H
# include <config.h>
#endif
#include "linuxmodule.hh"

#include "lexer.hh"
#include "router.hh"
#include "elements/linuxmodule/fromdevice.hh"
#include "elements/linuxmodule/todevice.hh"
#include "kernelerror.hh"
#include "straccum.hh"
#include "confparse.hh"

struct proc_dir_entry proc_click_entry = {
  0,				// dynamic inode
  5, "click",			// name
  S_IFDIR | S_IRUGO | S_IXUGO,
  2, 0, 0,			// nlink, uid, gid
  0, &proc_dir_inode_operations,
  NULL, NULL,
  NULL,
  NULL, NULL
};

ErrorHandler *kernel_errh = 0;
static Lexer *lexer = 0;
Router *current_router = 0;

static Vector<String> provisions;


class LinuxModuleLexerSource : public MemoryLexerSource {
 public:
  LinuxModuleLexerSource(const char *d, unsigned l)
    : MemoryLexerSource(d, l) { }
  void require(const String &, ErrorHandler *);
};

void
LinuxModuleLexerSource::require(const String &r, ErrorHandler *errh)
{
  for (int i = 0; i < provisions.size(); i++)
    if (provisions[i] == r)
      return;
  errh->error("unsatisfied requirement `%s'", String(r).cc());
}

void
initialize_router(const char *data, unsigned len)
{
  LinuxModuleLexerSource memsource(data, len);
  lexer->reset(&memsource);
  cleanup_router_element_procs();
  reset_proc_click_errors();
  delete current_router;
  
  while (lexer->ystatement())
    /* do nothing */;
  
  current_router = lexer->create_router();
  lexer->clear();
  
  if (current_router) {
    if (current_router->initialize(kernel_errh) >= 0)
      current_router->print_structure(kernel_errh);
    init_router_element_procs();
  }
}

/*
 * Count cycles for all of IPB code.
 * FromDevice and ToDevice call entering_ipb() and leaving_ipb().
 */
unsigned long long click_cycles = 0;
unsigned int click_calls = 0;
unsigned long long click_enter_time;
int click_entered;
int rtm_ipackets, rtm_opackets;
unsigned long long rtm_ibytes, rtm_obytes;
unsigned long long rtm_t0;
unsigned long long rtm_dev_packets;
unsigned long long rtm_dev_bytes;

static unsigned long cli_flags;

#if CLICK_STATS > 0 || XCYC > 0
void
_entering_ipb()
{
#if 1
  save_flags(cli_flags);
  cli();
#endif

  assert(click_entered == 0);
  click_entered++;
  click_enter_time = click_get_cycles();
}

void
_leaving_ipb()
{
  unsigned long long t;
  
  t = click_get_cycles();
  click_cycles += t - click_enter_time;
  click_calls += 1;
  assert(click_entered == 1);
  click_entered -= 1;
  
#if 1
  restore_flags(cli_flags);
#endif
}
#endif /* CLICK_STATS */

extern "C" void
click_cycle_counter(int which, unsigned int *fnp, unsigned long long *valp);

static String
read_cycles(Element *, void *)
{
  StringAccum sa;

#if 0
  extern unsigned int rtm_irq_calls, rtm_bh_calls, rtm_tulip_calls;
  extern unsigned long long rtm_irq_cycles, rtm_bh_cycles, rtm_tulip_cycles;
  unsigned int cli_flags;
  unsigned int fn0, fn1;
  unsigned long long val0, val1;
  
  save_flags(cli_flags);
  cli();
  unsigned long long now = click_get_cycles();
  unsigned long long t = now - rtm_t0;
  rtm_t0 = now;
  click_cycle_counter(0, &fn0, &val0);
  click_cycle_counter(1, &fn1, &val1);
  restore_flags(cli_flags);
  
  unsigned long long top = 0;
  unsigned long long tob = 0;
  struct device *dev;
  for(dev = dev_base; dev; dev = dev->next){
    if(dev->get_stats){
      struct net_device_stats *stats = dev->get_stats(dev);
      top += stats->tx_packets;
      tob += stats->tx_bytes;
    }
  }
  
  char tmp[128];
  sprintf(tmp, "0x%x %u %u, 0x%x %u %u\n",
          fn0,
          (int)(val0 >> 32),
          (int)val0,
          fn1,
          (int)(val1 >> 32),
          (int)val1);          
  
  sa << t << " cycles\n";
  sa << tmp;
  sa << rtm_ipackets << " in packets\n";
  sa << rtm_ibytes << " in bytes\n";
  sa << rtm_opackets << " out packets\n";
  sa << rtm_obytes << " out bytes\n";
  sa << (int)(top - rtm_dev_packets) << " device outputs\n";
  sa << (tob - rtm_dev_bytes) << " device output bytes\n";
  sa << click_calls << " ipb calls\n";
  sa << click_cycles << " ipb cycles\n";
  sa << rtm_irq_calls << " do_IRQ calls\n";
  sa << rtm_irq_cycles << " interrupt cycles\n";
  sa << rtm_tulip_calls << " tulip interrupts\n";
  sa << rtm_tulip_cycles << " tulip interrupt cycles\n";
  sa << rtm_bh_calls << " net_bh calls\n";
  sa << rtm_bh_cycles << " net_bh cycles\n";
  rtm_ipackets = rtm_opackets = 0;
  rtm_irq_calls = rtm_tulip_calls = rtm_bh_calls = 0;
  rtm_irq_cycles = rtm_tulip_cycles = rtm_bh_cycles = 0;
  rtm_dev_packets = top;
  rtm_dev_bytes = tob;
#endif
  
  sa << click_get_cycles() << " cycles\n";
  sa << click_calls << " ipb calls\n";
  sa << click_cycles << " ipb cycles\n";
  click_calls = 0;
  click_cycles = 0;

#if 0 && defined(HAVE_CLICK_KERNEL)
  extern unsigned int rtm_irq_calls;
  extern unsigned long long rtm_irq_cycles;
  sa << rtm_irq_calls << " irq calls\n";
  sa << rtm_irq_cycles << " irq cycles\n";
  rtm_irq_calls = 0;
  rtm_irq_cycles = 0;

  extern unsigned int rtm_ip_calls;
  extern unsigned long long rtm_ip_cycles;
  sa << rtm_ip_calls << " ip calls\n";
  sa << rtm_ip_cycles << " ip cycles\n";
  rtm_ip_calls = 0;
  rtm_ip_cycles = 0;

  extern unsigned int rtm_tulip_calls;
  extern unsigned long long rtm_tulip_cycles;
  sa << rtm_tulip_calls << " tulip calls\n";
  sa << rtm_tulip_cycles << " tulip cycles\n";
  rtm_tulip_calls = 0;
  rtm_tulip_cycles = 0;

  extern atomic_t netdev_rx_dropped;
  sa << atomic_read(&netdev_rx_dropped) << " netdev_rx_dropped\n";
  atomic_set(&netdev_rx_dropped, 0);
#endif

  return sa.take_string();
}

#ifdef HAVE_READ_NET_SKBCOUNT
extern "C" int read_net_skbcount(void);
#endif

static String
read_meminfo(Element *, void *)
{
  extern int click_new_count; /* glue.cc */
  extern int click_outstanding_news; /* glue.cc */
  StringAccum sa;
  sa << "outstanding news " << click_outstanding_news << "\n";
  sa << "news " << click_new_count << "\n";
#ifdef HAVE_READ_NET_SKBCOUNT
  sa << "net_skbcount " << read_net_skbcount() << "\n";
#endif
  return sa.take_string();
}

static String
read_flatconfig(Element *, void *)
{
  if (current_router)
    return current_router->flat_configuration_string();
  else
    return "";
}

static String
read_list(Element *, void *)
{
  if (!current_router)
    return "0\n";
  else
    return current_router->element_list_string();
}

static String
read_classes(Element *, void *)
{
  int nft = lexer->permanent_element_types();
  StringAccum sa;
  for (int i = lexer->first_element_type(); i < nft; i++)
    sa << lexer->element_type(i)->class_name() << "\n";
  return sa.take_string();
}

static String
read_requirements(Element *, void *)
{
  if (current_router) {
    const Vector<String> &v = current_router->requirements();
    StringAccum sa;
    for (int i = 0; i < v.size(); i++)
      sa << cp_unsubst(v[i]) << "\n";
    return sa.take_string();
  } else
    return "";
}

static int
write_driver(const String &conf_in, Element *, void *, ErrorHandler *errh)
{
  int num;
  String conf = cp_subst(conf_in);
  if (!conf)
    num = 1;
  else if (!cp_integer(conf, num)) {
    errh->error("write an integer to /proc/click/driver");
    return -EINVAL;
  }
  if (!current_router) {
    errh->error("no router to drive");
    return -EINVAL;
  }
  for (int i = 0; i < num; i++)
    if (!current_router->driver())
      break;
  return 0;
}


extern "C" void
click_provide(const char *name)
{
  MOD_INC_USE_COUNT;
  provisions.push_back(String(name));
}

extern "C" void
click_unprovide(const char *name)
{
  String n = name;
  for (int i = 0; i < provisions.size(); i++)
    if (provisions[i] == n) {
      MOD_DEC_USE_COUNT;
      provisions[i] = provisions.back();
      provisions.pop_back();
      break;
    }
}

extern "C" void
click_add_element_type(const char *name, Element *e)
{
  lexer->add_element_type(name, e);
}

extern "C" void
click_remove_element_type(const char *name)
{
  lexer->remove_element_type(name);
}


extern void export_elements(Lexer *);

extern "C" int
init_module()
{
  // first call C++ static initializers
  String::static_initialize();
  Element::static_initialize();
  ErrorHandler::static_initialize(new KernelErrorHandler);
  FromDevice::static_initialize();
  ToDevice::static_initialize();
  
  kernel_errh = ErrorHandler::default_handler();
  extern ErrorHandler *click_chatter_errh;
  click_chatter_errh = kernel_errh;
  
  lexer = new Lexer(kernel_errh);
  export_elements(lexer);
  lexer->save_element_types();
  
  current_router = 0;
  
  click_register_pde(&proc_root, &proc_click_entry);
  init_click_proc();  
  init_proc_click_config();
  init_proc_click_elements();
  init_proc_click_errors();

  // add handlers to the root directory. warning: this only works if there
  // is no current_router while the handlers are being added.
  KernelHandlerRegistry kfr(&proc_click_entry);
  kfr.add_read("meminfo", read_meminfo, 0);
  kfr.add_read("cycles", read_cycles, 0);
  kfr.add_read("flatconfig", read_flatconfig, 0);
  kfr.add_read("list", read_list, 0);
  kfr.add_read("classes", read_classes, 0);
  kfr.add_read("requirements", read_requirements, 0);
  kfr.add_write("driver", write_driver, 0);
  
  return 0;
}

extern "C" void
cleanup_module()
{
  extern int click_new_count; /* glue.cc */
  extern int click_outstanding_news; /* glue.cc */
  
  cleanup_proc_click_errors();
  cleanup_proc_click_elements();
  cleanup_proc_click_config();
  click_unregister_pde(&proc_click_entry);
  cleanup_click_proc();
  delete current_router;
  delete lexer;
  
  extern ErrorHandler *click_chatter_errh;
  click_chatter_errh = 0;
  
  ErrorHandler::static_cleanup();
  FromDevice::static_cleanup();
  ToDevice::static_cleanup();
  printk("<1>click module exiting\n");
  printk("<1>still allocated: %d elements\n", Element::nelements_allocated);
  
  printk("<1>cycles: %s", read_cycles(0, 0).cc());
  
  // must call after all operations that might create strings are done
  String::static_cleanup();
  printk("<1>outstanding news: %d\n", click_outstanding_news);
#ifdef HAVE_READ_NET_SKBCOUNT
  printk("<1>net_skbcount: %d\n", read_net_skbcount());
#endif
}
