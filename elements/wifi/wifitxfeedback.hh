// -*- mode: c++; c-basic-offset: 4 -*-
#ifndef CLICK_WIFITXFEEDBACK_HH
#define CLICK_WIFITXFEEDBACK_HH
#include <click/element.hh>
#include <click/atomic.hh>
#include <click/packet_anno.hh>
#include <click/packet.hh>
#include <click/timer.hh>
#include <click/glue.hh>
#include <click/element.hh>
#include <click/task.hh>
#include <click/standard/storage.hh>
CLICK_DECLS

/*
=c 

WifiTXFeedback

=s storage

*/

class WifiTXFeedback : public Element, public Storage { 
public:
    
    WifiTXFeedback();
    ~WifiTXFeedback();
    
    const char *class_name() const { return "WifiTXFeedback"; }
    const char *processing() const { return PUSH; }
    WifiTXFeedback *clone() const { return new WifiTXFeedback; }
    
    int configure(Vector<String> &, ErrorHandler *);
    int initialize(ErrorHandler *errh);
    void cleanup(CleanupStage);
    void add_handlers();
    int configure_phase() const { return (CONFIGURE_PHASE_LAST-1); }
    
    
    int got_skb(struct sk_buff *);
    static int static_got_skb(struct sk_buff *, void *arg);
    
private:
    
    void tx_completed(Packet *p);
    
};

CLICK_ENDDECLS
#endif

