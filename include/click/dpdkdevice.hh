#ifndef CLICK_DPDKDEVICE_HH
#define CLICK_DPDKDEVICE_HH

//Prevent bug under some configurations (like travis-ci's one) where these macros get undefined
#ifndef UINT8_MAX
#define UINT8_MAX 255
#endif
#ifndef UINT16_MAX
#define UINT16_MAX 65535
#endif

#include <rte_common.h>
#include <rte_eal.h>
#include <rte_ethdev.h>
#include <rte_lcore.h>
#include <rte_mbuf.h>
#include <rte_mempool.h>
#include <rte_pci.h>
#include <rte_version.h>

#include <click/packet.hh>
#include <click/error.hh>
#include <click/hashmap.hh>
#include <click/vector.hh>

CLICK_DECLS

class DPDKDevice {
public:

    static struct rte_mempool *get_mpool(unsigned int);

    static int get_port_numa_node(unsigned port_id);

    static int add_rx_device(unsigned port_id, int &queue_id, bool promisc,
                             unsigned n_desc, ErrorHandler *errh);

    static int add_tx_device(unsigned port_id, int &queue_id, unsigned n_desc,
                             ErrorHandler *errh);
    static int initialize(ErrorHandler *errh);

    inline static bool is_dpdk_packet(Packet* p) {
            return p->buffer_destructor() == DPDKDevice::free_pkt;
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
            rx_queues(0,false), tx_queues(0,false), promisc(false), n_rx_descs(0),
            n_tx_descs(0) {
            rx_queues.reserve(128);
            tx_queues.reserve(128);
        }

        Vector<bool> rx_queues;
        Vector<bool> tx_queues;
        bool promisc;
        unsigned n_rx_descs;
        unsigned n_tx_descs;
    };

    static bool _is_initialized;
    static HashMap<unsigned, DevInfo> _devs;
    static struct rte_mempool** _pktmbuf_pools;

    static int initialize_device(unsigned port_id, DevInfo &info,
                                 ErrorHandler *errh) CLICK_COLD;

    static bool alloc_pktmbufs() CLICK_COLD;

    static int add_device(unsigned port_id, Dir dir, int &queue_id,
                          bool promisc, unsigned n_desc, ErrorHandler *errh)
        CLICK_COLD;
};

CLICK_ENDDECLS

#endif
