// -*- mode: c++; c-basic-offset: 4 -*-
/*
 * fromdevice.{cc,hh} -- element reads packets live from network via pcap
 * Douglas S. J. De Couto, Eddie Kohler, John Jannotti
 *
 * Copyright (c) 1999-2000 Massachusetts Institute of Technology
 * Copyright (c) 2001 International Computer Science Institute
 * Copyright (c) 2005-2007 Regents of the University of California
 * Copyright (c) 2011 Meraki, Inc.
 * Copyright (c) 2012 Eddie Kohler
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
#include <sys/types.h>
#include <sys/time.h>
#if !defined(__sun)
# include <sys/ioctl.h>
#else
# include <sys/ioccom.h>
#endif
#if HAVE_NET_BPF_H
# include <net/bpf.h>
# define PCAP_DONT_INCLUDE_PCAP_BPF_H 1
#endif
#include "fromdevice.hh"
#include <click/etheraddress.hh>
#include <click/error.hh>
#include <click/straccum.hh>
#include <click/args.hh>
#include <click/glue.hh>
#include <click/packet_anno.hh>
#include <click/standard/scheduleinfo.hh>
#include <click/userutils.hh>
#include <unistd.h>
#include <fcntl.h>
#include "fakepcap.hh"

#if FROMDEVICE_ALLOW_LINUX
# include <sys/socket.h>
# include <net/if.h>
# include <features.h>
# if __GLIBC__ >= 2 && __GLIBC_MINOR__ >= 1
#  include <netpacket/packet.h>
#  include <net/ethernet.h>
# else
#  include <net/if_packet.h>
#  include <linux/if_packet.h>
#  include <linux/if_ether.h>
# endif
#endif

#if FROMDEVICE_ALLOW_NETMAP
# include <sys/mman.h>
#endif

CLICK_DECLS

FromDevice::FromDevice()
    :
#if FROMDEVICE_ALLOW_NETMAP || FROMDEVICE_ALLOW_PCAP
      _task(this),
#endif
#if FROMDEVICE_ALLOW_PCAP
      _pcap(0), _pcap_complaints(0),
#endif
      _datalink(-1), _count(0), _promisc(0), _snaplen(0)
{
#if FROMDEVICE_ALLOW_LINUX || FROMDEVICE_ALLOW_PCAP || FROMDEVICE_ALLOW_NETMAP
    _fd = -1;
#endif
}

FromDevice::~FromDevice()
{
}

int
FromDevice::configure(Vector<String> &conf, ErrorHandler *errh)
{
    bool promisc = false, outbound = false, sniffer = true, timestamp = true;
    _snaplen = default_snaplen;
    _headroom = Packet::default_headroom;
    _headroom += (4 - (_headroom + 2) % 4) % 4; // default 4/2 alignment
    _force_ip = false;
    _burst = 1;
    String bpf_filter, capture, encap_type;
    bool has_encap;
    if (Args(conf, this, errh)
	.read_mp("DEVNAME", _ifname)
	.read_p("PROMISC", promisc)
	.read_p("SNAPLEN", _snaplen)
	.read("SNIFFER", sniffer)
	.read("FORCE_IP", _force_ip)
	.read("METHOD", WordArg(), capture)
	.read("CAPTURE", WordArg(), capture) // deprecated
	.read("BPF_FILTER", bpf_filter)
	.read("OUTBOUND", outbound)
	.read("HEADROOM", _headroom)
	.read("ENCAP", WordArg(), encap_type).read_status(has_encap)
	.read("BURST", _burst)
	.read("TIMESTAMP", timestamp)
	.complete() < 0)
	return -1;
    if (_snaplen > 8190 || _snaplen < 14)
	return errh->error("SNAPLEN out of range");
    if (_headroom > 8190)
	return errh->error("HEADROOM out of range");
    if (_burst <= 0)
	return errh->error("BURST out of range");

#if FROMDEVICE_ALLOW_PCAP
    _bpf_filter = bpf_filter;
    if (has_encap) {
        _datalink = fake_pcap_parse_dlt(encap_type);
        if (_datalink < 0)
            return errh->error("bad encapsulation type");
    }
#endif

    // set _method
    if (capture == "") {
#if FROMDEVICE_ALLOW_NETMAP || FROMDEVICE_ALLOW_PCAP || FROMDEVICE_ALLOW_LINUX
# if FROMDEVICE_ALLOW_PCAP
	_method = _bpf_filter ? method_pcap : method_default;
# else
	_method = method_default;
# endif
#else
	return errh->error("cannot receive packets on this platform");
#endif
    }
#if FROMDEVICE_ALLOW_LINUX
    else if (capture == "LINUX")
	_method = method_linux;
#endif
#if FROMDEVICE_ALLOW_PCAP
    else if (capture == "PCAP")
	_method = method_pcap;
#endif
#if FROMDEVICE_ALLOW_NETMAP
    else if (capture == "NETMAP")
	_method = method_netmap;
#endif
    else
	return errh->error("bad METHOD");

    if (bpf_filter && _method != method_pcap)
	errh->warning("not using METHOD PCAP, BPF filter ignored");

    _sniffer = sniffer;
    _promisc = promisc;
    _outbound = outbound;
    _timestamp = timestamp;
    return 0;
}

#if FROMDEVICE_ALLOW_LINUX
int
FromDevice::open_packet_socket(String ifname, ErrorHandler *errh)
{
    int fd = socket(PF_PACKET, SOCK_RAW, htons(ETH_P_ALL));
    if (fd == -1)
	return errh->error("%s: socket: %s", ifname.c_str(), strerror(errno));

    // get interface index
    struct ifreq ifr;
    memset(&ifr, 0, sizeof(ifr));
    strncpy(ifr.ifr_name, ifname.c_str(), sizeof(ifr.ifr_name));
    int res = ioctl(fd, SIOCGIFINDEX, &ifr);
    if (res != 0) {
	close(fd);
	return errh->error("%s: SIOCGIFINDEX: %s", ifname.c_str(), strerror(errno));
    }
    int ifindex = ifr.ifr_ifindex;

    // bind to the specified interface.  from packet man page, only
    // sll_protocol and sll_ifindex fields are used; also have to set
    // sll_family
    sockaddr_ll sa;
    memset(&sa, 0, sizeof(sa));
    sa.sll_family = AF_PACKET;
    sa.sll_protocol = htons(ETH_P_ALL);
    sa.sll_ifindex = ifindex;
    res = bind(fd, (struct sockaddr *)&sa, sizeof(sa));
    if (res != 0) {
	close(fd);
	return errh->error("%s: bind: %s", ifname.c_str(), strerror(errno));
    }

    // nonblocking I/O on the packet socket so we can poll
    fcntl(fd, F_SETFL, O_NONBLOCK);

    return fd;
}

int
FromDevice::set_promiscuous(int fd, String ifname, bool promisc)
{
    // get interface flags
    struct ifreq ifr;
    memset(&ifr, 0, sizeof(ifr));
    strncpy(ifr.ifr_name, ifname.c_str(), sizeof(ifr.ifr_name));
    if (ioctl(fd, SIOCGIFFLAGS, &ifr) != 0)
	return -2;
    int was_promisc = (ifr.ifr_flags & IFF_PROMISC ? 1 : 0);

    // set or reset promiscuous flag
#ifdef SOL_PACKET
    if (ioctl(fd, SIOCGIFINDEX, &ifr) != 0)
	return -2;
    struct packet_mreq mr;
    memset(&mr, 0, sizeof(mr));
    mr.mr_ifindex = ifr.ifr_ifindex;
    mr.mr_type = (promisc ? PACKET_MR_PROMISC : PACKET_MR_ALLMULTI);
    if (setsockopt(fd, SOL_PACKET, PACKET_ADD_MEMBERSHIP, &mr, sizeof(mr)) < 0)
	return -3;
#else
    if (was_promisc != promisc) {
	ifr.ifr_flags = (promisc ? ifr.ifr_flags | IFF_PROMISC : ifr.ifr_flags & ~IFF_PROMISC);
	if (ioctl(fd, SIOCSIFFLAGS, &ifr) < 0)
	    return -3;
    }
#endif

    return was_promisc;
}
#endif /* FROMDEVICE_ALLOW_LINUX */

#if FROMDEVICE_ALLOW_PCAP
const char *
FromDevice::pcap_error(pcap_t *pcap, const char *ebuf)
{
    if ((!ebuf || !ebuf[0]) && pcap)
	ebuf = pcap_geterr(pcap);
    if (!ebuf || !ebuf[0])
	return "unknown error";
    else
	return ebuf;
}

pcap_t *
FromDevice::open_pcap(String ifname, int snaplen, bool promisc,
		      ErrorHandler *errh)
{
    char ebuf[PCAP_ERRBUF_SIZE];
    ebuf[0] = 0;
    pcap_t *pcap = pcap_open_live(ifname.mutable_c_str(), snaplen, promisc,
			       1,     /* timeout: don't wait for packets */
			       ebuf);

    // Note: pcap error buffer will contain the interface name
    if (!pcap) {
	errh->error("%s while opening %s", pcap_error(0, ebuf), ifname.c_str());
	return 0;
    } else if (ebuf[0])
	errh->warning("%s", ebuf);

    // nonblocking I/O on the packet socket so we can poll
# if HAVE_PCAP_SETNONBLOCK
    ebuf[0] = 0;
    if (pcap_setnonblock(pcap, 1, ebuf) < 0 || ebuf[0])
	errh->warning("pcap_setnonblock: %s", pcap_error(pcap, ebuf));
# else
    if (fcntl(pcap_fileno(pcap), F_SETFL, O_NONBLOCK) < 0)
	errh->warning("setting nonblocking: %s", strerror(errno));
# endif

    return pcap;
}
#endif

int
FromDevice::initialize(ErrorHandler *errh)
{
    if (!_ifname)
	return errh->error("interface not set");

#if FROMDEVICE_ALLOW_NETMAP
    if (_method == method_default || _method == method_netmap) {
	_fd = _netmap.open(_ifname, _method == method_netmap, errh);
	if (_fd >= 0) {
	    _datalink = FAKE_DLT_EN10MB;
	    _method = method_netmap;
	    _netmap.initialize_rings_rx(_timestamp);
	}
    }
#endif

#if FROMDEVICE_ALLOW_PCAP
    if (_method == method_default || _method == method_pcap) {
	assert(!_pcap);
	_pcap = open_pcap(_ifname, _snaplen, _promisc, errh);
	if (!_pcap)
	    return -1;
	_fd = pcap_fileno(_pcap);
	char *ifname = _ifname.mutable_c_str();

# if HAVE_PCAP_SETDIRECTION
	pcap_setdirection(_pcap, _outbound ? PCAP_D_INOUT : PCAP_D_IN);
# elif defined(BIOCSSEESENT)
	{
	    int r, accept = _outbound;
	    if ((r = ioctl(_fd, BIOCSSEESENT, &accept)) == -1)
		return errh->error("%s: BIOCSSEESENT: %s", ifname, strerror(errno));
	    else if (r != 0)
		errh->warning("%s: BIOCSSEESENT returns %d", ifname, r);
	}
# endif

# if defined(BIOCIMMEDIATE) && !defined(__sun) // pcap/bpf ioctl, not in DLPI/bufmod
	{
	    int r, yes = 1;
	    if ((r = ioctl(_fd, BIOCIMMEDIATE, &yes)) == -1)
		return errh->error("%s: BIOCIMMEDIATE: %s", ifname, strerror(errno));
	    else if (r != 0)
		errh->warning("%s: BIOCIMMEDIATE returns %d", ifname, r);
	}
# endif

        if (_datalink == -1) {  // no ENCAP specified in configure()
            _datalink = pcap_datalink(_pcap);
        } else {
            if (pcap_set_datalink(_pcap, _datalink) == -1)
                return errh->error("%s: pcap_set_datalink: %s", ifname, pcap_geterr(_pcap));
        }

	bpf_u_int32 netmask;
	bpf_u_int32 localnet;
	char ebuf[PCAP_ERRBUF_SIZE];
	ebuf[0] = 0;
	if (pcap_lookupnet(ifname, &localnet, &netmask, ebuf) < 0 || ebuf[0] != 0)
	    errh->warning("%s", pcap_error(ebuf));

	// Later versions of pcap distributed with linux (e.g. the redhat
	// linux pcap-0.4-16) want to have a filter installed before they
	// will pick up any packets.

	// compile the BPF filter
	struct bpf_program fcode;
	if (pcap_compile(_pcap, &fcode, _bpf_filter.mutable_c_str(), 0, netmask) < 0)
	    return errh->error("%s: %s", ifname, pcap_error(0));
	if (pcap_setfilter(_pcap, &fcode) < 0)
	    return errh->error("%s: %s", ifname, pcap_error(0));

	_datalink = pcap_datalink(_pcap);
	if (_force_ip && !fake_pcap_dlt_force_ipable(_datalink))
	    errh->warning("%s: strange data link type %d, FORCE_IP will not work", ifname, _datalink);

	_method = method_pcap;
    }
#endif

#if FROMDEVICE_ALLOW_LINUX
    if (_method == method_default || _method == method_linux) {
	_fd = open_packet_socket(_ifname, errh);
	if (_fd < 0)
	    return -1;

	int promisc_ok = set_promiscuous(_fd, _ifname, _promisc);
	if (promisc_ok < 0) {
	    if (_promisc)
		errh->warning("cannot set promiscuous mode");
	    _was_promisc = -1;
	} else
	    _was_promisc = promisc_ok;

	_datalink = FAKE_DLT_EN10MB;
	_method = method_linux;
    }
#endif

#if FROMDEVICE_ALLOW_PCAP || FROMDEVICE_ALLOW_NETMAP
    if (_method == method_pcap || _method == method_netmap)
	ScheduleInfo::initialize_task(this, &_task, false, errh);
#endif
#if FROMDEVICE_ALLOW_PCAP || FROMDEVICE_ALLOW_LINUX || FROMDEVICE_ALLOW_NETMAP
    if (_fd >= 0)
	add_select(_fd, SELECT_READ);
#endif

    if (!_sniffer)
	if (KernelFilter::device_filter(_ifname, true, errh) < 0)
	    _sniffer = true;

    return 0;
}

void
FromDevice::cleanup(CleanupStage stage)
{
    if (stage >= CLEANUP_INITIALIZED && !_sniffer)
	KernelFilter::device_filter(_ifname, false, ErrorHandler::default_handler());
#if FROMDEVICE_ALLOW_NETMAP
    if (_fd >= 0 && _method == method_netmap)
	_netmap.close(_fd);
#endif
#if FROMDEVICE_ALLOW_LINUX
    if (_fd >= 0 && _method == method_linux) {
	if (_was_promisc >= 0)
	    set_promiscuous(_fd, _ifname, _was_promisc);
	close(_fd);
    }
#endif
#if FROMDEVICE_ALLOW_PCAP
    if (_pcap)
	pcap_close(_pcap);
    _pcap = 0;
#endif
#if FROMDEVICE_ALLOW_NETMAP || FROMDEVICE_ALLOW_PCAP || FROMDEVICE_ALLOW_LINUX
    _fd = -1;
#endif
}

#if FROMDEVICE_ALLOW_PCAP || FROMDEVICE_ALLOW_NETMAP
void
FromDevice::emit_packet(WritablePacket *p, int extra_len, const Timestamp &ts)
{
    // set packet type annotation
    if (p->data()[0] & 1) {
	if (EtherAddress::is_broadcast(p->data()))
	    p->set_packet_type_anno(Packet::BROADCAST);
	else
	    p->set_packet_type_anno(Packet::MULTICAST);
    }

    // set annotations
    p->set_timestamp_anno(ts);
    p->set_mac_header(p->data());
    SET_EXTRA_LENGTH_ANNO(p, extra_len);

    if (!_force_ip || fake_pcap_force_ip(p, _datalink))
	output(0).push(p);
    else
	checked_output_push(1, p);
}
#endif

#if FROMDEVICE_ALLOW_PCAP
CLICK_ENDDECLS
extern "C" {
void
FromDevice_get_packet(u_char* clientdata,
		      const struct pcap_pkthdr* pkthdr,
		      const u_char* data)
{
    FromDevice *fd = (FromDevice *) clientdata;
    WritablePacket *p = Packet::make(fd->_headroom, data, pkthdr->caplen, 0);
    fd->emit_packet(p, pkthdr->len - pkthdr->caplen,
		    Timestamp::make_usec(pkthdr->ts.tv_sec, pkthdr->ts.tv_usec));
}
}
CLICK_DECLS
#endif

#if FROMDEVICE_ALLOW_NETMAP
int
FromDevice::netmap_dispatch()
{
    int n = 0;
    for (unsigned ri = _netmap.ring_begin; ri != _netmap.ring_end; ++ri) {
	struct netmap_ring *ring = NETMAP_RXRING(_netmap.nifp, ri);
	//click_chatter("netmap dispatch %s %u %u %u %u", _ifname.c_str(), ri, ring->cur, ring->tail, ring->head);

	while (NetmapInfo::nm_ring_reserved(ring) > 0 && NetmapInfo::refill(ring))
	   ;// click_chatter("Refilled") ;

	if (nm_ring_space(ring) == 0)
	    continue;

	int nzcopy = (int) (ring->num_slots / 2) - (int) NetmapInfo::nm_ring_reserved(ring);
	while (n != _burst && NetmapInfo::nm_ring_has_avail(ring)) {
	    unsigned cur = ring->cur;
	    unsigned buf_idx = ring->slot[cur].buf_idx;
	    if (buf_idx < 2)
		break;
	    unsigned char *buf = (unsigned char *) NETMAP_BUF(ring, buf_idx);
	    WritablePacket *p;
	    if (nzcopy > 0) {
		p = Packet::make(buf, ring->slot[cur].len, NetmapInfo::buffer_destructor);
	    } else {
		p = Packet::make(_headroom, buf, ring->slot[cur].len, 0);
		unsigned res1idx = NETMAP_RING_FIRST_RESERVED(ring);

		ring->slot[res1idx].buf_idx = buf_idx;
		ring->slot[res1idx].flags |= NS_BUF_CHANGED;
	    }

	    nzcopy = NetmapInfo::nm_advance(ring,nzcopy);

	    ++n;

	    emit_packet(p, 0, ring->ts);
	}
    }
    return n;
}
#endif

void
FromDevice::selected(int, int)
{
#if FROMDEVICE_ALLOW_NETMAP
    if (_method == method_netmap) {
	int r = netmap_dispatch();
	if (r > 0) {
	    _count += r;
	    _task.reschedule();
	}
    }
#endif
#if FROMDEVICE_ALLOW_PCAP
    if (_method == method_pcap) {
	// Read and push() at most one burst of packets.
	int r = pcap_dispatch(_pcap, _burst, FromDevice_get_packet, (u_char *) this);
	if (r > 0) {
	    _count += r;
	    _task.reschedule();
	} else if (r < 0 && ++_pcap_complaints < 5)
	    ErrorHandler::default_handler()->error("%p{element}: %s", this, pcap_geterr(_pcap));
    }
#endif
#if FROMDEVICE_ALLOW_LINUX
    int nlinux = 0;
    while (_method == method_linux && nlinux < _burst) {
	struct sockaddr_ll sa;
	socklen_t fromlen = sizeof(sa);
	WritablePacket *p = Packet::make(_headroom, 0, _snaplen, 0);
	int len = recvfrom(_fd, p->data(), p->length(), MSG_TRUNC, (sockaddr *)&sa, &fromlen);
	if (len > 0 && (sa.sll_pkttype != PACKET_OUTGOING || _outbound)) {
	    if (len > _snaplen) {
		assert(p->length() == (uint32_t)_snaplen);
		SET_EXTRA_LENGTH_ANNO(p, len - _snaplen);
	    } else
		p->take(_snaplen - len);
	    p->set_packet_type_anno((Packet::PacketType)sa.sll_pkttype);
	    p->timestamp_anno().set_timeval_ioctl(_fd, SIOCGSTAMP);
	    p->set_mac_header(p->data());
	    ++nlinux;
	    ++_count;
	    if (!_force_ip || fake_pcap_force_ip(p, _datalink))
		output(0).push(p);
	    else
		checked_output_push(1, p);
	} else {
	    p->kill();
	    if (len <= 0 && errno != EAGAIN)
		click_chatter("FromDevice(%s): recvfrom: %s", _ifname.c_str(), strerror(errno));
	    break;
	}
    }
#endif
}

#if FROMDEVICE_ALLOW_PCAP || FROMDEVICE_ALLOW_NETMAP
bool
FromDevice::run_task(Task *)
{
    // Read and push() at most one burst of packets.
    int r = 0;
# if FROMDEVICE_ALLOW_NETMAP
    if (_method == method_netmap)
	r = netmap_dispatch();
# endif
# if FROMDEVICE_ALLOW_PCAP
    if (_method == method_pcap) {
	r = pcap_dispatch(_pcap, _burst, FromDevice_get_packet, (u_char *) this);
	if (r < 0 && ++_pcap_complaints < 5)
	    ErrorHandler::default_handler()->error("%p{element}: %s", this, pcap_geterr(_pcap));
    }
# endif
    if (r > 0) {
	_count += r;
	_task.fast_reschedule();
	return true;
    } else
	return false;
}
#endif

void
FromDevice::kernel_drops(bool& known, int& max_drops) const
{
#if FROMDEVICE_ALLOW_LINUX
    // You might be able to do this better by parsing netstat/ifconfig output,
    // but for now, we just give up.
#endif
    known = false, max_drops = -1;
#if FROMDEVICE_ALLOW_PCAP
    if (_method == method_pcap) {
	struct pcap_stat stats;
	if (pcap_stats(_pcap, &stats) >= 0)
	    known = true, max_drops = stats.ps_drop;
    }
#endif
}

String
FromDevice::read_handler(Element* e, void *thunk)
{
    FromDevice* fd = static_cast<FromDevice*>(e);
    if (thunk == (void *) 0) {
	int max_drops;
	bool known;
	fd->kernel_drops(known, max_drops);
	if (known)
	    return String(max_drops);
	else if (max_drops >= 0)
	    return "<" + String(max_drops);
	else
	    return "??";
    } else if (thunk == (void *) 1)
	return String(fake_pcap_unparse_dlt(fd->_datalink));
    else
	return String(fd->_count);
}

int
FromDevice::write_handler(const String &, Element *e, void *, ErrorHandler *)
{
    FromDevice* fd = static_cast<FromDevice*>(e);
    fd->_count = 0;
    return 0;
}

void
FromDevice::add_handlers()
{
    add_read_handler("kernel_drops", read_handler, 0);
    add_read_handler("encap", read_handler, 1);
    add_read_handler("count", read_handler, 2);
    add_write_handler("reset_counts", write_handler, 0, Handler::BUTTON);
}

CLICK_ENDDECLS
ELEMENT_REQUIRES(userlevel FakePcap KernelFilter NetmapInfo)
EXPORT_ELEMENT(FromDevice)
