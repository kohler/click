/* udpgen -- output UDP packets.
 *
 * Module parameters:
 *
 * parameter	default		description
 * ---------	-------		-----------
 * data		UDP data!\n	data in UDP packet. Will be repeated as
 *				necessary to fill a packet to `len' bytes.
 * len		-1		total length of packet w/all headers. < 0 means
 *				calculate based on `data'.
 * rate		1000		packets per second
 * time		1		number of seconds
 * saddr	127.0.0.1	source IP address
 * daddr	127.0.0.1	destination IP address
 * sport	0x1369		source UDP port
 * dport	0x1369		destination UDP port
 * checksum	1		Do UDP checksums?
 * priority	0		IP priority
 * tos		0		IP terms of service
 * ttl		64		IP TTL
 * showpacket	0		show hex dump of packet contents
 *
 * Each of rate, dport, tos, ttl, priority can be given as a list of up
 * to four numbers.
 */

#include <asm/system.h>
#include <linux/types.h>
#include <linux/socket.h>
#include <linux/net.h>
#include <linux/in.h>
#include <linux/inet.h>
#include <linux/netdevice.h>
#include <linux/inetdevice.h>
#include <linux/ip.h>
#include <linux/udp.h>
#include <net/route.h>
#include <net/checksum.h>
#include <net/ip.h>
#include <linux/module.h>
#include <linux/tqueue.h>
#include <linux/delay.h>

struct udpgen_opt {

  struct udpgen_opt *next;
  
  struct tq_struct tq;

  int rate;
  int ngap;

  int total_packets_sent;	/* 0 */
  int total_packets_attempted;	/* 0 */
  int slow_path_sent;		/* 0 */

  int udp_checksum;
  int packet_size;
  char *data;
  int datalen;

  int ip_priority;		/* 0 */
  int ip_tos;			/* 0 */
  unsigned char ip_ttl;		/* ip_statistics.IpDefaultTTL */
  
  __u32 saddr;
  __u32 daddr;
  __u16 sport;
  __u16 dport;

  int have_hh;			/* 0 */
  int bound_dev_if;		/* 0 */

  struct sk_buff *skb_cache;

};


/* defaults for udpgen_opt */
static char *data = "UDP packet!\n";
static int len = -1;
static int rate[5] = { 1000, 0, 0, 0, 0 };
static int time = 1;
static int checksum = 1;
static int priority[5] = { 0, -1, -1, -1, -1 };
static int tos[5] = { 0, -1, -1, -1, -1 };
static int ttl[5] = { 64, -1, -1, -1, -1 };
static char *saddr = "127.0.0.1";
static char *daddr = "127.0.0.1";
static int sport = 0x1369;
static int dport[5] = { 0x1369, -1, -1, -1, -1 };
static int showpacket = 0;


static struct udpgen_opt *uopt;
static struct timeval tv_begin;
static struct timeval tv_end;
  

static void run(void *);
static void free_cached_udp_packet(struct udpgen_opt *);


static __u32
parse_ip_address(const char *s)
{
  int i;
  __u32 addr = 0;
  int place;
  for (i = 0; i < 4; i++) {
    if (i && *s != '.')
      return 0;
    if (i) s++;
    if (*s < '0' || *s > '9')
      return 0;
    for (place = 0; *s >= '0' && *s <= '9'; s++)
      place = (10*place) + *s - '0';
    if (place < 0 || place > 255)
      return 0;
    addr = (addr << 8) | place;
  }
  if (*s != 0)
    return 0;
  return htonl(addr);
}

static inline void
sub_timer(struct timeval *diff, struct timeval *tv2, struct timeval *tv1)
{
  diff->tv_sec = tv2->tv_sec - tv1->tv_sec;
  diff->tv_usec = tv2->tv_usec - tv1->tv_usec;
  if (diff->tv_usec < 0) {
    diff->tv_sec--;
    diff->tv_usec += 1000000;
  }
}

static struct udpgen_opt *
new_udpgen_opt(int which)
{
  struct udpgen_opt *uopt = (struct udpgen_opt *)kmalloc(sizeof(struct udpgen_opt), GFP_KERNEL);
  if (!uopt)
    return 0;
  uopt->next = 0;

  /* packet data */
  uopt->data = data;
  if (!data && len > 0) {
    printk("<1>udpgen: len > 0 but no data\n");
    goto error;
  }
  if (!data)
    uopt->datalen = 0;
  else
    uopt->datalen = strlen(data);
  
  if (len < 0)
    len = uopt->datalen + sizeof(struct iphdr) + sizeof(struct udphdr) + 14;
  uopt->packet_size = len;

  /* rate */
  if (rate[which] <= 0 || rate[which] > 2000000) {
    /* upper bound prevents overflow */
    printk("<1>udpgen: rate must be > 0 and <= 2000000\n");
    goto error;
  }
  if (time <= 0 || time > 180) {
    printk("<1>udpgen: time must be > 0 and <= 180\n");
    goto error;
  }
  uopt->rate = rate[which];
  uopt->ngap = 1000000000 / uopt->rate;

  uopt->total_packets_sent = 0;
  uopt->total_packets_attempted = 0;
  uopt->slow_path_sent = 0;

  tv_begin.tv_sec = 0;
  
  /* packet properties */
  uopt->udp_checksum = checksum;
  if (which && priority[which] < 0) priority[which] = priority[which - 1];
  uopt->ip_priority = priority[which];
  if (which && tos[which] < 0) tos[which] = tos[which - 1];
  uopt->ip_tos = tos[which];

  if (which && ttl[which] < 0) ttl[which] = ttl[which - 1];
  uopt->ip_ttl = ttl[which];
  if (ttl[which] < 0 || ttl[which] > 255) {
    printk("<1>udpgen: bad IP TTL\n");
    goto error;
  }

  /* source/destination */
  uopt->saddr = parse_ip_address(saddr);
  if (!uopt->saddr) {
    printk("<1>udpgen: bad source address\n");
    goto error;
  }
  
  uopt->daddr = parse_ip_address(daddr);
  if (!uopt->daddr) {
    printk("<1>udpgen: bad destination address\n");
    goto error;
  }
  
  uopt->sport = htons(sport);
  if (which && dport[which] < 0) dport[which] = dport[which - 1] + 1;
  uopt->dport = htons(dport[which]);

  /* routing, packet storage */
  uopt->have_hh = 0;
  uopt->bound_dev_if = 0;
  uopt->skb_cache = 0;
  
  uopt->tq.sync = 0;
  uopt->tq.routine = run;
  uopt->tq.data = uopt;

  return uopt;

 error:
  kfree(uopt);
  return 0;
}

static void
free_udpgen_opt(struct udpgen_opt *uopt)
{
  if (uopt) {
    free_cached_udp_packet(uopt);
    kfree(uopt);
  }
}


static struct sk_buff *
cache_udp_packet(struct udpgen_opt *uopt)
{
  struct rtable *rt = 0;
  int length;
  int pad_length;
  struct sk_buff *skb;
  struct iphdr *iph;
  struct udphdr *udph;
  unsigned char *data;

  /* get rid of old cached packet */
  if (uopt->skb_cache)
    free_cached_udp_packet(uopt);
  uopt->have_hh = 0;
  
  /* destination and routing table */
  if (ip_route_output(&rt, uopt->daddr, uopt->saddr, uopt->ip_tos,
		      uopt->bound_dev_if)) {
    if (rt)
      dst_release(&rt->u.dst);
    printk("<1>udpgen: no route\n");
    return 0;
  }
  /* XXX - check broadcast */
  
  /* packet length */
  {
    int dev_header_len = rt->u.dst.dev->hard_header_len;
    int min_packet_size =
      dev_header_len + sizeof(struct iphdr) + sizeof(struct udphdr);
    printk("<1>%d hard header %d src %d dst\n", dev_header_len, rt->rt_src, rt->rt_dst);
    if (min_packet_size < uopt->packet_size)
      length = uopt->packet_size;
    else
      length = min_packet_size;
    length -= dev_header_len;
  }

  /* get skb */
  pad_length = (length < 50 ? 50 : length); /* eth packets have min size */
  {
    int hard_header_len = (rt->u.dst.dev->hard_header_len + 15)&~15;
    int alloc_len = pad_length + hard_header_len + 15;
    skb = alloc_skb(alloc_len, GFP_KERNEL);
    if (!skb)
      return 0;
    skb_reserve(skb, hard_header_len);
  }
  
  /* set up IP header and UDP header */
  skb->priority = uopt->ip_priority;
  skb->dst = 0;

  /* set up IP header */
  /* remember that ethernet packets have a minimum size */
  skb->nh.iph = iph = (struct iphdr *)skb_put(skb, pad_length);
  
  /* dev_lock_list(); */ /* XXX why? */
  
  iph->version = 4;
  iph->ihl = 5;
  iph->tos = uopt->ip_tos;
  iph->tot_len = htons(length);
  iph->id = 0;			/* XXX htons(ip_id_count++) */
  iph->frag_off = 0;
  iph->ttl = uopt->ip_ttl;
  iph->protocol = IPPROTO_UDP;
  iph->saddr = rt->rt_src;
  iph->daddr = rt->rt_dst;
  iph->check = 0;
  iph->check = ip_compute_csum((unsigned char *)iph, iph->ihl*4);
  /* don't use ip_fast_csum; it calculates incorrectly (god knows why)! */

  /* set up UDP header */
  skb->h.uh = udph = (struct udphdr *)((char *)iph + iph->ihl*4);
  length -= iph->ihl*4;

  udph->source = uopt->sport;
  udph->dest = uopt->dport;
  udph->len = htons(length);

  /* set up data */
  data = (unsigned char *)udph + sizeof(struct udphdr);
  length -= sizeof(struct udphdr);

  if (length > 0 && uopt->datalen > 0) {
    unsigned char *d = data;
    int l = length;
    while (l > uopt->datalen) {
      memcpy(d, uopt->data, uopt->datalen);
      d += uopt->datalen;
      l -= uopt->datalen;
    }
    memcpy(d, uopt->data, l);
  }

  /* UDP checksum */
  udph->check = 0;
  if (uopt->udp_checksum) {
    unsigned int csum = csum_partial(data, length, 0);
    csum = csum_partial((unsigned char *)udph, sizeof(struct udphdr), csum);
    udph->check = csum_tcpudp_magic(uopt->saddr, uopt->daddr, ntohs(udph->len),
				    IPPROTO_UDP, csum);
    if (udph->check == 0)
      udph->check = -1;
  }
  
  /* dev_unlock_list(); */ /* XXX why? */
  
  /* device */
  skb->protocol = __constant_htons(ETH_P_IP);
  /* printk("<1>udpgen: using device `%s'\n", skb->dev->name); */
  
  /* done! */
  dst_release(&rt->u.dst);
  uopt->skb_cache = skb;
  return skb;
}

static void
free_cached_udp_packet(struct udpgen_opt *uopt)
{
  if (uopt->skb_cache) {
    kfree_skb(uopt->skb_cache);
    uopt->skb_cache = 0;
  }
}


inline static int
fast_output_packet(struct udpgen_opt *uopt)
{
  struct sk_buff *skb = skb_clone(uopt->skb_cache, GFP_KERNEL);
  struct device *dev = skb->dev;
  int ans;
  
  if (dev->tbusy)
    ans = -97;
  else
    ans = dev->hard_start_xmit(skb, dev);
  
  if (ans == 0)
    return 0;
  else {
    kfree_skb(skb);
    return (ans > 0 ? -ans : ans);
  }
}

static int
slow_output_packet(struct udpgen_opt *uopt)
{
  struct sk_buff *skb = uopt->skb_cache;
  struct hh_cache *hh;
  struct rtable *rt;
  int result;

  if (ip_route_output(&rt, uopt->daddr, uopt->saddr, uopt->ip_tos,
		      uopt->bound_dev_if)) {
    if (rt)
      dst_release(&rt->u.dst);
    printk("<1>udpgen: no route\n");
    return -1;
  }
  /* XXX - check broadcast */
  
  hh = rt->u.dst.hh;
  skb->dev = rt->u.dst.dev;
  dst_release(skb->dst);
  skb->dst = dst_clone(&rt->u.dst);
  
  if (hh) {
    /* read_lock_irq(&hh->hh_lock); */
    memcpy(skb->data - 16, hh->hh_data, 16);
    /* read_unlock_irq(&hh->hh_lock); */
    skb->mac.raw = skb_push(skb, skb->dev->hard_header_len);
    uopt->have_hh = 1;
    result = fast_output_packet(uopt);
  } else {
    skb = skb_clone(skb, GFP_KERNEL);
    uopt->slow_path_sent++;
    result = rt->u.dst.output(skb);
  }

  dst_release(&rt->u.dst);
  return result;
}

inline static int
write_cached_udp_packet(struct udpgen_opt *uopt)
{
  if (!uopt->skb_cache) /* || uopt->rtable_cache->u.dst.obsolete)*/
    cache_udp_packet(uopt);
  if (!uopt->skb_cache)
    return -EAGAIN;

  if (uopt->have_hh)
    return fast_output_packet(uopt);
  else
    return slow_output_packet(uopt);
}


static void
run(void *thunk)
{
  int i, live_uopt, nuopt, tried[5], succeeded[5];
  struct timeval tv1, tv2, diff;
  struct udpgen_opt *uopt1 = (struct udpgen_opt *)thunk;
  struct udpgen_opt *uopp;
  struct udpgen_opt *uopt[5];
  
  do_gettimeofday(&tv1);
  if (!tv_begin.tv_sec)
    tv_begin = tv1;
  
  tv2 = tv1;
  sub_timer(&diff, &tv2, &tv1);

  for (nuopt = 0, uopp = uopt1; nuopt < 4 && uopp;
       nuopt++, uopp = uopp->next) {
    tried[nuopt] = succeeded[nuopt] = 0;
    uopt[nuopt] = uopp;
  }
  
  i = 0;
  live_uopt = nuopt;
  while (live_uopt) {

    if (uopt[i]) {
      /* How many packets should we have sent by now? */
      int need = diff.tv_sec * uopt[i]->rate;
      need += (diff.tv_usec * 1000) / uopt[i]->ngap;
      
      /* Send one if we've fallen behind. */
      if (need > succeeded[i]) {
	if (write_cached_udp_packet(uopt[i]) >= 0)
	  succeeded[i]++;
	tried[i]++;
	
	if (succeeded[i] >= uopt[i]->rate) {
	  uopt[i] = 0;
	  live_uopt--;
	}
      }
    }

    /* get time at least once through every loop */
    do_gettimeofday(&tv2);
    sub_timer(&diff, &tv2, &tv1);
    if (diff.tv_sec > 2) break;
    
    i++;
    if (i >= nuopt) i = 0;
  }

  for (i = 0, uopp = uopt1; i < 4 && uopp; i++, uopp = uopp->next) {
    uopp->total_packets_attempted += tried[i];
    uopp->total_packets_sent += succeeded[i];
  }

  time--;
  if (time <= 0) {
    do_gettimeofday(&tv_end);
    sub_timer(&diff, &tv_end, &tv_begin);
    for (i = 0, uopp = uopt1; i < 4 && uopp; i++, uopp = uopp->next) {
      printk("<1>udpgen: [%d] %d packets sent (%d attempted) in %d.%06d s\n",
	     i, uopp->total_packets_sent, uopp->total_packets_attempted,
	     (int)diff.tv_sec, (int)diff.tv_usec);
    }
    MOD_DEC_USE_COUNT;
  } else
    queue_task(&uopt1->tq, &tq_scheduler);
}


EXPORT_NO_SYMBOLS;

MODULE_PARM(data, "s");
MODULE_PARM(len, "i");
MODULE_PARM(rate, "1-4i");
MODULE_PARM(time, "i");
MODULE_PARM(checksum, "i");
MODULE_PARM(priority, "1-4i");
MODULE_PARM(tos, "1-4i");
MODULE_PARM(ttl, "1-4i");
MODULE_PARM(saddr, "s");
MODULE_PARM(daddr, "s");
MODULE_PARM(sport, "i");
MODULE_PARM(dport, "1-4i");
MODULE_PARM(showpacket, "i");

void cleanup_module(void);

int
init_module(void)
{
  int ans;
  struct udpgen_opt *uuuu = 0;

  for (ans = 0; rate[ans]; ans++) {
    struct udpgen_opt *u = new_udpgen_opt(ans);
    if (!u) goto bad;
    if (!uopt)
      uopt = u;
    else
      uuuu->next = u;
    uuuu = u;
  }
  
  /* output packet stuff */
  if (showpacket) {
    ans = write_cached_udp_packet(uopt);
    if (uopt->skb_cache) {
      const unsigned char *data = uopt->skb_cache->data;
      int len = uopt->skb_cache->len;
      char buf[180];
      int i;
      for (i = 0; i < len; i += 16) {
	int j, l = 16;
	if (i + l > len) l = len - i;
	for (j = 0; j < l; j++)
	  sprintf(buf + j*3 + (j/4), "%02x  ", data[i+j]);
	sprintf(buf + l*3 + (l/4), "\n");
	printk("<1>udpgen: %s", buf);
      }
    }
  }
  
  MOD_INC_USE_COUNT;
  queue_task(&uopt->tq, &tq_scheduler);
  return 0;

 bad:
  cleanup_module();
  return -EINVAL;
}

void
cleanup_module(void)
{
  while (uopt) {
    struct udpgen_opt *n = uopt->next;
    free_udpgen_opt(uopt);
    uopt = n;
  }
}
