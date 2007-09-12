// -*- mode: c++; c-basic-offset: 4 -*-
/*
 * fromdevice.{cc,hh} -- element reads packets live from network via pcap
 * Douglas S. J. De Couto, Eddie Kohler, John Jannotti
 *
 * Copyright (c) 1999-2000 Massachusetts Institute of Technology
 * Copyright (c) 2001 International Computer Science Institute
 * Copyright (c) 2005-2007 Regents of the University of California
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
#include "fromdevice.hh"
#include <click/error.hh>
#include <click/straccum.hh>
#include <click/confparse.hh>
#include <click/glue.hh>
#include <click/packet_anno.hh>
#include <click/standard/scheduleinfo.hh>
#include <click/userutils.hh>
#include <unistd.h>
#include <fcntl.h>

#ifndef __sun
#include <sys/ioctl.h>
#else
#include <sys/ioccom.h>
#endif

#if FROMDEVICE_LINUX
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

#include "fakepcap.hh"

CLICK_DECLS

FromDevice::FromDevice()
    : 
#if FROMDEVICE_LINUX
      _linux_fd(-1),
#endif
#if FROMDEVICE_PCAP
      _pcap(0), _pcap_task(this), _pcap_complaints(0),
#endif
      _count(0), _promisc(0), _snaplen(0)
{
}

FromDevice::~FromDevice()
{
}

int
FromDevice::configure(Vector<String> &conf, ErrorHandler *errh)
{
    bool promisc = false, outbound = false, sniffer = true;
    _snaplen = 2046;
    _force_ip = false;
    String bpf_filter, capture;
    if (cp_va_kparse(conf, this, errh,
		     "DEVNAME", cpkP+cpkM, cpString, &_ifname,
		     "PROMISC", cpkP, cpBool, &promisc,
		     "SNAPLEN", cpkP, cpUnsigned, &_snaplen,
		     "SNIFFER", 0, cpBool, &sniffer,
		     "FORCE_IP", 0, cpBool, &_force_ip,
		     "CAPTURE", 0, cpWord, &capture,
		     "BPF_FILTER", 0, cpString, &bpf_filter,
		     "OUTBOUND", 0, cpBool, &outbound,
		     cpEnd) < 0)
	return -1;
    if (_snaplen > 8190 || _snaplen < 14)
	return errh->error("maximum packet length out of range");
    
#if FROMDEVICE_PCAP
    _bpf_filter = bpf_filter;
#endif

    // set _capture
    if (capture == "") {
#if FROMDEVICE_PCAP && FROMDEVICE_LINUX
	_capture = (bpf_filter ? CAPTURE_PCAP : CAPTURE_LINUX);
#elif FROMDEVICE_LINUX
	_capture = CAPTURE_LINUX;
#elif FROMDEVICE_PCAP
	_capture = CAPTURE_PCAP;
#else
	return errh->error("this platform does not support any capture method");
#endif
    }
#if FROMDEVICE_LINUX
    else if (capture == "LINUX")
	_capture = CAPTURE_LINUX;
#endif
#if FROMDEVICE_PCAP
    else if (capture == "PCAP")
	_capture = CAPTURE_PCAP;
#endif
    else
	return errh->error("capture method '%s' not supported", capture.c_str());
    
    if (bpf_filter && _capture != CAPTURE_PCAP)
	errh->warning("not using PCAP capture method, BPF filter ignored");

    _sniffer = sniffer;
    _promisc = promisc;
    _outbound = outbound;
    return 0;
}

#if FROMDEVICE_LINUX
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
#endif /* FROMDEVICE_LINUX */

int
FromDevice::initialize(ErrorHandler *errh)
{
    if (!_ifname)
	return errh->error("interface not set");

#if FROMDEVICE_PCAP
    if (_capture == CAPTURE_PCAP) {
	assert(!_pcap);
	char *ifname = _ifname.mutable_c_str();
	char ebuf[PCAP_ERRBUF_SIZE];
	_pcap = pcap_open_live(ifname, _snaplen, _promisc,
			       1,     /* timeout: don't wait for packets */
			       ebuf);
	// Note: pcap error buffer will contain the interface name
	if (!_pcap)
	    return errh->error("%s", ebuf);

	// nonblocking I/O on the packet socket so we can poll
	int pcap_fd = fd();
# if HAVE_PCAP_SETNONBLOCK
	if (pcap_setnonblock(_pcap, 1, ebuf) < 0)
	    errh->warning("pcap_setnonblock: %s", ebuf);
# else
	if (fcntl(pcap_fd, F_SETFL, O_NONBLOCK) < 0)
	    errh->warning("setting nonblocking: %s", strerror(errno));
# endif

# ifdef BIOCSSEESENT
	{
	    int r, accept = _outbound;
	    if ((r = ioctl(pcap_fd, BIOCSSEESENT, &accept)) == -1)
		return errh->error("%s: BIOCSSEESENT: %s", ifname, strerror(errno));
	    else if (r != 0)
		errh->warning("%s: BIOCSSEESENT returns %d", ifname, r);
	}
# endif

# if defined(BIOCIMMEDIATE) && !defined(__sun) // pcap/bpf ioctl, not in DLPI/bufmod
	{
	    int r, yes = 1;
	    if ((r = ioctl(pcap_fd, BIOCIMMEDIATE, &yes)) == -1)
		return errh->error("%s: BIOCIMMEDIATE: %s", ifname, strerror(errno));
	    else if (r != 0)
		errh->warning("%s: BIOCIMMEDIATE returns %d", ifname, r);
	}
# endif

	bpf_u_int32 netmask;
	bpf_u_int32 localnet;
	if (pcap_lookupnet(ifname, &localnet, &netmask, ebuf) < 0)
	    errh->warning("%s", ebuf);
  
	// Later versions of pcap distributed with linux (e.g. the redhat
	// linux pcap-0.4-16) want to have a filter installed before they
	// will pick up any packets.

	// compile the BPF filter
	struct bpf_program fcode;
	if (pcap_compile(_pcap, &fcode, _bpf_filter.mutable_c_str(), 0, netmask) < 0)
	    return errh->error("%s: %s", ifname, pcap_geterr(_pcap));
	if (pcap_setfilter(_pcap, &fcode) < 0)
	    return errh->error("%s: %s", ifname, pcap_geterr(_pcap));

	add_select(pcap_fd, SELECT_READ);

	_datalink = pcap_datalink(_pcap);
	if (_force_ip && !fake_pcap_dlt_force_ipable(_datalink))
	    errh->warning("%s: strange data link type %d, FORCE_IP will not work", ifname, _datalink);

	ScheduleInfo::initialize_task(this, &_pcap_task, false, errh);
    }
#endif

#if FROMDEVICE_LINUX
    if (_capture == CAPTURE_LINUX) {
	_linux_fd = open_packet_socket(_ifname, errh);
	if (_linux_fd < 0)
	    return -1;

	int promisc_ok = set_promiscuous(_linux_fd, _ifname, _promisc);
	if (promisc_ok < 0) {
	    if (_promisc)
		errh->warning("cannot set promiscuous mode");
	    _was_promisc = -1;
	} else
	    _was_promisc = promisc_ok;

	add_select(_linux_fd, SELECT_READ);

	_datalink = FAKE_DLT_EN10MB;
    }
#endif

    if (!_sniffer)
	device_filter(true, errh);
    
    return 0;
}

void
FromDevice::cleanup(CleanupStage stage)
{
    if (stage >= CLEANUP_INITIALIZED && !_sniffer)
	device_filter(false, ErrorHandler::default_handler());
#if FROMDEVICE_LINUX
    if (_linux_fd >= 0) {
	if (_was_promisc >= 0)
	    set_promiscuous(_linux_fd, _ifname, _was_promisc);
	close(_linux_fd);
	_linux_fd = -1;
    }
#endif
#if FROMDEVICE_PCAP
    if (_pcap) {
	pcap_close(_pcap);
	_pcap = 0;
    }
#endif
}

int
FromDevice::device_filter(bool add, ErrorHandler *errh)
{
    StringAccum cmda;
    cmda << "/sbin/iptables " << (add ? "-A" : "-D") << " INPUT -i "
	 << shell_quote(_ifname) << " -j DROP";
    String cmd = cmda.take_string();
    int before = errh->nerrors();
    String out = shell_command_output_string(cmd, "", errh);
    if (out)
	errh->error("%s: %s", cmd.c_str(), out.c_str());
    return errh->nerrors() == before ? 0 : -1;
}

#if FROMDEVICE_PCAP
CLICK_ENDDECLS
extern "C" {
void
FromDevice_get_packet(u_char* clientdata,
		      const struct pcap_pkthdr* pkthdr,
		      const u_char* data)
{
    static char bcast_addr[] = { 0xff, 0xff, 0xff, 0xff, 0xff, 0xff };

    FromDevice *fd = (FromDevice *) clientdata;
    int length = pkthdr->caplen;
#if defined(__sparc)
    // Packet::make(data,length) allocates new buffer to install
    // DEFAULT_HEADROOM (28 bytes). Thus data winds up on a 4 byte
    // boundary, irrespective of its original alignment. We assume we
    // want a two byte offset from a four byte boundary (DLT_EN10B).
    //
    // Furthermore, note that pcap-dlpi on Solaris uses bufmod by
    // default, hence while pcap-dlpi.pcap_read() is careful to load
    // the initial read from the stream head into a buffer aligned
    // appropriately for the network interface type, (I believe)
    // subsequent packets in the batched read will be copied from the
    // Stream's byte sequence into the pcap-dlpi user-level buffer at
    // arbitrary alignments.
    Packet *p = Packet::make(data - 2, length + 2);
    p->pull(2); 
#else
    Packet *p = Packet::make(data, length);
#endif

    // set packet type annotation
    if (p->data()[0] & 1) {
	if (memcmp(bcast_addr, p->data(), 6) == 0)
	    p->set_packet_type_anno(Packet::BROADCAST);
	else
	    p->set_packet_type_anno(Packet::MULTICAST);
    }

    // set annotations
    p->set_timestamp_anno(Timestamp::make_usec(pkthdr->ts.tv_sec, pkthdr->ts.tv_usec));
    p->set_mac_header(p->data());
    SET_EXTRA_LENGTH_ANNO(p, pkthdr->len - length);

    if (!fd->_force_ip || fake_pcap_force_ip(p, fd->_datalink))
	fd->output(0).push(p);
    else
	p->kill();
}
}
CLICK_DECLS
#endif

void
FromDevice::selected(int)
{
#if FROMDEVICE_PCAP
    if (_capture == CAPTURE_PCAP) {
	// Read and push() at most one packet.
	int r = pcap_dispatch(_pcap, 1, FromDevice_get_packet, (u_char *) this);
	if (r > 0)
	    _pcap_task.reschedule();
	else if (r < 0 && ++_pcap_complaints < 5)
	    ErrorHandler::default_handler()->error("%{element}: %s", this, pcap_geterr(_pcap));
    }
#endif
#if FROMDEVICE_LINUX
    if (_capture == CAPTURE_LINUX) {
	struct sockaddr_ll sa;
	socklen_t fromlen = sizeof(sa);
	// store data offset 2 bytes into the packet, assuming that first 14
	// bytes are ether header, and that we want remaining data to be
	// 4-byte aligned.  this assumes that _packetbuf is 4-byte aligned,
	// and that the buffer allocated by Packet::make is also 4-byte
	// aligned.  Actually, it doesn't matter if the packet is 4-byte
	// aligned; perhaps there is some efficiency aspect?  who cares....
	WritablePacket *p = Packet::make(2, 0, _snaplen, 0);
	int len = recvfrom(_linux_fd, p->data(), p->length(), MSG_TRUNC, (sockaddr *)&sa, &fromlen);
	if (len > 0 && (sa.sll_pkttype != PACKET_OUTGOING || _outbound)) {
	    if (len > _snaplen) {
		assert(p->length() == (uint32_t)_snaplen);
		SET_EXTRA_LENGTH_ANNO(p, len - _snaplen);
	    } else
		p->take(_snaplen - len);
	    p->set_packet_type_anno((Packet::PacketType)sa.sll_pkttype);
	    p->timestamp_anno().set_timeval_ioctl(_linux_fd, SIOCGSTAMP);
	    p->set_mac_header(p->data());
	    if (!_force_ip || fake_pcap_force_ip(p, _datalink))
		output(0).push(p);
	    else
		p->kill();
	} else {
	    p->kill();
	    if (len <= 0 && errno != EAGAIN)
		click_chatter("FromDevice(%s): recvfrom: %s", _ifname.c_str(), strerror(errno));
	}
    }
#endif
}

#if FROMDEVICE_PCAP
bool
FromDevice::run_task(Task *)
{
    // Read and push() at most one packet.
    int r = pcap_dispatch(_pcap, 1, FromDevice_get_packet, (u_char *) this);
    if (r > 0)
	_pcap_task.fast_reschedule();
    else if (r < 0 && ++_pcap_complaints < 5)
	ErrorHandler::default_handler()->error("%{element}: %s", this, pcap_geterr(_pcap));
    return r > 0;
}
#endif

void
FromDevice::kernel_drops(bool& known, int& max_drops) const
{
#if FROMDEVICE_LINUX
    // You might be able to do this better by parsing netstat/ifconfig output,
    // but for now, we just give up.
#endif
    known = false, max_drops = -1;
#if FROMDEVICE_PCAP
    if (_capture == CAPTURE_PCAP) {
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
    add_read_handler("kernel_drops", read_handler, (void *) 0);
    add_read_handler("encap", read_handler, (void *) 1);
    add_read_handler("count", read_handler, (void *) 2);
    add_write_handler("reset_counts", write_handler, 0);
}

CLICK_ENDDECLS
ELEMENT_REQUIRES(userlevel FakePcap)
EXPORT_ELEMENT(FromDevice)
