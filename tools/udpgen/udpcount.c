/* udpcount -- count UDP packets.
 *
 * Module parameters:
 *
 * parameter	default		description
 * ---------	-------		-----------
 * saddr	0.0.0.0		count packets from this IP address
 *				(default all)
 * sport	0		count packets from this UDP port (default all)
 * dport	0		count packets to this UDP port (default all)
 *
 * Get information out of /proc/net/udpcount
 */

#include <linux/autoconf.h>
#ifdef CONFIG_SMP
# define __SMP__ 1
#endif
#if defined(CONFIG_MODVERSIONS) && !defined(MODVERSIONS)
# define MODVERSIONS 1
#endif
#ifdef MODVERSIONS
# include <linux/modversions.h>
#endif
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
#include <linux/proc_fs.h>

#define GRAB_EARLY 1

#ifdef GRAB_EARLY
/* called by dev.c/netif_rx() during receive interrupt */
extern int (*udpcount_hook)(struct sk_buff *skb);
#endif

/* defaults for udpgen_opt */
static char *saddr = "0.0.0.0";
static __u32 saddr_a;
static int sport = 0;
static int dport = 0;

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

static void
prepare(void)
{
  saddr_a = parse_ip_address(saddr);
  sport = htons(sport);
  dport = htons(dport);
}


static struct timeval last_time;

#ifdef USE_BINS

#define USEC_PER_BIN	10
#define BINS_PER_SEC	((int)(1000000 / USEC_PER_BIN))
#define NBINS		1500
static int bins[NBINS + 1];
static int total_in;

#else

static int in_by_tos[257];
static int time_by_tos[257];
static int time2_by_tos[257];

#endif


#ifndef GRAB_EARLY
static int (*old_udp_rcv)(struct sk_buff *, unsigned short);
static void (*old_udp_err)(struct sk_buff *, unsigned char *, int);
static int total_err_in;
static int total_unmatched;
#endif

static void
got_one(struct sk_buff *skb)
{
  struct timeval this_time, diff;
  do_gettimeofday(&this_time);

#ifdef USE_BINS
  
  if (last_time.tv_sec) {
    int bin;
    struct iphdr *ip = (struct iphdr *) skb->data;
    sub_timer(&diff, &this_time, &last_time);
    
    if (diff.tv_sec >= 5)
      bin = NBINS;
    else {
      int diff_usec = diff.tv_usec + diff.tv_sec*1000000;
      bin = diff_usec / USEC_PER_BIN;
      if (bin > NBINS) bin = NBINS;
    }
    
    bins[bin]++;
  }
  
  last_time = this_time;
  total_in++;

#else

  if (last_time.tv_sec) {
    struct iphdr *ip = (struct iphdr *) skb->data;
    int tos = ip->tos;
    
    sub_timer(&diff, &this_time, &last_time);
    if (diff.tv_sec < 1) {
      int diff_usec = diff.tv_usec + diff.tv_sec*1000000;
      in_by_tos[tos]++;
      time_by_tos[tos] += diff_usec;
      time2_by_tos[tos] += diff_usec * diff_usec;
    }
  }
  
  last_time = this_time;
  
#endif
}

#ifdef GRAB_EARLY
/*
 * Called at interrupt time from dev.c/netif_rx().
 */
int
udpcount_call(struct sk_buff *skb)
{
  struct iphdr *ip = (struct iphdr *) skb->data;

  /*
   * skb->mac.raw points to the ether header.
   * skb->data points to the IP or ARP or whatever packet.
   */

  if(skb->mac.ethernet->h_proto == htons(0x0800) &&
     ip->protocol == 17 &&
     ip->saddr == saddr_a){
    got_one(skb);
    kfree_skb(skb);
    return(1);
  } else {
    return(0); /* Linux should process the packet */
  }
}
#else
static int
my_udp_rcv(struct sk_buff *skb, unsigned short len)
{
  struct udphdr *uh = skb->h.uh;
  __u32 this_saddr = skb->nh.iph->saddr;
  __u16 this_sport = uh->source;
  __u16 this_dport = uh->dest;

  if ((!saddr_a || this_saddr == saddr_a)
      && (!sport || this_sport == sport)
      && (!dport || this_dport == dport)) {
    
    got_one(skb);
    kfree_skb(skb);
    return 0;
    
  } else {
    total_unmatched++;
    return old_udp_rcv(skb, len);
  }
}

static void
my_udp_rcv_err(struct sk_buff *skb, unsigned char *dp, int len)
{
  total_err_in++;
  old_udp_err(skb, dp, len);
}
#endif



/* proc stuff */

struct udpcount_proc_data {
  int length;
  char data[0];
};

#ifdef USE_BINS

static struct udpcount_proc_data *
prepare_proc_data()
{
  char *buf = kmalloc(1024, GFP_KERNEL);
  int pos = 4;
  int len = 1024;
  struct udpcount_proc_data *upd;
  int i, j, n;
  
  if (!buf)
    return 0;
  
  n = 0;
  for (i = 0; i < NBINS; i++)
    n += bins[i];

  pos += sprintf(buf + pos, "# %d packets\n\
# %d outlier packets\n\
# %d unmatched packets\n\
# %d ICMP errors\n\
# %d usec/bin\n\
#\n\
# BIN-CENTER-USEC BIN-COUNT\n", n, bins[NBINS], total_unmatched, total_err_in, USEC_PER_BIN);

  j = USEC_PER_BIN/2;
  for (i = 0; i < NBINS; i++, j += USEC_PER_BIN) {
    if (pos + 48 > len) {
      char *new_buf = kmalloc(len * 2, GFP_KERNEL);
      if (!new_buf) {
	kfree(buf);
	return 0;
      }
      memcpy(new_buf, buf, len);
      kfree(buf);
      buf = new_buf;
      len *= 2;
    }
    pos += sprintf(buf + pos, "%d %d\n", j, bins[i]);
  }

  upd = (struct udpcount_proc_data *)buf;
  upd->length = pos - 4;
  return upd;
}

#else

static struct udpcount_proc_data *
prepare_proc_data()
{
  char *buf = kmalloc(1024, GFP_KERNEL);
  int pos = 4;
  int len = 1024;
  struct udpcount_proc_data *upd;
  int i, n;
  
  if (!buf)
    return 0;
  
  n = 0;
  for (i = 0; i < 256; i++)
    n += in_by_tos[i];

  pos += sprintf(buf + pos, "# %d packets\n\
# TOS COUNT SUM_DELAY SUM_SQ_DELAY MEAN\n", n);

  for (i = 0; i < 256; i++)
    if (in_by_tos[i]) {
      if (pos + 48 > len) {
	char *new_buf = kmalloc(len * 2, GFP_KERNEL);
	if (!new_buf) {
	  kfree(buf);
	  return 0;
	}
	memcpy(new_buf, buf, len);
	kfree(buf);
	buf = new_buf;
	len *= 2;
      }
      pos += sprintf(buf + pos, "%d %d %d %d %d\n", i, in_by_tos[i],
		     time_by_tos[i], time2_by_tos[i], time_by_tos[i]/in_by_tos[i]);
    }
  
  upd = (struct udpcount_proc_data *)buf;
  upd->length = pos - 4;
  return upd;
}

#endif


static int
proc_open(struct inode *ino, struct file *filp)
{
  struct udpcount_proc_data *upd;
  if ((filp->f_flags & O_ACCMODE) == O_RDWR
      || (filp->f_flags & O_ACCMODE) == O_WRONLY)
    return -EACCES;
  
  upd = prepare_proc_data();
  
  if (upd) {
    filp->private_data = (void *)upd;
    MOD_INC_USE_COUNT;
    return 0;
  } else
    return -ENOMEM;
}

static ssize_t
proc_read(struct file *filp, char *buffer, size_t count, loff_t *f_pos_p)
{
  loff_t f_pos = *f_pos_p;
  struct udpcount_proc_data *s =
    (struct udpcount_proc_data *)filp->private_data;
  if (f_pos + count > s->length)
    count = s->length - f_pos;
  if (copy_to_user(buffer, s->data + f_pos, count) > 0)
    return -EFAULT;
  *f_pos_p += count;
  return count;
}

static int
proc_release(struct inode *ino, struct file *filp)
{
  kfree(filp->private_data);
  MOD_DEC_USE_COUNT;
  return 0;
}

static struct file_operations proc_operations = {
    NULL,			/* lseek */
    proc_read,			/* read */
    NULL,			/* write */
    NULL,			/* readdir */
    NULL,			/* select */
    NULL,			/* ioctl */
    NULL,			/* mmap */
    proc_open,			/* open */
    NULL,			/* flush */
    proc_release,		/* release */
    NULL			/* fsync */
};

static struct inode_operations proc_inode_operations;

static struct proc_dir_entry proc_entry = {
  0,				/* dynamic inode */
  8, "udpcount",		/* name */
  S_IFREG | S_IRUGO,
  1, 0, 0,			/* nlink, uid, gid */
  0, &proc_inode_operations,
  NULL, NULL,
  NULL, NULL, NULL,
};


/* rest */

EXPORT_NO_SYMBOLS;

MODULE_PARM(saddr, "s");
MODULE_PARM(sport, "i");
MODULE_PARM(dport, "i");

extern struct inet_protocol *inet_get_protocol(unsigned char);

int
init_module(void)
{
#ifdef USE_BINS
  int i;
  
  i = USEC_PER_BIN * BINS_PER_SEC;
  if (i != 1000000) {
    printk("<1>udpcount: BINS_PER_SEC must be divisor of 1e6 (%d)\n", i);
    return -EINVAL;
  }
#endif
  
  prepare();
  last_time.tv_sec = last_time.tv_usec = 0;

  /* register /proc/net/udpcount */
  proc_inode_operations = proc_dir_inode_operations;
  proc_inode_operations.default_file_ops = &proc_operations;
  proc_register(proc_net, &proc_entry);

#if GRAB_EARLY
  /* grab packets from dev.c/netif_rx() */
  udpcount_hook = udpcount_call;
#else
  /* grab packets relatively late in IP processing */
  {
    struct inet_protocol *udp = inet_get_protocol(IPPROTO_UDP);
    if (!udp) {
      printk("<1>udpcount: can't find UDP protocol\n");
      return -EINVAL;
    }

    old_udp_rcv = udp->handler;
    old_udp_err = udp->err_handler;
    udp->handler = my_udp_rcv;
    udp->err_handler = my_udp_rcv_err;
  }
#endif
  
  return 0;
}

void
cleanup_module(void)
{
  int i, n;
#ifdef USE_BINS
  int j, sum_usec, sum_sq_usec, mean;
#endif
  
#ifdef GRAB_EARLY
  udpcount_hook = 0;
#else
  struct inet_protocol *udp = inet_get_protocol(IPPROTO_UDP);
  if (udp) {
    udp->handler = old_udp_rcv;
    udp->err_handler = old_udp_err;
  } else {
    printk("<1>udpcount: error: no UDP second time through\n");
    printk("<1>udpcount: prepare to crash!\n");
  }
#endif

#ifdef USE_BINS
  /* determine statistics */
  n = sum_usec = sum_sq_usec = 0;
  for (i = 0, j = USEC_PER_BIN/2; i < NBINS; i++, j += USEC_PER_BIN) {
    n += bins[i];
    sum_usec += j*bins[i];
    sum_sq_usec += (j*j)*bins[i];
  }

  printk("<1>udpcount: %d packets, %d unmatched\n", n, total_unmatched);
  if (total_err_in)
    printk("<1>udpcount: %d ICMP errors\n", total_err_in);
  printk("<1>udpcount: %d usec/bin\n", USEC_PER_BIN);
  if (n) {
    mean = sum_usec/n;
    printk("<1>udpcount: %d sum_usec\n", sum_usec);
    printk("<1>udpcount: %d sum_sq_usec\n", sum_sq_usec);
    printk("<1>udpcount: %d approx_mean\n", mean);
  }
  printk("<1>udpcount: %d outliers\n", bins[NBINS]);

#else
  /* determine statistics */
  n = 0;
  for (i = 0; i < 256; i++)
    n += in_by_tos[i];

  printk("<1>udpcount: %d packets\n", n);
  for (i = 0; i < 256; i++)
    if (in_by_tos[i]) {
      printk("<1>udpcount: [%d] %d packets, %d sum, %d sum_sq, %d mean\n",
	     i, in_by_tos[i], time_by_tos[i], time2_by_tos[i],
	     time_by_tos[i]/in_by_tos[i]);
    }
  
#endif

  /* remove /proc/net/udpcount */
  proc_unregister(proc_net, proc_entry.low_ino);
}
