#ifdef HAVE_CONFIG_H
# include <config.h>
#endif
#include "glue.hh"
#include "fromdevice.hh"
#include "error.hh"
#include "packet.hh"
#include "confparse.hh"
#include "router.hh"
extern "C" {
#include <linux/netdevice.h>
}

static int min_ifindex;
static Vector<FromDevice *> *ifindex_map;
static int registered_readers;
static struct notifier_block notifier;

extern "C" int click_FromDevice_in(struct notifier_block *nb, unsigned long val, void *v);


FromDevice::FromDevice()
  : _dev(0), _registered(0)
{
#ifdef CLICK_BENCHMARK
  _bm_done = 0;
#endif
  add_output();
}

FromDevice::FromDevice(const String &devname)
  : _devname(devname), _dev(0), _registered(0)
{
#ifdef CLICK_BENCHMARK
  _bm_done = 0;
#endif
  add_output();
}

FromDevice::~FromDevice()
{
  if (_registered) click_chatter("FromDevice still registered");
  uninitialize();
}

void
FromDevice::static_initialize()
{
  if (!ifindex_map)
    ifindex_map = new Vector<FromDevice *>;
  notifier.notifier_call = click_FromDevice_in;
  notifier.priority = 1;
}

void
FromDevice::static_cleanup()
{
  delete ifindex_map;
#ifdef HAVE_CLICK_KERNEL
  if (registered_readers)
    unregister_net_in(&notifier);
#endif
}


FromDevice *
FromDevice::clone() const
{
  return new FromDevice();
}

int
FromDevice::configure(const String &conf, ErrorHandler *errh)
{
  return cp_va_parse(conf, this, errh,
		     cpString, "interface name", &_devname,
		     cpEnd);
}

static int
update_ifindex_map(int ifindex, ErrorHandler *errh)
{
  if (ifindex_map->size() == 0) {
    min_ifindex = ifindex;
    ifindex_map->push_back(0);
    return 0;
  }
  
  int left = min_ifindex;
  int right = min_ifindex + ifindex_map->size();
  if (ifindex < left) left = ifindex;
  if (ifindex >= right) right = ifindex + 1;
  if (right - left >= 1000 || right - left < 0)
    return errh->error("too many devices");

  if (left < min_ifindex) {
    int delta = min_ifindex - left;
    for (int i = 0; i < delta; i++)
      ifindex_map->push_back(0);
    for (int i = ifindex_map->size() - delta - 1; i >= 0; i--)
      (*ifindex_map)[i + delta] = (*ifindex_map)[i];
    for (int i = 0; i < delta; i++)
      (*ifindex_map)[i] = 0;
    min_ifindex = left;
  } else if (right > min_ifindex + ifindex_map->size()) {
    int delta = right - min_ifindex - ifindex_map->size();
    for (int i = 0; i < delta; i++)
      ifindex_map->push_back(0);
  }
  return 0;
}

/*
 * Use a Linux interface added by us, in net/core/dev.c,
 * to register to grab incoming packets.
 */
int
FromDevice::initialize(ErrorHandler *errh)
{
  _dev = dev_get(_devname.cc());
  if (!_dev)
    return errh->error("no device `%s'", _devname.cc());
  
  if (!_registered) {
    // add to ifindex_map
    if (update_ifindex_map(_dev->ifindex, errh) < 0)
      return -1;
    int ifindex_off = _dev->ifindex - min_ifindex;
    if ((*ifindex_map)[ifindex_off])
      return errh->error("duplicate FromDevice for device `%s'", _devname.cc());
    (*ifindex_map)[ifindex_off] = this;
    
    if (!registered_readers) {
#ifdef HAVE_CLICK_KERNEL
      notifier.next = 0;
      register_net_in(&notifier);
#else
      errh->warning("can't get packets: not compiled for a Click kernel");
#endif
    }
    
    _registered = 1;
    registered_readers++;
  }
  
  return 0;
}

void
FromDevice::uninitialize()
{
  if (_registered) {
    registered_readers--;
#ifdef HAVE_CLICK_KERNEL
    if (registered_readers == 0)
      unregister_net_in(&notifier);
#endif

    // remove from ifindex_map
    int ifindex_off = _dev->ifindex - min_ifindex;
    (*ifindex_map)[ifindex_off] = 0;
    while (ifindex_map->size() > 0 && ifindex_map->back() == 0)
      ifindex_map->pop_back();
    
    _registered = 0;
  }
}


/*
 * Called by net_bh() with each packet.
 */
extern "C" int
click_FromDevice_in(struct notifier_block *nb, unsigned long backlog_len,
		    void *v)
{
  static int called_times;
  struct sk_buff *skb = (struct sk_buff *)v;

#if CLICK_STATS > 0 || XCYC > 0
  entering_ipb();
#endif
  
  int stolen = 0;
  int ifindex_off = skb->dev->ifindex - min_ifindex;
  if (ifindex_off >= 0 && ifindex_off < ifindex_map->size())
    if (FromDevice *kr = (*ifindex_map)[ifindex_off]) {

#if CLICK_STATS >= 2
      unsigned long long c0 = click_get_cycles();
#endif
      
      stolen = kr->got_skb(skb);

#if CLICK_STATS >= 2
      unsigned long long c1 = click_get_cycles();
      kr->_calls += 1;
      kr->_self_cycles += c1 - c0;
#endif

      // Call scheduled things.
      // This causes a bit of batching, so that not every packet causes
      // Queue to put ToDevice on the work list.
      // What about the last packet? If net_bh's backlog queue is
      // empty, we want to run_scheduled().
      called_times++;
      if (called_times == 8 || backlog_len == 0) {
	extern Router *current_router; // module.cc
	if (current_router->any_scheduled())
	  current_router->run_scheduled();
	called_times = 0;
      }
    }
  
#if CLICK_STATS > 0 || XCYC > 0
  leaving_ipb();
#endif

  return (stolen ? NOTIFY_STOP_MASK : 0);
}

/*
 * Per-FromDevice packet input routine.
 */
int
FromDevice::got_skb(struct sk_buff *skb)
{
  assert(skb->data - skb->head >= 14);
  assert(skb->mac.raw == skb->data - 14);
  assert(skb_shared(skb) == 0); /* else skb = skb_clone(skb, GFP_ATOMIC); */

#if CLICK_STATS > 0
  extern int rtm_ipackets;
  extern unsigned long long rtm_ibytes;
  rtm_ipackets++;
  rtm_ibytes += skb->len;
#endif

  /* Retrieve the ether header. */
  skb_push(skb, 14);

  Packet *p = Packet::make(skb);
  if(skb->pkt_type == PACKET_MULTICAST || skb->pkt_type == PACKET_BROADCAST)
    p->set_mac_broadcast_anno(1);
  output(0).push(p);
  return(1);
}

#ifdef CLICK_BENCHMARK
/*
 * Benchmark FromDevice
 */
void
FromDevice::bm()
{
  unsigned long long c0, nc;
  struct sk_buff *skb;

  skb = alloc_skb(100, GFP_ATOMIC);
  skb->dev = _dev;

  c0 = click_get_cycles();

  int i;
  for(i = 0; i < 100000; i++){
    skb->data = skb->head + 14;
    skb->tail = skb->data + 20;
    skb->len = 20;
    skb->mac.raw = skb->data - 14;
    handoff(&_xnb._nb, skb);
    if(atomic_read(&skb->users) != 1){
      printk("<1>FromDevice::bm oops\n");
      skb = 0;
      break;
    }
  }

  nc = click_get_cycles() - c0;
  printk("<1>%u %u for %d packet FromDevice::bm()\n",
         (int) (nc >> 32),
         (int) nc,
         i);
  
  if(skb){
    skb->dev = 0;
    kfree_skb(skb);
  }
}
#endif

EXPORT_ELEMENT(FromDevice)
