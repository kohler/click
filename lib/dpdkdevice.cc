/*
 * dpdkdevice.{cc,hh} -- library for interfacing with DPDK
 * Cyril Soldani, Tom Barbette, Georgios Katsikas
 *
 * Copyright (c) 2014-2018 University of Liege
 * Copyright (c) 2016 Cisco Meraki
 * Copyright (c) 2017-2018 RISE SICS
 * Copyright (c) 2018-2019 KTH Royal Institute of Technology
 *
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files (the "Software"),
 * to deal in the Software without restriction, subject to the conditions
 * listed in the Click LICENSE file. These conditions include: you must
 * preserve this copyright notice, and you cannot mention the copyright
 * holders in advertising related to the Software without their permission.
 * The Software is provided WITHOUT ANY WARRANTY, EXPRESS OR IMPLIED. This
 * notice is a summary of the Click LICENSE file; the license in that file is
 * legally binding.
 */

#include <click/config.h>
#include <click/dpdkdevice.hh>
#include <rte_errno.h>

CLICK_DECLS

DPDKDevice::DPDKDevice() : port_id(-1), info() {
}

DPDKDevice::DPDKDevice(portid_t port_id) : port_id(port_id) {
};

int DPDKDevice::nbRXQueues() {
    return info.rx_queues.size();
};

int DPDKDevice::nbTXQueues() {
    return info.tx_queues.size();
};

const char *DPDKDevice::get_device_driver()
{
    return info.driver;
}

/* Wraps rte_eth_dev_socket_id(), which may return -1 for valid ports when NUMA
 * is not well supported. This function will return 0 instead in that case. */
int DPDKDevice::get_port_numa_node(portid_t port_id)
{
    if (port_id >= dev_count())
        return -1;
    int numa_node = rte_eth_dev_socket_id(port_id);
    return (numa_node == -1) ? 0 : numa_node;
}

unsigned int DPDKDevice::get_nb_txdesc()
{
    return info.n_tx_descs;
}

bool DPDKDevice::alloc_pktmbufs()
{
    // Count NUMA sockets
    int max_socket = -1;
    for (HashTable<portid_t, DPDKDevice>::const_iterator it = _devs.begin();
         it != _devs.end(); ++it) {
        int numa_node = DPDKDevice::get_port_numa_node(it.key());
        if (numa_node > max_socket)
            max_socket = numa_node;
    }

    if (max_socket == -1)
        return false;

    // Allocate pktmbuf_pool array
    _nr_pktmbuf_pools = max_socket + 1;
    typedef struct rte_mempool *rte_mempool_p;
    _pktmbuf_pools = new rte_mempool_p[_nr_pktmbuf_pools];
    if (!_pktmbuf_pools)
        return false;
    memset(_pktmbuf_pools, 0, (max_socket + 1) * sizeof(rte_mempool_p));

    // Create a pktmbuf pool for each active socket
    for (HashTable<portid_t, DPDKDevice>::const_iterator it = _devs.begin();
         it != _devs.end(); ++it) {
        int numa_node = DPDKDevice::get_port_numa_node(it.key());
        if (!_pktmbuf_pools[numa_node]) {
            char name[64];
            snprintf(name, 64, "mbuf_pool_%u", numa_node);
            _pktmbuf_pools[numa_node] =
                rte_mempool_create(
                    name, NB_MBUF, MBUF_SIZE, MBUF_CACHE_SIZE,
                    sizeof (struct rte_pktmbuf_pool_private),
                    rte_pktmbuf_pool_init, NULL, rte_pktmbuf_init, NULL,
                    numa_node, 0);
            if (!_pktmbuf_pools[numa_node])
                return false;
        }
    }

    return true;
}

struct rte_mempool *DPDKDevice::get_mpool(unsigned int socket_id) {
    return _pktmbuf_pools[socket_id];
}

int DPDKDevice::initialize_device(ErrorHandler *errh)
{
    struct rte_eth_conf dev_conf;
    struct rte_eth_dev_info dev_info;
    memset(&dev_conf, 0, sizeof dev_conf);

    rte_eth_dev_info_get(port_id, &dev_info);

    info.driver = dev_info.driver_name;

#if RTE_VERSION >= RTE_VERSION_NUM(18,02,0,0) && RTE_VERSION < RTE_VERSION_NUM(18,11,0,0)
    dev_conf.rxmode.offloads = DEV_RX_OFFLOAD_CRC_STRIP;
    dev_conf.txmode.offloads = 0;
#endif
    dev_conf.rxmode.mq_mode = ETH_MQ_RX_RSS;
    dev_conf.rx_adv_conf.rss_conf.rss_key = NULL;
    dev_conf.rx_adv_conf.rss_conf.rss_hf = ETH_RSS_IP | ETH_RSS_UDP | ETH_RSS_TCP;

    //We must open at least one queue per direction
    if (info.rx_queues.size() == 0) {
        info.rx_queues.resize(1);
        info.n_rx_descs = DEF_DEV_RXDESC;
    }
    if (info.tx_queues.size() == 0) {
        info.tx_queues.resize(1);
    }

#if RTE_VERSION >= RTE_VERSION_NUM(18,05,0,0)
    if (info.n_rx_descs == 0)
        info.n_rx_descs = dev_info.default_rxportconf.ring_size > 0? dev_info.default_rxportconf.ring_size : DEF_DEV_RXDESC;

    if (info.n_tx_descs == 0)
        info.n_tx_descs = dev_info.default_txportconf.ring_size > 0? dev_info.default_txportconf.ring_size : DEF_DEV_TXDESC;
#else
    if (info.n_rx_descs == 0)
        info.n_rx_descs = DEF_DEV_RXDESC;

    if (info.n_tx_descs == 0)
        info.n_tx_descs = DEF_DEV_TXDESC;
#endif

    if (info.rx_queues.size() > dev_info.max_rx_queues) {
        return errh->error("Port %d can only use %d RX queues (asked for %d), use MAXQUEUES to set the maximum "
                           "number of queues or N_QUEUES to strictly define it.", port_id, dev_info.max_rx_queues, info.rx_queues.size());
    }
    if (info.tx_queues.size() > dev_info.max_tx_queues) {
        return errh->error("Port %d can only use %d TX queues (FastClick asked for %d, probably to serve that same amount of threads).\n"
                           "Add the argument \"MAXQUEUES %d\" to the corresponding ToDPDKDevice to set the maximum "
                           "number of queues to %d or \"N_QUEUES %d\" to strictly define it. "
                           "If the TX device has more threads than queues due to this parameter change, it will automatically rely on locking to share the queues as evenly as possible between the threads.", port_id, dev_info.max_tx_queues, info.tx_queues.size(), dev_info.max_tx_queues, dev_info.max_tx_queues, dev_info.max_tx_queues);
    }

    if (info.n_rx_descs < dev_info.rx_desc_lim.nb_min || info.n_rx_descs > dev_info.rx_desc_lim.nb_max) {
        return errh->error("The number of receive descriptors is %d but needs to be between %d and %d",info.n_rx_descs, dev_info.rx_desc_lim.nb_min, dev_info.rx_desc_lim.nb_max);
    }

    if (info.n_tx_descs < dev_info.tx_desc_lim.nb_min || info.n_tx_descs > dev_info.tx_desc_lim.nb_max) {
        return errh->error("The number of transmit descriptors is %d but needs to be between %d and %d",info.n_tx_descs, dev_info.tx_desc_lim.nb_min, dev_info.tx_desc_lim.nb_max);
    }

    int ret;
    if ((ret = rte_eth_dev_configure(
            port_id, info.rx_queues.size(),
            info.tx_queues.size(), &dev_conf)) < 0)
        return errh->error(
            "Cannot initialize DPDK port %u with %u RX and %u TX queues\nError %d : %s",
            port_id, info.rx_queues.size(), info.tx_queues.size(),
            ret, strerror(ret));

    rte_eth_dev_info_get(port_id, &dev_info);

#if RTE_VERSION >= RTE_VERSION_NUM(16,07,0,0)
    if (dev_info.nb_rx_queues != info.rx_queues.size()) {
        return errh->error("Device only initialized %d RX queues instead of %d. "
                "Please check configuration.", dev_info.nb_rx_queues,
                info.rx_queues.size());
    }
    if (dev_info.nb_tx_queues != info.tx_queues.size()) {
        return errh->error("Device only initialized %d TX queues instead of %d. "
                "Please check configuration.", dev_info.nb_tx_queues,
                info.tx_queues.size());
    }
#endif

    struct rte_eth_rxconf rx_conf;
#if RTE_VERSION >= RTE_VERSION_NUM(2,0,0,0)
    memcpy(&rx_conf, &dev_info.default_rxconf, sizeof rx_conf);
#else
    bzero(&rx_conf,sizeof rx_conf);
#endif

#if RTE_VERSION < RTE_VERSION_NUM(18,8,0,0)
    rx_conf.rx_thresh.pthresh = RX_PTHRESH;
    rx_conf.rx_thresh.hthresh = RX_HTHRESH;
    rx_conf.rx_thresh.wthresh = RX_WTHRESH;
#endif

#if RTE_VERSION >= RTE_VERSION_NUM(18,02,0,0)
    rx_conf.offloads = dev_conf.rxmode.offloads;
#endif

    struct rte_eth_txconf tx_conf;
    tx_conf = dev_info.default_txconf;
#if RTE_VERSION >= RTE_VERSION_NUM(2,0,0,0)
    memcpy(&tx_conf, &dev_info.default_txconf, sizeof tx_conf);
#else
    bzero(&tx_conf,sizeof tx_conf);
#endif

#if RTE_VERSION < RTE_VERSION_NUM(18,8,0,0) && RTE_VERSION >= RTE_VERSION_NUM(18,02,0,0)
    tx_conf.txq_flags = ETH_TXQ_FLAGS_IGNORE;
#else
    tx_conf.tx_thresh.pthresh = TX_PTHRESH;
    tx_conf.tx_thresh.hthresh = TX_HTHRESH;
    tx_conf.tx_thresh.wthresh = TX_WTHRESH;
#endif
#if RTE_VERSION >= RTE_VERSION_NUM(18,02,0,i0)
    tx_conf.offloads = dev_conf.txmode.offloads;
#endif
#if RTE_VERSION <= RTE_VERSION_NUM(18,05,0,0)
    tx_conf.txq_flags |= ETH_TXQ_FLAGS_NOMULTSEGS | ETH_TXQ_FLAGS_NOOFFLOADS;
#endif

    int numa_node = DPDKDevice::get_port_numa_node(port_id);
    for (unsigned i = 0; i < (unsigned)info.rx_queues.size(); ++i) {
        if (rte_eth_rx_queue_setup(
                port_id, i, info.n_rx_descs, numa_node, &rx_conf,
                _pktmbuf_pools[numa_node]) != 0)
            return errh->error(
                "Cannot initialize RX queue %u of port %u on node %u : %s",
                i, port_id, numa_node, rte_strerror(rte_errno));
    }

    for (unsigned i = 0; i < (unsigned)info.tx_queues.size(); ++i)
        if (rte_eth_tx_queue_setup(port_id, i, info.n_tx_descs, numa_node,
                                   &tx_conf) != 0)
            return errh->error(
                "Cannot initialize TX queue %u of port %u on node %u",
                i, port_id, numa_node);

    if (info.init_mtu != 0) {
        if (rte_eth_dev_set_mtu(port_id, info.init_mtu) != 0) {
            return errh->error("Could not set MTU %d",info.init_mtu);
        }
    }

    int err = rte_eth_dev_start(port_id);
    if (err < 0)
        return errh->error(
            "Cannot start DPDK port %u: error %d", port_id, err);

    if (info.promisc)
        rte_eth_promiscuous_enable(port_id);

    if (info.init_mac != EtherAddress()) {
        struct rte_ether_addr addr;
        memcpy(&addr,info.init_mac.data(),sizeof(struct rte_ether_addr));
        if (rte_eth_dev_default_mac_addr_set(port_id, &addr) != 0) {
            return errh->error("Could not set default MAC address");
        }
    }

    return 0;
}

void DPDKDevice::set_init_mac(EtherAddress mac) {
    assert(!_is_initialized);
    info.init_mac = mac;
}

void DPDKDevice::set_init_mtu(uint16_t mtu) {
    assert(!_is_initialized);
    info.init_mtu = mtu;
}

EtherAddress DPDKDevice::get_mac() {
    assert(_is_initialized);
    struct rte_ether_addr addr;
    rte_eth_macaddr_get(port_id,&addr);
    return EtherAddress((unsigned char*)&addr);
}

/**
 * Set v[id] to true in vector v, expanding it if necessary. If id is 0,
 * the first available slot will be taken.
 * If v[id] is already true, this function return false. True if it is a
 *   new slot or if the existing slot was false.
 */
bool set_slot(Vector<bool> &v, unsigned &id) {
    if (id <= 0) {
        unsigned i;
        for (i = 0; i < (unsigned)v.size(); i ++) {
            if (!v[i]) break;
        }
        id = i;
        if (id >= (unsigned)v.size())
            v.resize(id + 1, false);
    }
    if (id >= (unsigned)v.size()) {
        v.resize(id + 1,false);
    }
    if (v[id])
        return false;
    v[id] = true;
    return true;
}

int DPDKDevice::add_queue(DPDKDevice::Dir dir,
                           unsigned &queue_id, bool promisc, unsigned n_desc,
                           ErrorHandler *errh)
{
    if (_is_initialized) {
        return errh->error(
            "Trying to configure DPDK device after initialization");
    }

    if (dir == RX) {
        if (info.rx_queues.size() > 0 && promisc != info.promisc)
            return errh->error(
                "Some elements disagree on whether or not device %u should"
                " be in promiscuous mode", port_id);
        info.promisc |= promisc;
        if (n_desc > 0) {
            if (n_desc != info.n_rx_descs && info.rx_queues.size() > 0)
                return errh->error(
                        "Some elements disagree on the number of RX descriptors "
                        "for device %u", port_id);
            info.n_rx_descs = n_desc;
        }
        if (!set_slot(info.rx_queues, queue_id))
            return errh->error(
                        "Some elements are assigned to the same RX queue "
                        "for device %u", port_id);
    } else {
        if (n_desc > 0) {
            if (n_desc != info.n_tx_descs && info.tx_queues.size() > 0)
                return errh->error(
                        "Some elements disagree on the number of TX descriptors "
                        "for device %u", port_id);
            info.n_tx_descs = n_desc;
        }
        if (!set_slot(info.tx_queues,queue_id))
            return errh->error(
                        "Some elements are assigned to the same TX queue "
                        "for device %u", port_id);
    }

    return 0;
}

int DPDKDevice::add_rx_queue(unsigned &queue_id, bool promisc,
                              unsigned n_desc, ErrorHandler *errh)
{
    return add_queue(DPDKDevice::RX, queue_id, promisc, n_desc, errh);
}

int DPDKDevice::add_tx_queue(unsigned &queue_id, unsigned n_desc,
                              ErrorHandler *errh)
{
    return add_queue(DPDKDevice::TX, queue_id, false, n_desc, errh);
}

int DPDKDevice::initialize(ErrorHandler *errh)
{
    if (_is_initialized)
        return 0;

    if (!dpdk_enabled)
        return errh->error( "Supply the --dpdk argument to use DPDK.");

    click_chatter("Initializing DPDK");
#if RTE_VERSION < RTE_VERSION_NUM(2,0,0,0)
    if (rte_eal_pci_probe())
        return errh->error("Cannot probe the PCI bus");
#endif

    const unsigned n_ports = dev_count();
    if (n_ports == 0)
        return errh->error("No DPDK-enabled ethernet port found");

    for (HashTable<portid_t, DPDKDevice>::const_iterator it = _devs.begin();
         it != _devs.end(); ++it)
        if (it.key() >= n_ports)
            return errh->error("Cannot find DPDK port %u", it.key());

    if (!alloc_pktmbufs())
        return errh->error("Could not allocate packet MBuf pools");

    for (HashTable<portid_t, DPDKDevice>::iterator it = _devs.begin();
         it != _devs.end(); ++it) {
        int ret = it.value().initialize_device(errh);
        if (ret < 0)
            return ret;
    }

    _is_initialized = true;
    return 0;
}

void DPDKDevice::free_pkt(unsigned char *, size_t, void *pktmbuf)
{
    rte_pktmbuf_free((struct rte_mbuf *) pktmbuf);
}


bool
DPDKDeviceArg::parse(
    const String &str, DPDKDevice* &result, const ArgContext &ctx)
{
    portid_t port_id;

    if (!IntArg().parse(str, port_id)) {
#if RTE_VERSION >= RTE_VERSION_NUM(18,05,0,0)
       uint16_t id;
       if (rte_eth_dev_get_port_by_name(str.c_str(), &id) != 0)
           return false;
       else
           port_id = id;
#else
       //Try parsing a ffff:ff:ff.f format. Code adapted from EtherAddressArg::parse
        unsigned data[4];
        int d = 0, p = 0;
        const char *s, *end = str.end();

        for (s = str.begin(); s != end; ++s) {
           int digit;
           if (*s >= '0' && *s <= '9')
             digit = *s - '0';
           else if (*s >= 'a' && *s <= 'f')
             digit = *s - 'a' + 10;
           else if (*s >= 'A' && *s <= 'F')
             digit = *s - 'A' + 10;
           else {
             if (((*s == ':' && d < 2) ||
                (*s == '.' && d == 2)) &&
                (p == 1 || (d < 3 && p == 2) || (d == 0 && (p == 3 || p == 4)))
                && d < 3) {
               p = 0;
               ++d;
               continue;
             } else
               break;
           }

           if ((d == 0 && p == 4) || (d > 0 && p == 2)||
                (d == 3 && p == 1) || d == 4)
               break;

           data[d] = (p ? data[d] << 4 : 0) + digit;
           ++p;
        }

        if (s == end && p != 0 && d != 3) {
            ctx.error("invalid id or invalid PCI address format");
            return false;
        }

        port_id = DPDKDevice::get_port_from_pci(
            data[0], data[1], data[2], data[3]
        );
#endif
    }

    if (port_id >= 0 && port_id < DPDKDevice::dev_count()) {
        result = DPDKDevice::get_device(port_id);
    } else {
        ctx.error("Cannot resolve PCI address to DPDK device");
        return false;
    }

    return true;
}

int DPDKDevice::NB_MBUF = 65536;
#ifdef RTE_MBUF_DEFAULT_BUF_SIZE
int DPDKDevice::MBUF_DATA_SIZE = RTE_MBUF_DEFAULT_BUF_SIZE;
#else
int DPDKDevice::MBUF_DATA_SIZE = 2048 + RTE_PKTMBUF_HEADROOM;
#endif
int DPDKDevice::MBUF_SIZE = MBUF_DATA_SIZE
                          + sizeof (struct rte_mbuf);
int DPDKDevice::MBUF_CACHE_SIZE = 256;
int DPDKDevice::RX_PTHRESH = 8;
int DPDKDevice::RX_HTHRESH = 8;
int DPDKDevice::RX_WTHRESH = 4;
int DPDKDevice::TX_PTHRESH = 36;
int DPDKDevice::TX_HTHRESH = 0;
int DPDKDevice::TX_WTHRESH = 0;
String DPDKDevice::MEMPOOL_PREFIX = "click_mempool_";

unsigned DPDKDevice::DEF_DEV_RXDESC = 256;
unsigned DPDKDevice::DEF_DEV_TXDESC = 256;

unsigned DPDKDevice::DEF_BURST_SIZE = 32;

bool DPDKDevice::_is_initialized = false;
HashTable<portid_t, DPDKDevice> DPDKDevice::_devs;
struct rte_mempool** DPDKDevice::_pktmbuf_pools;
unsigned DPDKDevice::_nr_pktmbuf_pools;
bool DPDKDevice::no_more_buffer_msg_printed = false;

CLICK_ENDDECLS
