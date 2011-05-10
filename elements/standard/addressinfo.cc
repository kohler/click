// -*- c-basic-offset: 4; related-file-name: "../../include/click/standard/addressinfo.hh" -*-
/*
 * addressinfo.{cc,hh} -- element stores address information
 * Eddie Kohler
 *
 * Copyright (c) 2000 Mazu Networks, Inc.
 * Copyright (c) 2004 The Regents of the University of California
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
#include <click/standard/addressinfo.hh>
#include <click/nameinfo.hh>
#include <click/glue.hh>
#include <click/confparse.hh>
#include <click/router.hh>
#include <click/error.hh>
#include <clicknet/ether.h>
#include <click/etheraddress.hh>
#if CLICK_NS
# include <click/master.hh>
#endif
#if CLICK_USERLEVEL
# include <unistd.h>
#endif
#if CLICK_USERLEVEL && defined(__linux__)
# include <net/if.h>
# include <sys/ioctl.h>
# include <net/if_arp.h>
# include <click/userutils.hh>
# include <time.h>
#elif CLICK_USERLEVEL && (defined(__APPLE__) || defined(__FreeBSD__))
# include <sys/sysctl.h>
# include <net/if.h>
# include <net/if_dl.h>
# include <net/if_types.h>
# include <net/route.h>
#endif
#if CLICK_LINUXMODULE
# include <click/cxxprotect.h>
CLICK_CXX_PROTECT
# include <linux/netdevice.h>
# include <linux/rtnetlink.h>
# include <linux/if_arp.h>
# include <linux/inetdevice.h>
# if LINUX_VERSION_CODE >= KERNEL_VERSION(2, 6, 24)
#  include <net/net_namespace.h>
# endif
CLICK_CXX_UNPROTECT
# include <click/cxxunprotect.h>
#endif
CLICK_DECLS

AddressInfo::AddressInfo()
{
}

AddressInfo::~AddressInfo()
{
}

int
AddressInfo::configure(Vector<String> &conf, ErrorHandler *errh)
{
    enum { t_eth = 1, t_ip4 = 2, t_ip4net = 4, t_ip6 = 8, t_ip6net = 16 };
    int before = errh->nerrors();

    for (int i = 0; i < conf.size(); i++) {
	Vector<String> parts;
	cp_spacevec(conf[i], parts);
	if (parts.size() == 0)
	    // allow empty arguments
	    continue;
	if (parts.size() < 2)
	    errh->error("expected %<NAME [ADDRS]%>");
	int types = 0;

	for (int j = 1; j < parts.size(); j++) {
	    struct in_addr ip4[2];
	    unsigned char ether[6];
# if HAVE_IP6
	    struct {
		unsigned char c[sizeof(click_in6_addr)];
		int p;
	    } ip6;
# endif

	    int my_types = 0;
	    if (EtherAddressArg().parse(parts[j], ether))
		my_types |= t_eth;
	    if (IPAddressArg().parse(parts[j], ip4[0]))
		my_types |= t_ip4;
	    else if (IPPrefixArg().parse(parts[j], ip4[0], ip4[1])) {
		my_types |= t_ip4net;
		if (ip4[0].s_addr & ~ip4[1].s_addr)
		    my_types |= t_ip4;
	    }
#if HAVE_IP6
	    if (cp_ip6_address(parts[j], ip6.c))
		my_types |= t_ip6;
	    else if (cp_ip6_prefix(parts[j], ip6.c, &ip6.p, false)) {
		my_types |= t_ip6net;
		if (IP6Address(ip6.c) & IP6Address::make_inverted_prefix(ip6.p))
		    my_types |= t_ip6;
	    }
#endif

	    bool one_type = (my_types & (my_types - 1)) == 0;
	    if ((my_types & t_eth) && (one_type || !(types & t_eth)))
		NameInfo::define(NameInfo::T_ETHERNET_ADDR, this, parts[0], ether, 6);
	    if ((my_types & t_ip4) && (one_type || !(types & t_ip4)))
		NameInfo::define(NameInfo::T_IP_ADDR, this, parts[0], &ip4[0], 4);
	    if ((my_types & t_ip4net) && (one_type || !(types & t_ip4net)))
		NameInfo::define(NameInfo::T_IP_PREFIX, this, parts[0], &ip4[0], 8);
#if HAVE_IP6
	    if ((my_types & t_ip6) && (one_type || !(types & t_ip6)))
		NameInfo::define(NameInfo::T_IP6_ADDR, this, parts[0], ip6.c, 16);
	    if ((my_types & t_ip6net) && (one_type || !(types & t_ip6net)))
		NameInfo::define(NameInfo::T_IP6_PREFIX, this, parts[0], &ip6, 16 + sizeof(int));
#endif

	    types |= my_types;
	    if (!my_types)
		errh->error("\"%s\" %<%s%> is not a recognizable address", parts[0].c_str(), parts[j].c_str());
	}
    }

    return (errh->nerrors() == before ? 0 : -1);
}


#if CLICK_USERLEVEL && !CLICK_NS && (defined(__linux__) || defined(__APPLE__) || defined(__FreeBSD__))
# define CLICK_QUERY_NETDEVICE 1
#endif

#if CLICK_QUERY_NETDEVICE

static bool
query_netdevice(const String &s, unsigned char *store, int type, int len)
    // type: should be 'e' (Ethernet) or 'i' (ipv4)
{
    // 5 Mar 2004 - Don't call ioctl for every attempt to look up an Ethernet
    // device name, because this causes the kernel to try to load weird kernel
    // modules.
    static time_t read_time = 0;
    static Vector<String> device_names;
    static Vector<String> device_addrs;

    // XXX magic time constant
    if (!read_time || read_time + 30 < time(0)) {
	device_names.clear();
	device_addrs.clear();

# ifdef __linux__
	int query_fd = socket(AF_INET, SOCK_DGRAM, 0);
	if (query_fd < 0)
	    return false;
	struct ifreq ifr;

	String f = file_string("/proc/net/dev");
	const char *begin = f.begin(), *end = f.end();
	while (begin < end) {
	    const char *colon = find(begin, end, ':');
	    const char *nl = find(begin, end, '\n');
	    if (colon > begin && colon < nl) {
		const char *word = colon;
		while (word > begin && !isspace((unsigned char) word[-1]))
		    word--;
		if ((size_t) (colon - word) < sizeof(ifr.ifr_name)) {
		    // based on patch from Jose Vasconcellos
		    // <jvasco@bellatlantic.net>
		    String dev_name = f.substring(word, colon);
		    strcpy(ifr.ifr_name, dev_name.c_str());
		    if (ioctl(query_fd, SIOCGIFHWADDR, &ifr) >= 0
			&& ifr.ifr_hwaddr.sa_family == ARPHRD_ETHER) {
			device_names.push_back(dev_name);
			device_addrs.push_back(String('e') + String(ifr.ifr_hwaddr.sa_data, 6));
		    }
		    if (ioctl(query_fd, SIOCGIFADDR, &ifr) >= 0) {
			device_names.push_back(dev_name);
			device_addrs.push_back(String('i') + String((const char *)&((struct sockaddr_in *)&ifr.ifr_addr)->sin_addr, 4));
		    }
		}
	    }
	    begin = nl + 1;
	}

	close(query_fd);
# elif defined(__APPLE__) || defined(__FreeBSD__)
	// get list of interfaces (this code borrowed, with changes, from
	// FreeBSD ifconfig(8))
	int mib[8];
	mib[0] = CTL_NET;
	mib[1] = PF_ROUTE;
	mib[2] = 0;
	mib[3] = 0;		// address family
	mib[4] = NET_RT_IFLIST;
	mib[5] = 0;		// ifindex

	size_t if_needed;
	char* buf = 0;
	while (!buf) {
	    if (sysctl(mib, 6, 0, &if_needed, 0, 0) < 0)
		return false;
	    if ((buf = new char[if_needed]) == 0)
		return false;
	    if (sysctl(mib, 6, buf, &if_needed, 0, 0) < 0) {
		if (errno == ENOMEM) {
		    delete[] buf;
		    buf = 0;
		} else
		    return false;
	    }
	}

	for (char* pos = buf; pos < buf + if_needed; ) {
	    // grab next if_msghdr
	    struct if_msghdr* ifm = reinterpret_cast<struct if_msghdr*>(pos);
	    if (ifm->ifm_type != RTM_IFINFO)
		break;
	    int datalen = sizeof(struct if_data);
#  if HAVE_IF_DATA_IFI_DATALEN
	    if (ifm->ifm_data.ifi_datalen)
		datalen = ifm->ifm_data.ifi_datalen;
#  endif

	    // extract interface name from 'ifm'
	    struct sockaddr_dl* sdl = reinterpret_cast<struct sockaddr_dl*>(pos + sizeof(struct if_msghdr) - sizeof(struct if_data) + datalen);
	    String name(sdl->sdl_data, sdl->sdl_nlen);

	    // Ethernet address is stored in 'sdl'
	    if (sdl->sdl_type == IFT_ETHER && sdl->sdl_alen == 6) {
		device_names.push_back(name);
		device_addrs.push_back(String('e') + String((const char*)(LLADDR(sdl)), 6));
	    }

	    // parse all addresses, looking for IP
	    pos += ifm->ifm_msglen;
	    while (pos < buf + if_needed) {
		struct if_msghdr* nextifm = reinterpret_cast<struct if_msghdr*>(pos);
		if (nextifm->ifm_type != RTM_NEWADDR)
		    break;

		struct ifa_msghdr* ifam = reinterpret_cast<struct ifa_msghdr*>(nextifm);
		char* sa_buf = reinterpret_cast<char*>(ifam + 1);
		pos += nextifm->ifm_msglen;
		for (int i = 0; i < RTAX_MAX && sa_buf < pos; i++) {
		    if (!(ifam->ifam_addrs & (1 << i)))
			continue;
		    struct sockaddr* sa = reinterpret_cast<struct sockaddr*>(sa_buf);
		    if (sa->sa_len)
			sa_buf += 1 + ((sa->sa_len - 1) | (sizeof(long) - 1));
		    else
			sa_buf += sizeof(long);
		    if (i != RTAX_IFA)
			continue;
		    if (sa->sa_family == AF_INET) {
			device_names.push_back(name);
			device_addrs.push_back(String('i') + String((const char *)&((struct sockaddr_in*)sa)->sin_addr, 4));
		    }
		}
	    }
	}

	delete[] buf;
# endif /* defined(__APPLE__) || defined(__FreeBSD__) */
	read_time = time(0);
    }

    for (int i = 0; i < device_names.size(); i++)
	if (device_names[i] == s && device_addrs[i][0] == type) {
	    memcpy(store, device_addrs[i].data() + 1, len);
	    return true;
	}

    return false;
}

#endif /* CLICK_QUERY_NETDEVICE */


bool
AddressInfo::query_ip(String s, unsigned char *store, const Element *e)
{
    int colon = s.find_right(':');
    if (colon >= 0) {
	String typestr = s.substring(colon).lower();
	s = s.substring(0, colon);
	if (typestr.equals(":ip", 3) || typestr.equals(":ip4", 4))
	    /* do nothing */;
	else if (typestr.equals(":bcast", 6)) {
	    // ":bcast" type only supported for NameDB at the moment
	    uint32_t addr[2];
	    if (NameInfo::query(NameInfo::T_IP_PREFIX, e, s, &addr[0], 8)) {
		addr[0] |= ~addr[1];
		memcpy(store, addr, 4);
		return true;
	    } else
		return false;
	} else
	    return false;
    }

    if (NameInfo::query(NameInfo::T_IP_ADDR, e, s, store, 4))
	return true;

    // if it's a device name, return a primary IP address
#if CLICK_LINUXMODULE
# if LINUX_VERSION_CODE >= KERNEL_VERSION(2, 6, 24)
    net_device *dev = dev_get_by_name(&init_net, s.c_str());
# else
    net_device *dev = dev_get_by_name(s.c_str());
# endif
    if (dev) {
	bool found = false;
	in_device *in_dev = in_dev_get(dev);
	if (in_dev) {
	    for_primary_ifa(in_dev) {
		memcpy(store, &ifa->ifa_local, 4);
		found = true;
		break;
	    }
	    endfor_ifa(in_dev);
	    in_dev_put(in_dev);
	}
	dev_put(dev);
	if (found)
	    return true;
    }
#elif CLICK_NS
    if (e) {
	char tmp[255];
	int r = simclick_sim_command(e->router()->master()->simnode(), SIMCLICK_IPADDR_FROM_NAME, s.c_str(), tmp, 255);
	if (r >= 0 && tmp[0] && IPAddressArg().parse(tmp, *reinterpret_cast<IPAddress *>(store)))
	    return true;
    }
#elif CLICK_QUERY_NETDEVICE
    if (query_netdevice(s, store, 'i', 4))
	return true;
#endif

    return false;
}

bool
AddressInfo::query_ip_prefix(String s, unsigned char *store,
			     unsigned char *mask_store, const Element *e)
{
    int colon = s.find_right(':');
    if (colon >= 0) {
	String typestr = s.substring(colon).lower();
	s = s.substring(0, colon);
	if (!(typestr.equals(":net", 4) || typestr.equals(":ipnet", 6)
	      || typestr.equals(":ip4net", 7)))
	    return false;
    }

    uint8_t data[8];
    if (NameInfo::query(NameInfo::T_IP_PREFIX, e, s, &data[0], 8)) {
	memcpy(store, &data[0], 4);
	memcpy(mask_store, &data[4], 4);
	return true;
    }

    return false;
}


#ifdef HAVE_IP6

bool
AddressInfo::query_ip6(String s, unsigned char *store, const Element *e)
{
    int colon = s.find_right(':');
    if (colon >= 0 && s.substring(colon).lower() != ":ip6")
	return false;
    else if (colon >= 0)
	s = s.substring(0, colon);

    return NameInfo::query(NameInfo::T_IP6_ADDR, e, s, store, 16);
}

bool
AddressInfo::query_ip6_prefix(String s, unsigned char *store,
			      int *bits_store, const Element *e)
{
    int colon = s.find_right(':');
    if (colon >= 0 && s.substring(colon).lower() != ":ip6net")
	return false;
    else if (colon >= 0)
	s = s.substring(0, colon);

    struct {
	unsigned char c[16];
	int p;
    } data;
    if (NameInfo::query(NameInfo::T_IP6_PREFIX, e, s, &data, sizeof(data))) {
	memcpy(store, data.c, 16);
	*bits_store = data.p;
	return true;
    }

    return false;
}

#endif /* HAVE_IP6 */


bool
AddressInfo::query_ethernet(String s, unsigned char *store, const Element *e)
{
    int colon = s.find_right(':');
    if (colon >= 0 && s.substring(colon).lower() != ":eth"
	&& s.substring(colon).lower() != ":ethernet")
	return false;
    else if (colon >= 0)
	s = s.substring(0, colon);

    if (NameInfo::query(NameInfo::T_ETHERNET_ADDR, e, s, store, 6))
	return true;

    // if it's a device name, return its Ethernet address
#if CLICK_LINUXMODULE
    // in the Linux kernel, just look at the device list
# if LINUX_VERSION_CODE >= KERNEL_VERSION(2, 6, 24)
    net_device *dev = dev_get_by_name(&init_net, s.c_str());
# else
    net_device *dev = dev_get_by_name(s.c_str());
# endif
    if (dev && (dev->type == ARPHRD_ETHER || dev->type == ARPHRD_80211)) {
	memcpy(store, dev->dev_addr, 6);
	dev_put(dev);
	return true;
    } else if (dev)
	dev_put(dev);
#elif CLICK_NS
    if (e) {
	char tmp[255];
	int r = simclick_sim_command(e->router()->master()->simnode(), SIMCLICK_MACADDR_FROM_NAME, s.c_str(), tmp, 255);
	if (r >= 0 && tmp[0] && EtherAddressArg().parse(tmp, store))
	    return true;
    }
#elif CLICK_QUERY_NETDEVICE
    if (query_netdevice(s, store, 'e', 6))
	return true;
#endif

    return false;
}

CLICK_ENDDECLS
EXPORT_ELEMENT(AddressInfo)
ELEMENT_HEADER(<click/standard/addressinfo.hh>)
