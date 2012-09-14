#ifndef CLICK_ETHERPAUSESOURCE_HH
#define CLICK_ETHERPAUSESOURCE_HH
#include <click/element.hh>
#include <click/timer.hh>
CLICK_DECLS

/*
 * =c
 * EtherPauseSource(SRC, PAUSETIME [, I<keywords> DST, LIMIT, INTERVAL, ACTIVE])
 *
 * =s ethernet
 * creates and emits Ethernet 802.3x pause frames
 * =d
 *
 * Ethernet pause frames can be used for hardware flow control
 * if the receiving device supports them.  A pause frame specifies
 * a requested PAUSETIME for how long the receiving device should pause its
 * transmission.  This pause time is measured in pause quanta, where a pause
 * quantum equals the time it takes to
 * transmit 512 bits with the current link speed. For instance,
 * on a 1Gbps link, a quantum of 195 is equavalent to pause of 100
 * microseconds.  The rate of Ethernet pause frames and the pause time
 * can be used to achieve a desired I<aggregate> link rate.
 * SRC must equal the address of the device sending the pause frames.
 *
 * In pull mode, EtherPauseSource emits a packet per pull request.  In push
 * mode, it generates a packet every INTERVAL seconds (with millisecond
 * precision).  INTERVAL defaults to 1 second.
 *
 * Keyword arguments are:
 *
 * =over 8
 *
 * =item DST
 *
 * Ethernet address.  Destination address of the PAUSE frames.  Defaults to
 * 01-80-C2-00-00-01, the special multicast address allocated for PAUSE
 * frames.  Can also be a specific station's address.
 *
 * =item LIMIT
 *
 * Integer.  The maximum number of packets to emit.  If -1, emits packets
 * forever.  Defaults to -1.
 *
 * =item ACTIVE
 *
 * Boolean.  If false, does not emit packets.  Defaults to true.
 *
 * =back
 *
 * =h count read-only
 * Returns the total number of packets that have been generated.
 * =h src read/write
 * Returns or sets the Ethernet source address.
 * =h dst read/write
 * Returns or sets the Ethernet destination address.
 * =h pausetime read/write
 * Returns or sets the pause time value.
 * =h limit read/write
 * Returns or sets the LIMIT parameter.
 * =h reset_counts write
 * Resets the packet count.  This may cause EtherPauseSource to generate LIMIT
 * more packets.
 * =h active read/write
 * Returns or sets the ACTIVE parameter.
 *
 * =e
 *  EtherPauseSource(00:1e:13:22:48:91, 390)
 *    -> ToDevice;
 */

class EtherPauseSource : public Element { public:

    static const unsigned NO_LIMIT = 0xFFFFFFFFU;

    EtherPauseSource() CLICK_COLD;
    ~EtherPauseSource() CLICK_COLD;

    const char *class_name() const	{ return "EtherPauseSource"; }
    const char *port_count() const	{ return PORTS_0_1; }

    int configure(Vector<String> &conf, ErrorHandler *errh) CLICK_COLD;
    int initialize(ErrorHandler *errh) CLICK_COLD;
    void cleanup(CleanupStage) CLICK_COLD;
    void add_handlers() CLICK_COLD;

    void run_timer(Timer *);
    Packet *pull(int);

  private:

    int _count;
    int _limit;
    bool _active;
    uint32_t _interval;
    Packet *_packet;
    Timer _timer;

    enum { h_limit, h_active, h_src, h_dst, h_pausetime, h_reset_counts };
    int rewrite_packet(const void *data, uint32_t offset, uint32_t size, ErrorHandler *errh);
    void check_awake();
    static String reader(Element *e, void *user_data);
    static int writer(const String &str, Element *e, void *user_data, ErrorHandler *errh);

};

CLICK_ENDDECLS
#endif
