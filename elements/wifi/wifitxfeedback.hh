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

=s storage, wifi

When used in conjunction with a wifi-enabled kernel (the
hostap or airo drivers), this element pushes out packets
with the WIFI_TX_SUCCESS_ANNO set.

This element MUST be used in conjunction with a MSQueue,
because the push call runs during an interrupt.

For example, the following can be used:

WifiTXFeedback -> MSQueue(10) -> ...

If only one output exists, it sends all packets it receives to
that output.
If a second one exists, it sends sucessful packets to the first output
and failures to the second.

=a
AutoTXRate, SetTXRate

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
    
    void notify_noutputs(int);
    
    int got_skb(struct sk_buff *);
    static int static_got_skb(struct sk_buff *, void *arg);
    static String static_print_stats(Element *e, void *);
private:
    
    void tx_completed(Packet *p);

    int _successes;
    int _failures;
};

CLICK_ENDDECLS
#endif

