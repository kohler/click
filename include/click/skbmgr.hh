// -*- c-basic-offset: 2; related-file-name: "../../linuxmodule/skbmgr.cc" -*-
#ifndef CLICK_SKBMGR_HH
#define CLICK_SKBMGR_HH
CLICK_DECLS

void skbmgr_init();
void skbmgr_cleanup();

/* allocate skbs. Number of skbs allocated is stored in the want variable */
struct sk_buff *skbmgr_allocate_skbs(unsigned headroom, unsigned size, int *want);

/* recycle skb back into pool */
void skbmgr_recycle_skbs(struct sk_buff *);

CLICK_ENDDECLS
#endif
