
#ifdef __KERNEL__
#ifndef SKBMGR_H
#define SKBMGR_H

struct sk_buff;

void skbmgr_init();
void skbmgr_cleanup();

// allocate skbs. number of skbs allocated is stored in the want variable
struct sk_buff* skbmgr_allocate_skbs(unsigned size, int *want);

// recycle skb back into pool, if clean is true, these skbs were never used
// and do not need to be reinitialized
void skbmgr_recycle_skbs(struct sk_buff*, int dirty);

#endif
#endif

