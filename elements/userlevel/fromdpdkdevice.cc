// -*- c-basic-offset: 4; related-file-name: "fromdpdkdevice.hh" -*-
/*
 * fromdpdkdevice.{cc,hh} -- element reads packets live from network via
 * the DPDK.
 *
 * Copyright (c) 2014-2015 Cyril Soldani, University of Liège
 * Copyright (c) 2016-2018 Tom Barbette, University of Liège
 * Copyright (c) 2017-2018 Georgios Katsikas, RISE SICS
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

#include <click/args.hh>
#include <click/error.hh>
#include <click/standard/scheduleinfo.hh>
#include <click/straccum.hh>

#include "fromdpdkdevice.hh"

CLICK_DECLS

FromDPDKDevice::FromDPDKDevice() :
    _dev(0), _queue_id(0), _promisc(true),
    _count(0), _active(true), _task(this)
{
    _burst_size = DPDKDevice::DEF_BURST_SIZE;
}

FromDPDKDevice::~FromDPDKDevice()
{
}

int FromDPDKDevice::configure(Vector<String> &conf, ErrorHandler *errh)
{
    int n_desc = -1;
    String dev;
    bool allow_nonexistent = false;

    if (Args(conf, this, errh)
        .read_mp("PORT", dev)
        .read_p("QUEUE", _queue_id)
        .read("PROMISC", _promisc)
        .read("BURST", _burst_size)
        .read("NDESC", n_desc)
        .read("ALLOW_NONEXISTENT", allow_nonexistent)
        .read("ACTIVE", _active)
        .complete() < 0)
        return -1;

    if (!DPDKDeviceArg::parse(dev, _dev)) {
        if (allow_nonexistent)
            return 0;
        else
            return errh->error("%s : Unknown or invalid PORT", dev.c_str());
    }

    return _dev->add_rx_queue(_queue_id, _promisc, (n_desc > 0) ?
                n_desc : DPDKDevice::DEF_DEV_RXDESC, errh);
}

int FromDPDKDevice::initialize(ErrorHandler *errh)
{
    if (!_dev)
        return 0;

    ScheduleInfo::initialize_task(this, &_task, true, errh);

    return DPDKDevice::initialize(errh);
}

void FromDPDKDevice::cleanup(CleanupStage)
{
}

bool FromDPDKDevice::run_task(Task * t)
{
    struct rte_mbuf *pkts[_burst_size];

    unsigned n = rte_eth_rx_burst(_dev->port_id, _queue_id, pkts, _burst_size);
    for (unsigned i = 0; i < n; ++i) {
        unsigned char* data = rte_pktmbuf_mtod(pkts[i], unsigned char *);
        rte_prefetch0(data);
        WritablePacket *p =
            Packet::make(data,
                         rte_pktmbuf_data_len(pkts[i]), DPDKDevice::free_pkt,
                         pkts[i],
                         rte_pktmbuf_headroom(pkts[i]),
                         rte_pktmbuf_tailroom(pkts[i]));
        p->set_packet_type_anno(Packet::HOST);
        p->set_mac_header(data);

        output(0).push(p);
    }
    _count += n;

    /* We reschedule directly, as we cannot know if there is actually packet
     * available and DPDK has no select mechanism*/
    t->fast_reschedule();

    return n;
}

String FromDPDKDevice::read_handler(Element *e, void * thunk)
{
    FromDPDKDevice *fd = static_cast<FromDPDKDevice *>(e);

    switch((uintptr_t) thunk) {
		case h_count:
			return String(fd->_count);
        case h_active:
              if (!fd->_dev)
                  return "false";
              else
                  return String(fd->_active);
        case h_device:
              if (!fd->_dev)
                  return "undefined";
              else
                  return String((int) fd->_dev->port_id);
        case h_nb_rx_queues:
            return String(fd->_dev->nbRXQueues());
        case h_nb_tx_queues:
            return String(fd->_dev->nbTXQueues());
        case h_mac: {
            if (!fd->_dev)
                return String::make_empty();
            return fd->_dev->get_mac().unparse();
        }
        case h_driver:
            return String(fd->_dev->get_device_driver());
    }

    return 0;
}

String FromDPDKDevice::status_handler(Element *e, void * thunk)
{
    FromDPDKDevice *fd = static_cast<FromDPDKDevice *>(e);
    struct rte_eth_link link;
    if (!fd->_dev) {
        return "0";
    }

    rte_eth_link_get_nowait(fd->_dev->port_id, &link);
#ifndef ETH_LINK_UP
    #define ETH_LINK_UP 1
#endif
    switch((uintptr_t) thunk) {
      case h_carrier:
          return (link.link_status == ETH_LINK_UP ? "1" : "0");
      case h_duplex:
          return (link.link_status == ETH_LINK_UP ?
            (link.link_duplex == ETH_LINK_FULL_DUPLEX ? "1" : "0") : "-1");
#if RTE_VERSION >= RTE_VERSION_NUM(16,04,0,0)
      case h_autoneg:
          return String(link.link_autoneg);
#endif
      case h_speed:
          return String(link.link_speed);
    }
    return 0;
}

String FromDPDKDevice::statistics_handler(Element *e, void *thunk)
{
    FromDPDKDevice *fd = static_cast<FromDPDKDevice *>(e);
    struct rte_eth_stats stats;
    if (!fd->_dev) {
        return "0";
    }

    if (rte_eth_stats_get(fd->_dev->port_id, &stats))
        return String::make_empty();

    switch((uintptr_t) thunk) {
        case h_ipackets:
            return String(stats.ipackets);
        case h_ibytes:
            return String(stats.ibytes);
        case h_imissed:
            return String(stats.imissed);
        case h_ierrors:
            return String(stats.ierrors);
        default:
            return "<unknown>";
    }
}

int FromDPDKDevice::write_handler(
        const String &input, Element *e, void *thunk, ErrorHandler *errh) {
    FromDPDKDevice *fd = static_cast<FromDPDKDevice *>(e);
    if (!fd->_dev) {
        return -1;
    }

    switch((uintptr_t) thunk) {
        case h_add_mac: {
            EtherAddress mac;
            int pool = 0;
            int ret;
            if (!EtherAddressArg().parse(input, mac)) {
                return errh->error("Invalid MAC address %s",input.c_str());
            }

            ret = rte_eth_dev_mac_addr_add(
                fd->_dev->port_id,
                reinterpret_cast<ether_addr*>(mac.data()), pool
            );
            if (ret != 0) {
                return errh->error("Could not add mac address !");
            }
            return 0;
        }
        case h_active: {
            bool active;
            if (!BoolArg::parse(input,active))
                return errh->error("Not a valid boolean");
            if (fd->_active != active) {
                fd->_active = active;
                if (fd->_active)
                    fd->_task.reschedule();
                else
                    fd->_task.unschedule();
            }
            return 0;
        }
        case h_reset_count:
            fd->_count = 0;
			return 0;
    }
    return -1;
}


int FromDPDKDevice::xstats_handler(
        int operation, String& input, Element* e,
        const Handler *handler, ErrorHandler* errh) {
    FromDPDKDevice *fd = static_cast<FromDPDKDevice *>(e);
    if (!fd->_dev)
        return -1;

        struct rte_eth_xstat_name* names;
#if RTE_VERSION >= RTE_VERSION_NUM(16,07,0,0)
        int len = rte_eth_xstats_get_names(fd->_dev->port_id, 0, 0);
        names = static_cast<struct rte_eth_xstat_name*>(
            malloc(sizeof(struct rte_eth_xstat_name) * len)
        );
        rte_eth_xstats_get_names(fd->_dev->port_id,names,len);
        struct rte_eth_xstat* xstats;
        xstats = static_cast<struct rte_eth_xstat*>(malloc(
            sizeof(struct rte_eth_xstat) * len)
        );
        rte_eth_xstats_get(fd->_dev->port_id,xstats,len);
        if (input == "") {
            StringAccum acc;
            for (int i = 0; i < len; i++) {
                acc << names[i].name << "["<<
                xstats[i].id << "] = " <<
                xstats[i].value << "\n";
            }

            input = acc.take_string();
        } else {
            for (int i = 0; i < len; i++) {
                if (strcmp(names[i].name,input.c_str()) == 0) {
                    input = String(xstats[i].value);
                    return 0;
                }
            }
            return -1;
        }
#else
        input = "unsupported with DPDK < 16.07";
        return -1;
#endif
    return 0;
}

void FromDPDKDevice::add_handlers()
{
    add_read_handler("count", read_handler, h_count);
    add_write_handler("reset_count", write_handler, h_reset_count,
                          Handler::BUTTON);

    add_read_handler("device",read_handler, h_device);

    add_read_handler("duplex",status_handler, h_duplex);
#if RTE_VERSION >= RTE_VERSION_NUM(16,04,0,0)
    add_read_handler("autoneg",status_handler, h_autoneg);
#endif
    add_read_handler("speed",status_handler, h_speed);
    add_read_handler("carrier",status_handler, h_carrier);
    add_read_handler("driver", read_handler, h_driver);

    set_handler("xstats", Handler::f_read | Handler::f_read_param, xstats_handler);

    add_read_handler("active", read_handler, h_active);
    add_write_handler("active", write_handler, h_active, Handler::BUTTON);

    add_read_handler("nb_rx_queues",read_handler, h_nb_rx_queues);
    add_read_handler("nb_tx_queues",read_handler, h_nb_tx_queues);

    add_read_handler("mac",read_handler, h_mac);
    add_write_handler("add_mac",write_handler, h_add_mac, 0);
    add_write_handler("remove_mac",write_handler, h_remove_mac, 0);

    add_read_handler("hw_count",statistics_handler, h_ipackets);
    add_read_handler("hw_bytes",statistics_handler, h_ibytes);
    add_read_handler("hw_dropped",statistics_handler, h_imissed);
    add_read_handler("hw_errors",statistics_handler, h_ierrors);

    add_data_handlers("burst", Handler::h_read | Handler::h_write, &_burst_size);
}

CLICK_ENDDECLS
ELEMENT_REQUIRES(userlevel dpdk)
EXPORT_ELEMENT(FromDPDKDevice)
ELEMENT_MT_SAFE(FromDPDKDevice)
