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
    void cleanup();
    void add_handlers();
    
    
    int got_skb(struct sk_buff *);
    static int static_got_skb(struct sk_buff *, void *arg);
    
    bool run_task();
    
private:
    
    void tx_completed(Packet *p);
    
    unsigned _burst;
    unsigned _drops;
    
    Task _task;

    enum { QSIZE = 511 };
    Packet *_queue[QSIZE+1];
};

CLICK_ENDDECLS
#endif

