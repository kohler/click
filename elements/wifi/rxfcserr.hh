#ifndef CLICK_RXFCSERR_HH
#define CLICK_RXFCSERR_HH
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

class RXFCSErr : public Element { 
public:
    
    RXFCSErr();
    ~RXFCSErr();
    
    const char *class_name() const { return "RXFCSErr"; }
    const char *processing() const { return PUSH; }
    RXFCSErr *clone() const { return new RXFCSErr; }
    
    int configure(Vector<String> &, ErrorHandler *);
    int initialize(ErrorHandler *);
    void cleanup(CleanupStage);

    int configure_phase() const { return (CONFIGURE_PHASE_LAST-1); }
    
private:
    
    static int static_cb(struct sk_buff *, void *arg);
    void cb(struct sk_buff *);

};

CLICK_ENDDECLS
#endif

