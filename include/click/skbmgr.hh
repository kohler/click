// -*- c-basic-offset: 2; related-file-name: "../../linuxmodule/skbmgr.cc" -*-
#ifndef CLICK_SKBMGR_HH
#define CLICK_SKBMGR_HH

void skbmgr_init();
void skbmgr_cleanup();

/* allocate skbs. Number of skbs allocated is stored in the want variable */
struct sk_buff *skbmgr_allocate_skbs(unsigned headroom, unsigned size, int *want);

/* recycle skb back into pool. If dirty == 0, these skbs were never used
   and do not need to be reinitialized */
void skbmgr_recycle_skbs(struct sk_buff *, int dirty);

#endif
