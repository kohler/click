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
#include <click/hashtable.hh>
#include <click/vector.hh>
#include <click/args.hh>

CLICK_DECLS
class DPDKDeviceArg;

class DPDKDevice {
public:

    unsigned port_id;

    DPDKDevice() : port_id(-1), info() {
    }

    DPDKDevice(unsigned port_id) : port_id(port_id) {
    } CLICK_COLD;

    int add_rx_queue(int &queue_id, bool promisc,
                             unsigned n_desc, ErrorHandler *errh) CLICK_COLD;

    int add_tx_queue(int &queue_id, unsigned n_desc,
                             ErrorHandler *errh) CLICK_COLD;

    unsigned int get_nb_txdesc();

    static struct rte_mempool *get_mpool(unsigned int);

    static int get_port_numa_node(unsigned port_id);

    static int initialize(ErrorHandler *errh);

    inline static bool is_dpdk_packet(Packet* p) {
            return p->buffer_destructor() == DPDKDevice::free_pkt || (p->data_packet() && is_dpdk_packet(p->data_packet()));
    }

    inline static rte_mbuf* get_pkt(unsigned numa_node);
    inline static rte_mbuf* get_pkt();
    static void free_pkt(unsigned char *, size_t, void *pktmbuf);

    static int NB_MBUF;
    static int MBUF_DATA_SIZE;
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

    DevInfo info;

    static bool _is_initialized;
    static HashTable<unsigned, DPDKDevice> _devs;
    static struct rte_mempool** _pktmbuf_pools;
    static bool no_more_buffer_msg_printed;

    int initialize_device(ErrorHandler *errh) CLICK_COLD;
    int add_queue(Dir dir, int &queue_id, bool promisc,
                   unsigned n_desc, ErrorHandler *errh) CLICK_COLD;

    static bool alloc_pktmbufs() CLICK_COLD;

    static DPDKDevice* get_device(unsigned port_id) {
       return &(_devs.find_insert(port_id, DPDKDevice(port_id)).value());
    }

    static int get_port_from_pci(uint16_t domain, uint8_t bus, uint8_t dev_id, uint8_t function) {
       struct rte_eth_dev_info dev_info;
       for (uint8_t port_id = 0 ; port_id < rte_eth_dev_count(); ++port_id) {
          rte_eth_dev_info_get(port_id, &dev_info);
          struct rte_pci_addr addr = dev_info.pci_dev->addr;
          if (addr.domain   == domain &&
              addr.bus      == bus &&
              addr.devid    == dev_id &&
              addr.function == function)
              return port_id;
       }
       return -1;
    }

    friend class DPDKDeviceArg;
};

inline rte_mbuf* DPDKDevice::get_pkt(unsigned numa_node) {
    struct rte_mbuf* mbuf = rte_pktmbuf_alloc(get_mpool(numa_node));
    if (unlikely(!mbuf)) {
        if (!DPDKDevice::no_more_buffer_msg_printed)
            click_chatter("No more DPDK buffer available ! Try using "
                               "DPDKInfo to allocate more.");
        else
            DPDKDevice::no_more_buffer_msg_printed = true;
    }
    return mbuf;
}

inline rte_mbuf* DPDKDevice::get_pkt() {
    return get_pkt(rte_socket_id());
}

/** @class DPDKPortArg
  @brief Parser class for DPDK Port, either an integer or a PCI address. */
class DPDKDeviceArg { public:
    static bool parse(const String &str, DPDKDevice* &result, const ArgContext &args = ArgContext());
    static String unparse(DPDKDevice* dev) {
        return String(dev->port_id);
    }
};

template<> struct DefaultArg<DPDKDevice*> : public DPDKDeviceArg {};


CLICK_ENDDECLS

#endif
