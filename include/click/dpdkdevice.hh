#ifndef CLICK_DPDKDEVICE_HH
#define CLICK_DPDKDEVICE_HH

#include <click/packet.hh>
#include <click/error.hh>
#include <click/hashmap.hh>

CLICK_DECLS

class DpdkDevice {
public:

    static struct rte_mempool *get_mpool(unsigned int);

    static int get_port_numa_node(unsigned port_id);

    static int add_rx_device(unsigned port_id, int &queue_id, bool promisc,
                             unsigned n_desc, ErrorHandler *errh);

    static int add_tx_device(unsigned port_id, int &queue_id, unsigned n_desc,
                             ErrorHandler *errh);
    static int initialize(ErrorHandler *errh);

    inline static bool is_dpdk_packet(Packet* p) {
            return p->buffer_destructor() == DpdkDevice::free_pkt;
    }

    static void free_pkt(unsigned char *, size_t, void *pktmbuf);

    static void fake_free_pkt(unsigned char *, size_t, void *pktmbuf);

    static unsigned int get_nb_txdesc(unsigned port_id);

    static int NB_MBUF;
    static int MBUF_SIZE;
    static int MBUF_CACHE_SIZE;
    static int RX_PTHRESH;
    static int RX_HTHRESH;
    static int RX_WTHRESH;
    static int TX_PTHRESH;
    static int TX_HTHRESH;
    static int TX_WTHRESH;

private:

    enum Dir { RX, TX };

    struct DevInfo {
        inline DevInfo() :
            n_rx_queues(0), n_tx_queues(0), promisc(false), n_rx_descs(0),
            n_tx_descs(0) {}
        inline DevInfo(DpdkDevice::Dir dir, unsigned queue_id, bool promisc,
                       unsigned n_desc) :
            n_rx_queues((dir == DpdkDevice::RX) ? queue_id + 1 : 0),
            n_tx_queues((dir == DpdkDevice::TX) ? queue_id + 1 : 0),
            promisc(promisc),
            n_rx_descs((dir == DpdkDevice::RX) ? n_desc : 256),
            n_tx_descs((dir == DpdkDevice::TX) ? n_desc : 1024)
            {}

        unsigned n_rx_queues;
        unsigned n_tx_queues;
        bool promisc;
        unsigned n_rx_descs;
        unsigned n_tx_descs;
    };

    static bool _is_initialized;
    static HashMap<unsigned, DevInfo> _devs;
    static struct rte_mempool** _pktmbuf_pools;

    static int initialize_device(unsigned port_id, const DevInfo &info,
                                 ErrorHandler *errh) CLICK_COLD;

    static bool alloc_pktmbufs() CLICK_COLD;

    static int add_device(unsigned port_id, Dir dir, int &queue_id,
                          bool promisc, unsigned n_desc, ErrorHandler *errh)
        CLICK_COLD;
};

CLICK_ENDDECLS

#endif
